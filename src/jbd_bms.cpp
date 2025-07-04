#include "jbd_bms.h"

uint16_t swap16(uint16_t val) { return (val >> 8) | (val << 8); }
int16_t swap16s(int16_t val) {
        uint16_t u = (uint16_t)val;
        return (int16_t)((u >> 8) | (u << 8));
}
uint32_t swap32(uint32_t val) { return __builtin_bswap32(val); }

uint16_t BMS::calculate_checksum(const uint8_t *buffer, size_t len) {
        uint16_t sum = buffer[2] + buffer[3];
        for (size_t i = 0; i < len; ++i)
                sum += buffer[4 + i];
        return (uint16_t)(~sum + 1);
}

basic_info_data BMS::parse_basic_info(const uint8_t *buffer) {
        basic_info_data info;
        memcpy(&info, &buffer[4], sizeof(basic_info_data));
        info.total_voltage = swap16(info.total_voltage);
        info.current_usage = swap16s(info.current_usage);
        info.remaining_capacity = swap16(info.remaining_capacity);
        info.nominal_capacity = swap16(info.nominal_capacity);
        info.cycles = swap16(info.cycles);
        info.production_date = swap16(info.production_date);
        info.equilibrium = swap32(info.equilibrium);
        info.protection_status = swap16(info.protection_status);
        for (int i = 0; i < info.ntc_count; ++i)
                info.ntc_content[i] = swap16(info.ntc_content[i]);
        info.checksum = swap16(info.checksum);
        return info;
}

cell_voltage_data BMS::parse_cell_voltages(const uint8_t *buffer) {
        cell_voltage_data info;
        int cell_count = data_len / 2;
        for (uint8_t i = 0; i < cell_count; ++i)
                info.cell_voltage[i] = swap16(info.cell_voltage[i]);
        info.checksum = swap16(info.checksum);
        return info;
}

bool BMS::process_buffer(const uint8_t *buffer, size_t len) {
        if (len < 7 || buffer[0] != 0xDD || buffer[len - 1] != 0x77)
                return false;

        uint8_t req_type = buffer[1];
        uint8_t status = buffer[2];
        uint8_t data_len = buffer[3];

        if (status != 0x00 || len != (size_t)(4 + data_len + 2 + 1))
                return false;

        uint16_t calculated_checksum = calculate_checksum(buffer, data_len);

        switch (req_type) {
        case BASIC_INFO:
                response_type = BASIC_INFO;
                basic_info = parse_basic_info(buffer);
                return basic_info.checksum == calculated_checksum;

        case CELL_VOLTAGE_INFO:
                response_type = CELL_VOLTAGE_INFO;
                cell_info = parse_cell_voltages(buffer);
                return cell_info.checksum == calculated_checksum;

        default:
                response_type = BMS_UNKNOWN;
                return false;
        }
}

bool BMS::process_response() {
        uint8_t buffer[256] = {0};
        size_t index = 0;
        unsigned long start = millis();

        while ((millis() - start) < BMS_TIMEOUT && index < sizeof(buffer)) {
                if (port.available()) {
                        buffer[index++] = port.read();
                }
        }

        return process_buffer(buffer, index);
}
