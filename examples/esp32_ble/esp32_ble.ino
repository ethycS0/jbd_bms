#include <BLEAddress.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEScan.h>
#include <BLEUtils.h>

#include <jbd_bms.h>

BLEClient *p_client;
BLERemoteService *p_remote_service;
BLERemoteCharacteristic *p_char_write;
BLERemoteCharacteristic *p_char_notify;

BLEAddress *p_server_address = nullptr;
bool do_connect = false;
bool connected = false;

// Notification buffer
std::vector<uint8_t> response_buffer;
int notification_count = 0;
unsigned long last_notify_time = 0;

// Dummy Stream to satisfy BMS constructor — we won't use it
HardwareSerial dummy_serial(0);
BMS bms(dummy_serial);

// Called for each advertised BLE device
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) override {
                if (advertisedDevice.haveServiceUUID() &&
                    advertisedDevice.getServiceUUID().equals(BLEUUID("0000ff00-0000-1000-8000-00805f9b34fb"))) {
                        Serial.print("Found BMS: ");
                        Serial.println(advertisedDevice.toString().c_str());
                        p_server_address = new BLEAddress(advertisedDevice.getAddress());
                        do_connect = true;
                        BLEDevice::getScan()->stop();
                }
        }
};

// Notification callback
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length,
                           bool isNotify) {
        Serial.printf("[Notify] Length: %d\n", length);
        for (size_t i = 0; i < length; ++i) {
                Serial.printf("%02X ", pData[i]);
                response_buffer.push_back(pData[i]);
        }
        Serial.println();

        notification_count++;
        last_notify_time = millis();

        // Assume 3 notifications complete a full BMS frame
        if (notification_count >= 3) {
                Serial.println("=== Full BMS Response Reconstructed ===");

                if (bms.process_buffer(response_buffer.data(), response_buffer.size())) {
                        Serial.println("✔ Valid BMS frame");

                        if (bms.get_last_response_type() == BASIC_INFO) {
                                auto info = bms.get_basic_info();
                                Serial.printf("Voltage: %.2f V\n", info.total_voltage / 100.0);
                                Serial.printf("Current: %.2f A\n", info.current_usage / 100.0);
                                Serial.printf("SOC: %d%%\n", info.battery_percentage);
                        }

                } else {
                        Serial.println("✖ Invalid BMS frame or checksum mismatch");
                }

                response_buffer.clear();
                notification_count = 0;
        }
}

void setup() {
        Serial.begin(115200);
        BLEDevice::init("");

        BLEScan *p_ble_scan = BLEDevice::getScan();
        p_ble_scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        p_ble_scan->setActiveScan(true);
        p_ble_scan->start(5);
}

void loop() {
        if (do_connect && !connected) {
                p_client = BLEDevice::createClient();
                if (p_client->connect(*p_server_address)) {
                        Serial.println("Connected to BMS");

                        p_remote_service = p_client->getService("0000ff00-0000-1000-8000-00805f9b34fb");
                        if (!p_remote_service) {
                                Serial.println("❌ Service not found.");
                                return;
                        }

                        p_char_write = p_remote_service->getCharacteristic("0000ff02-0000-1000-8000-00805f9b34fb");
                        p_char_notify = p_remote_service->getCharacteristic("0000ff01-0000-1000-8000-00805f9b34fb");

                        if (!p_char_write || !p_char_notify) {
                                Serial.println("❌ Characteristics not found.");
                                return;
                        }

                        if (p_char_notify->canNotify()) {
                                p_char_notify->registerForNotify(notifyCallback);
                                auto *p2902 = p_char_notify->getDescriptor(BLEUUID((uint16_t)0x2902));
                                if (p2902) {
                                        uint8_t notifyOn[] = {0x01, 0x00};
                                        p2902->writeValue(notifyOn, 2, true);
                                        Serial.println("🔔 Notifications enabled");
                                }
                        }

                        delay(300); // give time before sending command

                        // Send request for BASIC_INFO
                        p_char_write->writeValue((uint8_t *)request_basic_info, sizeof(request_basic_info));
                        Serial.println("📤 Sent request_basic_info");

                        last_notify_time = millis();
                        connected = true;
                } else {
                        Serial.println("❌ Failed to connect.");
                        do_connect = false;
                }
        }

        // Timeout check
        if (connected && notification_count == 0 && millis() - last_notify_time > 5000) {
                Serial.println("⚠️ Timeout: No response from BMS");
                last_notify_time = millis();
        }
}
