#include "bms_data.hpp"
#include <HardwareSerial.h>

HardwareSerial BMS(2);
#define RXD2 16
#define TXD2 17

void setup() {
        Serial.begin(115200);
        BMS.begin(9600, SERIAL_8N1, RXD2, TXD2);
        delay(1000);
}

void loop() {
        static uint8_t buffer[256];
        uint16_t calculated_checksum = 0;
        bool read = false;

        // Choose Message to send
        send_request(BMS, request_basic_info, sizeof(request_basic_info));
        // send_request(BMS, request_cell_voltages, sizeof(request_cell_voltages));

        read = validate_buffer(BMS, buffer, sizeof(buffer), type);
        if (read == true) {
                Serial.print("Received: ");
                for (size_t i = 0; i < 4 + data_len + 3; ++i) {
                        Serial.printf("%02X ", buffer[i]);
                }
                Serial.println();

                calculated_checksum = calculate_bms_checksum(buffer, data_len);

                switch (type) {
                case BASIC_INFO: {
                        basic_info_data *basic_info = (basic_info_data *)&buffer[4];
                        process_basic_info(*basic_info);
                        Serial.printf("Calculated Checksum: 0x%04X | Received: 0x%04X\n",
                                      calculated_checksum, basic_info->checksum);
                        display_basic_info(*basic_info);
                        break;
                }
                case CELL_VOLTAGE_INFO: {
                        cell_voltage_data *cell_info = (cell_voltage_data *)&buffer[4];
                        process_cell_voltage(*cell_info, data_len / 2);
                        Serial.printf("Calculated Checksum: 0x%04X | Received: 0x%04X\n",
                                      calculated_checksum, cell_info->checksum);
                        display_cell_voltage(*cell_info, data_len / 2);
                        break;
                }
                case BMS_UNKNOWN:
                        // Handle case
                        break;
                }
        }
        delay(5000);
}
