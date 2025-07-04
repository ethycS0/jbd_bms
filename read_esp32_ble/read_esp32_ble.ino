#include <BLEAddress.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

BLEClient *pClient;
BLERemoteService *pRemoteService;
BLERemoteCharacteristic *pRemoteCharacteristicWrite;
BLERemoteCharacteristic *pRemoteCharacteristicRead;

static BLEAddress *pServerAddress = nullptr;
bool doConnect = false;
bool connected = false;

std::vector<uint8_t> responseBuffer;
int notificationCount = 0;
unsigned long lastNotifyTime = 0;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) {
                if (advertisedDevice.haveServiceUUID() &&
                    advertisedDevice.getServiceUUID().equals(
                        BLEUUID("0000ff00-0000-1000-8000-00805f9b34fb"))) {

                        Serial.print("Found BMS: ");
                        if (advertisedDevice.haveName()) {
                                Serial.print(advertisedDevice.getName().c_str());
                        }
                        Serial.print(" [");
                        Serial.print(advertisedDevice.getAddress().toString().c_str());
                        Serial.println("]");

                        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
                        doConnect = true;
                        BLEDevice::getScan()->stop();
                }
        }
};

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData,
                           size_t length, bool isNotify) {

        Serial.printf("[DEBUG] Notify callback triggered: length = %d\n", length);
        Serial.print("Notification received: ");
        for (size_t i = 0; i < length; i++) {
                Serial.printf("%02X ", pData[i]);
                responseBuffer.push_back(pData[i]);
        }
        Serial.println();

        notificationCount++;
        lastNotifyTime = millis();

        if (notificationCount >= 3) {
                Serial.println("=== Full Response Reconstructed ===");
                for (size_t i = 0; i < responseBuffer.size(); i++) {
                        Serial.printf("%02X ", responseBuffer[i]);
                }
                Serial.println("\n==================================");

                responseBuffer.clear();
                notificationCount = 0;
        }
}

void setup() {
        Serial.begin(115200);
        BLEDevice::init("");

        BLEScan *pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setActiveScan(true);
        pBLEScan->start(5);
}

void loop() {
        if (doConnect && !connected) {
                pClient = BLEDevice::createClient();

                if (pClient->connect(*pServerAddress)) {
                        Serial.println("Connected to BMS");

                        pRemoteService =
                            pClient->getService("0000ff00-0000-1000-8000-00805f9b34fb");
                        if (!pRemoteService) {
                                Serial.println("Failed to find service");
                                return;
                        }

                        pRemoteCharacteristicWrite = pRemoteService->getCharacteristic(
                            BLEUUID("0000ff02-0000-1000-8000-00805f9b34fb"));
                        pRemoteCharacteristicRead = pRemoteService->getCharacteristic(
                            BLEUUID("0000ff01-0000-1000-8000-00805f9b34fb"));

                        if (!pRemoteCharacteristicWrite || !pRemoteCharacteristicRead) {
                                Serial.println("Missing characteristics.");
                                return;
                        }

                        // Enable notifications
                        if (pRemoteCharacteristicRead->canNotify()) {
                                pRemoteCharacteristicRead->registerForNotify(notifyCallback);

                                BLERemoteDescriptor *p2902 =
                                    pRemoteCharacteristicRead->getDescriptor(
                                        BLEUUID((uint16_t)0x2902));
                                if (p2902) {
                                        uint8_t notifyOn[] = {0x01, 0x00};
                                        p2902->writeValue(notifyOn, 2, true);
                                        Serial.println("Notifications enabled.");
                                }
                        }

                        // Add a delay before sending the command
                        delay(300);

                        // Send read basic info command: 0xDD A5 03 00 FF FD 77
                        uint8_t command[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
                        pRemoteCharacteristicWrite->writeValue(command, sizeof(command));
                        Serial.println("Command sent to BMS.");

                        lastNotifyTime = millis();
                        connected = true;
                } else {
                        Serial.println("Connection failed.");
                        doConnect = false;
                }
        }

        // Optional timeout debug (5 seconds)
        if (connected && notificationCount == 0 && millis() - lastNotifyTime > 5000) {
                Serial.println("⚠️ Timeout: No notification response from BMS.");
                lastNotifyTime = millis(); // Reset to avoid spamming
        }
}
