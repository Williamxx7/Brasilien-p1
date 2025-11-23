#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_HX711.h>

// BLE UUIDs
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID_weight("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID charUUID_id("12345678-1234-1234-1234-1234567890ab");

BLECharacteristic *pCharWeight;
String currentID = "";
float lastNotifiedWeight = 0.0;

// HX711 pins (matches your existing scale)
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
Adafruit_HX711 scale;

// BLE callbacks
bool deviceConnected = false;
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) override { deviceConnected = false; }
};

class IDWriteCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string val = pCharacteristic->getValue();
    if (val.length() > 0) currentID = String(val.c_str());
  }
};

void setup() {
  Serial.begin(115200);

  // HX711 scale setup
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(20.21);  // adjust to your scale factor
  scale.tare();

  // BLE setup
  BLEDevice::init("ESP32_Weight");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(serviceUUID);

  pCharWeight = pService->createCharacteristic(
    charUUID_weight,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pCharWeight->addDescriptor(new BLE2902());

  BLECharacteristic *pCharID = pService->createCharacteristic(
    charUUID_id,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
  pCharID->setCallbacks(new IDWriteCallback());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE Weight Broadcaster started.");
}

unsigned long lastNotify = 0;
const unsigned long notifyInterval = 1000; // 1 second

void loop() {
  // Read weight from your scale
  float weightg = scale.get_units(10);
  float weightkg = weightg / 1000.0;

  // Notify connected client every second if weight changed
  if (deviceConnected && millis() - lastNotify > notifyInterval) {
    lastNotify = millis();

    if (abs(weightkg - lastNotifiedWeight) > 0.01) { // notify on ~10g change
      lastNotifiedWeight = weightkg;

      char buf[64];
      if (currentID.length())
        snprintf(buf, sizeof(buf), "ID:%s;W:%.2f", currentID.c_str(), weightkg);
      else
        snprintf(buf, sizeof(buf), "W:%.2f", weightkg);

      pCharWeight->setValue((uint8_t*)buf, strlen(buf));
      pCharWeight->notify();
      Serial.print("Notify -> "); Serial.println(buf);
    }
  }

  delay(10);
}
