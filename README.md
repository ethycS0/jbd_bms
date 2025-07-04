# JBD BMS Toolkit for Arduino / ESP32 / PC

An Arduino library and set of Python tools for communicating with **Jiabaida BMS** (JBD) Smart Battery Management Systems (BMS) via **UART** and **BLE**.

This toolkit provides everything you need to read and parse data from your JBD BMS on various platforms, including Arduino, ESP32, and any PC running Python. It implements the JBD protocol V4, allowing you to easily access battery voltage, current, capacity, individual cell voltages, temperature, protection status, etc.

---

## Features

- **Cross-Platform:** Works with Arduino (AVR), ESP32 (UART/BLE), and Python on PC.
- **Structured Library:** Packaged as a standard Arduino library with easy-to-use examples.
- **Core Functions:** Read and deserialize essential BMS data:
  - Basic System Info (`0x03`)
  - Individual Cell Voltages (`0x04`)
- **Checksum Validation:** Ensures data integrity for all incoming messages.
- **ROS 2 Integration:** Includes a Python script to publish BMS data directly to ROS 2 topics.

---

## Project Structure

The repository is now organized as a standard Arduino library.

```
.
├── examples                        # Examples for ESP32/Arduino
│   ├── esp32_ble
│   │   └── esp32_ble.ino           # BLE Communication for ESP32
│   └── serial
│       └── serial.ino              # Serial Communication for ESP32/Arduino
├── library.properties
├── protocol_datasheet_v4.pdf       # V4 Documentation
├── python_scripts                  # Python Scripts for PC
│   ├── packet_test.py
│   ├── parsed_data.py
│   └── ros2_publish_data.py
├── README.md
└── src                             # Library files
    ├── jbd_bms.cpp
    └── jbd_bms.h
```

---

## Getting Started

### 1\. Arduino / ESP32 Installation

This project is designed to be used as an Arduino library.

1.  Click the **Code** button on this repository's page and select **Download ZIP**.
2.  Open the Arduino IDE.
3.  Navigate to **Sketch \> Include Library \> Add .ZIP Library...**
4.  Select the downloaded ZIP file.
5.  The library and its examples will now be available in your Arduino IDE.

### 2\. Python Requirements

To use the Python scripts, first install the necessary dependencies.

```bash
# Navigate to the python_scripts directory
cd python_scripts

# Install required packages
pip install pyserial
# For ROS 2 usage, also install:
pip install rclpy
```

---

## Usage

### Arduino & ESP32 (Library Examples)

Once the library is installed, you can access the examples directly from the Arduino IDE.

1.  Go to **File \> Examples \> JBD BMS Toolkit**.

2.  Choose one of the following examples:

    - **avr:** A basic UART example for boards like the Arduino Uno.
    - **esp32:** A UART example for ESP32.
    - **esp32_ble:** Demonstrates connecting to the BMS over BLE on an ESP32.

3.  Make sure to update the serial port or BLE address in the sketch to match your setup.

4.  Upload and open the Serial Monitor to see the BMS data.

### Python Scripts (for PC)

The Python scripts are located in the `python_scripts/` directory.

> ⚠️ **Important:** Before running, make sure to set the correct serial port (e.g., `/dev/ttyUSB0`, `COM3`) in the script you are using.

#### `packet_test.py`

A simple script to test UART connectivity. It sends a pre-defined command and prints the raw response from the BMS.

```bash
python3 python_scripts/packet_test.py
```

#### `parsed_data.py`

Sends a request, validates the response, and prints the fully parsed and human-readable BMS data.

```bash
python3 python_scripts/parsed_data.py
```

#### `ros2_publish_data.py`

Functions as a ROS 2 node that reads data from the BMS and publishes it to topics.

```bash
# Run the publisher node
python3 python_scripts/ros2_publish_data.py

# In another terminal, listen to the topics
ros2 topic echo /bms/basic_info
ros2 topic echo /bms/cell_voltages
```

- `/bms/basic_info` is published as a `std_msgs/String`.
- `/bms/cell_voltages` is published as a `std_msgs/Float32MultiArray`.

---

Sure! Here's a refined **Usage** section for your `README` written in the format you specified, tailored to your `BMS` class and how it works over both **Serial** and **BLE**:

---

## Arduino Usage

### Initialization

Include the header and initialize the `BMS` class with a `Stream` object:

```cpp
#include "jbd_bms.h"

HardwareSerial serial(2);  // or use SoftwareSerial if needed
BMS bms(serial);

void setup() {
  serial.begin(9600);
}
```

---

### Reading Basic Info from BMS (Serial)

```cpp
bms.send_request(request_basic_info, sizeof(request_basic_info));
if (bms.process_response()) {
  // Use get-functions below
}
```

After calling `send_request()` and `process_response()`, the data is parsed. If successful, `process_response()` returns `true`.

---

### Reading Basic Info from BMS (BLE)

In your BLE notification callback, once the full response is reconstructed:

```cpp
if (bms.process_buffer(buffer, length)) {
  // Use get-functions below
}
```

This lets you use the same `BMS` class to parse BLE responses.

---

### Reading Cell Voltages

```cpp
bms.send_request(request_cell_voltages, sizeof(request_cell_voltages));
if (bms.process_response()) {
  // Use get-function below
}
```

---

### Getting Information

We can get the type of data received as follows:

```cpp
bms.get_last_response_type();

```

This returns enum `request_type` value.

```cpp
enum bms_request_type { BMS_UNKNOWN = 0, BASIC_INFO = 3, CELL_VOLTAGE_INFO = 4 };
```

Based on return value, you can now get data as such:

```cpp
basic_info_data info = bms.get_basic_info();
int current = info.current;

cell_voltage_data info = bms.get_cell_voltages();
uint16_t cell1 = bms.cell_voltage[0];
```

2 Message types have been implemented so `basic_info_data` and `cell_voltage_data` are returned. Protocol section explains what data can be takes from these structs.

## Protocol Implementation

This toolkit implements the **JBD Smart BMS UART/BLE protocol V4**. Communication is based on a command-response format: `[Start][Command][Status][Length][Data...][Checksum][End]`.

### Supported Commands

#### **Basic Info (`0x03`):** Fetches overall status, including voltage, current, capacity, temperatures, and protection flags.

##### Example Output

```
Voltage: 22.62 V
Current: -1.25 A
Remaining Capacity: 100400 mAh
Nominal Capacity: 150000 mAh
Cycles: 205
Production Date: 2023-06-15
Equilibrium Bits: 0x00000000
Protection Status: 0x0000
Software Version: 3
Battery Percentage: 74%
MOS Status: 0x03
Battery Strings: 13
NTC Count: 3
NTC 1: 24.5 °C
NTC 2: 24.3 °C
NTC 3: 24.6 °C
Checksum: 0x3D4A
```

---

#### **Cell Voltages (`0x04`):** Fetches the real-time voltage of each individual cell.

##### Example Output

```
Cell Count: 6
Cell  1 Voltage: 3.821 V (Raw: 0x0EED)
Cell  2 Voltage: 3.823 V (Raw: 0x0EEF)
Cell  3 Voltage: 3.820 V (Raw: 0x0EEC)
Cell  4 Voltage: 3.819 V (Raw: 0x0EEB)
Cell  5 Voltage: 3.824 V (Raw: 0x0EF0)
Cell  6 Voltage: 3.822 V (Raw: 0x0EEE)
Checksum: 0xC9FA
```

---

### Future Support

The following commands are planned for future updates:

- Hardware version (`0x05`)
- MOSFET control commands
