#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String, Float32MultiArray
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

    if len(buffer) < 7 or buffer[0] != 0xDD or buffer[-1] != 0x77:
        return None

    data_len = buffer[3]
    expected_len = 4 + data_len + 2 + 1
    if len(buffer) != expected_len:
        return None

    if buffer[2] != 0x00:
        return None

    return buffer


def parse_basic_info(data):
    fields = struct.unpack(">HhHHHHIHB4B3H9BH", data)
    info = {
        "total_voltage": fields[0] / 100.0,
        "current_usage": fields[1] / 100.0,
        "remaining_capacity": fields[2] * 10,
        "nominal_capacity": fields[3] * 10,
        "cycles": fields[4],
        "production_date": fields[5],
        "equilibrium": fields[6],
        "protection_status": fields[7],
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
    return {"cell_voltages": cell_voltages, "checksum": checksum}


class BMSPublisher(Node):
    def __init__(self):
        super().__init__("bms_reader")
        self.publisher_basic = self.create_publisher(String, "bms/basic_info", 10)
        self.publisher_cells = self.create_publisher(
            Float32MultiArray, "bms/cell_voltages", 10
        )

        self.ser = serial.Serial(PORT, BAUDRATE, timeout=TIMEOUT)
        time.sleep(0.2)

        self.timer = self.create_timer(5.0, self.timer_callback)

    def timer_callback(self):
        try:
            self.ser.write(request_basic_info)
            time.sleep(0.2)
            frame = read_frame(self.ser)
            if frame and frame[1] == 0x03:
                payload = frame[4:-1]
                info = parse_basic_info(payload)
                msg = String()
                msg.data = str(info)
                self.publisher_basic.publish(msg)

            self.ser.write(request_cell_voltages)
            time.sleep(0.2)
            frame = read_frame(self.ser)
            if frame and frame[1] == 0x04:
                payload = frame[4:-1]
                info = parse_cell_voltages(payload)
                msg = Float32MultiArray()
                msg.data = info["cell_voltages"]
                self.publisher_cells.publish(msg)

        except Exception as e:
            self.get_logger().error(f"Error reading BMS: {e}")


def main(args=None):
    rclpy.init(args=args)
    node = BMSPublisher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
