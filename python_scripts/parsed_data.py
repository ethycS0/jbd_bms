import serial
import struct
import time

PORT = "/dev/ttyACM0"
BAUDRATE = 9600
TIMEOUT = 1

request_basic_info = bytes([0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77])
request_cell_voltages = bytes([0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77])

NO_CELLS = 6
NO_NTC = 3


def calculate_bms_checksum(buffer):
    sum_ = sum(buffer[2 : 4 + buffer[3]])
    return (~sum_ + 1) & 0xFFFF


def read_frame(ser):
    buffer = bytearray()
    start_time = time.time()
    while time.time() - start_time < 1:
        if ser.in_waiting:
            buffer += ser.read(ser.in_waiting)
            if buffer and buffer[-1] == 0x77:
                break
        time.sleep(0.01)

    if len(buffer) < 7:
        print("✖ Incomplete frame")
        return None

    if buffer[0] != 0xDD:
        print("✖ Invalid start byte")
        return None

    if buffer[-1] != 0x77:
        print("✖ Invalid end byte")
        return None

    data_len = buffer[3]
    expected_len = 4 + data_len + 2 + 1
    if len(buffer) != expected_len:
        print(f"✖ Length mismatch: got {len(buffer)}, expected {expected_len}")
        return None

    if buffer[2] != 0x00:
        print(f"✖ Status error: 0x{buffer[2]:02X}")
        return None

    return buffer


def parse_basic_info(data):
    fields = struct.unpack(">HhHHHHIHB4B3H9BH", data)
    info = {
        "total_voltage": (fields[0]) / 100.0,
        "current_usage": (fields[1]) / 100.0,
        "remaining_capacity": (fields[2]) * 10,
        "nominal_capacity": (fields[3]) * 10,
        "cycles": (fields[4]),
        "production_date": (fields[5]),
        "equilibrium": (fields[6]),
        "protection_status": (fields[7]),
        "software_version": fields[8],
        "battery_percentage": fields[9],
        "mos_status": fields[10],
        "battery_strings": fields[11],
        "ntc_count": fields[12],
        "ntc_content": [(v) for v in fields[13 : 13 + NO_NTC]],
        "extra_bytes": fields[13 + NO_NTC : 13 + NO_NTC + 9],
        "checksum": fields[-1],
    }

    pd = info["production_date"]
    info["production_date_str"] = (
        f"{2000 + (pd >> 9)}-{(pd >> 5) & 0x0F:02d}-{pd & 0x1F:02d}"
    )

    return info


def parse_cell_voltages(data):
    fields = struct.unpack(">6H H", data)
    voltages_raw = fields[:NO_CELLS]
    checksum = fields[-1]

    cell_voltages = [v / 1000.0 for v in voltages_raw]

    return {
        "cell_voltages": cell_voltages,
        "checksum": checksum,
    }


def display_cell_voltages(info):
    for i, v in enumerate(info["cell_voltages"]):
        print(f"Cell {i+1}: {v:.3f} V")


def display_basic_info(info):
    print(f"Voltage: {info['total_voltage']:.2f} V")
    print(f"Current: {info['current_usage']:.2f} A")
    print(f"Remaining Capacity: {info['remaining_capacity']} mAh")
    print(f"Nominal Capacity: {info['nominal_capacity']} mAh")
    print(f"Cycles: {info['cycles']}")
    print(f"Production Date: {info['production_date_str']}")
    print(f"Equilibrium Bits: 0x{info['equilibrium']:08X}")
    print(f"Protection Status: 0x{info['protection_status']:04X}")
    print(f"Software Version: {info['software_version']}")
    print(f"Battery Percentage: {info['battery_percentage']}%")
    print(f"MOS Status: 0x{info['mos_status']:02X}")
    print(f"Battery Strings: {info['battery_strings']}")
    print(f"NTC Count: {info['ntc_count']}")
    for i, temp in enumerate(info["ntc_content"]):
        tempC = (temp - 2731) / 10.0
        print(f"NTC {i + 1}: {tempC:.1f} °C")
    print(f"Checksum: 0x{info['checksum']:04X}")


def main():
    with serial.Serial(PORT, BAUDRATE, timeout=TIMEOUT) as ser:
        time.sleep(0.2)
        while True:
            # Comment and Uncomment Appropriate packet

            ser.write(request_basic_info)
            print(f"Sent: {request_basic_info.hex().upper()}")

            # ser.write(request_cell_voltages)
            # print(f"Sent: {request_cell_voltages.hex().upper()}")

            time.sleep(0.2)
            frame = read_frame(ser)
            if not frame:
                continue

            print("Received:", " ".join(f"{b:02X}" for b in frame))
            payload = frame[4:-1]
            checksum_received = (frame[-3] << 8) | frame[-2]
            checksum_calc = calculate_bms_checksum(frame)

            print(
                f"Calculated Checksum: 0x{checksum_calc:04X} | Received: 0x{checksum_received:04X}"
            )

            if frame[1] == 0x03:
                info = parse_basic_info(payload)
                display_basic_info(info)
            elif frame[1] == 0x04:
                info = parse_cell_voltages(payload)
                display_cell_voltages(info)
            else:
                print("Unsupported frame type.")

            time.sleep(5)


if __name__ == "__main__":
    main()
