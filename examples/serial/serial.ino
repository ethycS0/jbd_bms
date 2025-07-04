#include <jbd_bms.h>

BMS bms(Serial);

void setup() {
    Serial.begin(9600);
    delay(1000);
    bms.send_request(request_basic_info, sizeof(request_basic_info));
}

void loop() {
    if (bms.process_response()) {
        if (bms.get_last_response_type() == BASIC_INFO) {
            basic_info_data info = bms.get_basic_info();
            Serial.print("Total Voltage: ");
            Serial.println(info.total_voltage);
        }
    }
    delay(5000);
}

