#ifndef BMS_DATA
#define BMS_DATA

#include <Arduino.h>

#define BMS_TIMEOUT 1000
#define NO_CELLS 6
#define NO_NTC 3
#define EXTRA_BYTES 9

uint8_t data_len;

enum bms_request_type { BMS_UNKNOWN = 0, BASIC_INFO = 3, CELL_VOLTAGE_INFO = 4 };
bms_request_type type;

const uint8_t request_basic_info[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t request_cell_voltages[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};

uint16_t swap16(uint16_t val) { return (val >> 8) | (val << 8); }
int16_t swap16s(int16_t val) {
        uint16_t u = (uint16_t)val;
        return (int16_t)((u >> 8) | (u << 8));
}
uint32_t swap32(uint32_t val) { return __builtin_bswap32(val); }

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

void process_basic_info(basic_info_data &info) {
        info.total_voltage = swap16(info.total_voltage);
        info.current_usage = swap16s(info.current_usage);
        info.remaining_capacity = swap16(info.remaining_capacity);
        info.nominal_capacity = swap16(info.nominal_capacity);
        info.cycles = swap16(info.cycles);
        info.production_date = swap16(info.production_date);
        info.equilibrium = swap32(info.equilibrium);
        info.protection_status = swap16(info.protection_status);
        for (int i = 0; i < info.ntc_count; i++) {
                info.ntc_content[i] = swap16(info.ntc_content[i]);
        }
        info.checksum = swap16(info.checksum);
}

void display_basic_info(basic_info_data &info) {
        char buf[1024];

        sprintf(buf, "Voltage: %.2f V", info.total_voltage / 100.0);
        Serial.println(buf);

        sprintf(buf, "Current: %.2f A", info.current_usage / 100.0);
        Serial.println(buf);

        sprintf(buf, "Remaining Capacity: %u mAh", info.remaining_capacity * 10);
        Serial.println(buf);

        sprintf(buf, "Nominal Capacity: %u mAh", info.nominal_capacity * 10);
        Serial.println(buf);

        sprintf(buf, "Cycles: %u", info.cycles);
        Serial.println(buf);

        uint16_t pd = info.production_date;
        sprintf(buf, "Production Date: %u-%02u-%02u", 2000 + (pd >> 9), (pd >> 5) & 0x0F,
                pd & 0x1F);
        Serial.println(buf);

        sprintf(buf, "Equilibrium Bits: 0x%08lX", info.equilibrium);
        Serial.println(buf);

        sprintf(buf, "Protection Status: 0x%04X", info.protection_status);
        Serial.println(buf);

        sprintf(buf, "Software Version: %u", info.software_version);
        Serial.println(buf);

        sprintf(buf, "Battery Percentage: %u%%", info.battery_percentage);
        Serial.println(buf);

        sprintf(buf, "MOS Status: 0x%02X", info.mos_status);
        Serial.println(buf);

        sprintf(buf, "Battery Strings: %u", info.battery_strings);
        Serial.println(buf);

        sprintf(buf, "NTC Count: %u", info.ntc_count);
        Serial.println(buf);

        for (int i = 0; i < info.ntc_count && i < NO_NTC; i++) {
                float tempC = (info.ntc_content[i] - 2731) / 10.0;
                sprintf(buf, "NTC %d: %.1f °C", i + 1, tempC);
                Serial.println(buf);
        }

        sprintf(buf, "Checksum: 0x%04X", info.checksum);
        Serial.println(buf);
}

void process_cell_voltage(cell_voltage_data &data, uint8_t cell_count) {
        for (uint8_t i = 0; i < cell_count; ++i) {
                data.cell_voltage[i] = (data.cell_voltage[i] >> 8) | (data.cell_voltage[i] << 8);
        }
        data.checksum = swap16(data.checksum);
}

void display_cell_voltage(cell_voltage_data &data, uint8_t cell_count) {
        char buf[64];
        sprintf(buf, "Cell Count: %u", cell_count);
        Serial.println(buf);

        for (uint8_t i = 0; i < cell_count; ++i) {
                float voltage = data.cell_voltage[i] / 1000.0;
                sprintf(buf, "Cell %2u Voltage: %.3f V (Raw: 0x%04X)", i + 1, voltage,
                        data.cell_voltage[i]);
                Serial.println(buf);
        }

        sprintf(buf, "Checksum: 0x%04X", data.checksum);
        Serial.println(buf);
}

void send_request(Stream &port, const uint8_t *request, size_t len) {
        port.write(request, len);
        Serial.print("Sent request: ");
        for (size_t i = 0; i < len; ++i) {
                char buf[8];
                sprintf(buf, "%02X ", request[i]);
                Serial.print(buf);
        }
        Serial.println();
}

bool validate_buffer(Stream &port, uint8_t *buffer, size_t max_len, bms_request_type &type_out) {
        size_t index = 0;
        unsigned long start_time = millis();

        while ((millis() - start_time) < BMS_TIMEOUT && index < max_len) {
                if (port.available()) {
                        buffer[index++] = port.read();
                }
        }

        if (index < 7) {
                Serial.println("✖ Incomplete frame");
                return false;
        }

        if (buffer[0] != 0xDD) {
                Serial.println("✖ Invalid start byte");
                return false;
        }

        if (buffer[index - 1] != 0x77) {
                Serial.println("✖ Invalid end byte");
                return false;
        }

        uint8_t request_type = buffer[1];
        uint8_t status = buffer[2];
        data_len = buffer[3];

        size_t expected_frame_size = 4 + data_len + 2 + 1;
        if (index != expected_frame_size) {
                Serial.print("✖ Length mismatch: got ");
                Serial.print(index);
                Serial.print(", expected ");
                Serial.println(expected_frame_size);
                return false;
        }

        if (status != 0x00) {
                Serial.print("✖ Status error: 0x");
                Serial.println(status, HEX);
                return false;
        }

        switch (request_type) {
        case 0x03:
                type_out = BASIC_INFO;
                break;
        case 0x04:
                type_out = CELL_VOLTAGE_INFO;
                break;
        default:
                type_out = BMS_UNKNOWN;
                return false;
        }

        Serial.print("✔ Valid frame: type 0x");
        Serial.print(request_type, HEX);
        Serial.print(" (");
        Serial.print(type_out == BASIC_INFO          ? "BASIC_INFO"
                     : type_out == CELL_VOLTAGE_INFO ? "CELL_VOLTAGE_INFO"
                                                     : "UNKNOWN");
        Serial.print("), length ");
        Serial.println(index);

        return true;
}

uint16_t calculate_bms_checksum(const uint8_t *buffer, size_t data_len) {
        uint16_t sum = 0;
        sum += buffer[2];
        sum += buffer[3];
        for (size_t i = 0; i < data_len; ++i) {
                sum += buffer[4 + i];
        }
        return (uint16_t)(~sum + 1);
}

#endif // BMS_DATA
