#include "bms_data.hpp"
#include <SoftwareSerial.h>

#define RX_PIN 2
#define TX_PIN 3

SoftwareSerial BMS(RX_PIN, TX_PIN); // RX, TX

void setup() {
        Serial.begin(115200);
        BMS.begin(9600);
        pinMode(RX_PIN, INPUT_PULLUP);
        delay(1000);
}

void loop() {
        static uint8_t buffer[256];
        uint16_t calculated_checksum = 0;
        bool read = false;

        send_request(BMS, request_basic_info, sizeof(request_basic_info));
        // send_request(BMS, request_cell_voltages,
        // sizeof(request_cell_voltages));
        read = validate_buffer(BMS, buffer, sizeof(buffer), type);

        if (read == true) {
                Serial.print("Received: ");
                for (size_t i = 0; i < 4 + data_len + 3; ++i) {
                        char hexBuf[6];
                        sprintf(hexBuf, "%02X ", buffer[i]);
                        Serial.print(hexBuf);
                }
                Serial.println();

                calculated_checksum = calculate_bms_checksum(buffer, data_len);

                switch (type) {
                case BASIC_INFO: {
                        basic_info_data *basic_info = (basic_info_data *)&buffer[4];
                        process_basic_info(*basic_info);
                        char buff[64];
                        sprintf(buff, "Calculated Checksum: 0x%04X | Received: 0x%04X\n",
                                calculated_checksum, basic_info->checksum);

                        Serial.println(buff);
                        display_basic_info(*basic_info);
                        break;
                }
                case CELL_VOLTAGE_INFO: {
                        cell_voltage_data *cell_info = (cell_voltage_data *)&buffer[4];
                        process_cell_voltage(*cell_info, data_len / 2);
                        char buff[64];
                        sprintf(buff, "Calculated Checksum: 0x%04X | Received: 0x%04X\n",
                                calculated_checksum, cell_info->checksum);
                        Serial.println(buff);
                        display_cell_voltage(*cell_info, data_len / 2);
                        break;
                }
                case BMS_UNKNOWN:
                        break;
                }
        }

        delay(5000);
}
