#ifndef BMS_H
#define BMS_H

#include <Arduino.h>

#define NO_CELLS 6
#define NO_NTC 3
#define EXTRA_BYTES 9
#define BMS_TIMEOUT 1000
#define BUFFER_SIZE 256

enum bms_request_type { BMS_UNKNOWN = 0, BASIC_INFO = 3, CELL_VOLTAGE_INFO = 4 };

constexpr uint8_t request_basic_info[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
constexpr uint8_t request_cell_voltages[7] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};

#pragma pack(push, 1)
struct basic_info_data {
        uint16_t total_voltage;
        int16_t current_usage;
        uint16_t remaining_capacity;
        uint16_t nominal_capacity;
        uint16_t cycles;
        uint16_t production_date;
        uint32_t equilibrium;
        uint16_t protection_status;
        uint8_t software_version;
        uint8_t battery_percentage;
        uint8_t mos_status;
        uint8_t battery_strings;
        uint8_t ntc_count;
        uint16_t ntc_content[NO_NTC];
        uint8_t extra_bytes[EXTRA_BYTES];
        uint16_t checksum;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct cell_voltage_data {
        uint16_t cell_voltage[NO_CELLS];
        uint16_t checksum;
};
#pragma pack(pop)

class BMS {
      public:
        BMS(Stream &serial) : port(serial) {}

        void send_request(const uint8_t *request, size_t len) { port.write(request, len); }

        bool process_response();
        bool process_buffer(const uint8_t *buffer, size_t len);

        basic_info_data get_basic_info() const { return basic_info; }
        cell_voltage_data get_cell_voltages() const { return cell_info; }
        bms_request_type get_last_response_type() const { return response_type; }

      private:
        Stream &port;
        basic_info_data basic_info;
        cell_voltage_data cell_info;
        uint8_t data_len = 0;
        bms_request_type response_type = BMS_UNKNOWN;

        uint16_t calculate_checksum(const uint8_t *buffer, size_t len);
        basic_info_data parse_basic_info(const uint8_t *buffer);
        cell_voltage_data parse_cell_voltages(const uint8_t *buffer);
};

#endif // BMS_H
