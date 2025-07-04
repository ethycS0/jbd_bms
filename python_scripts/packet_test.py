import serial

PORT = "/dev/ttyACM0"
BAUD = 9600

# Uncomment the Message Type to test

# Cell Voltages
# data = bytes([0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77])

# Basic Info
data = bytes([0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77])

# Hardware Version
# data = bytes([0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77])

with serial.Serial(PORT, BAUD, timeout=3) as ser:
    ser.write(data)
    print("Sent:", data.hex())

    response = ser.read(1000)
    print("Received:", response.hex(" "))
