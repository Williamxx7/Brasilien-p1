#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

BLEServer *pServer;

#define DEVICE_NAME            "ESP32_Weigh_1"
#define SERVICE_UUID           "ab49b033-1163-48db-931c-9c2a3002ee1d"
#define USER_INFO_CHAR_UUID    "ab49b033-1163-48db-931c-9c2a3002ee1f"
#define COMMAND_CHAR_UUID      "ab49b033-1163-48db-931c-9c2a3002ee1e"
#define WEIGHT_DATA_CHAR_UUID  "ab49b033-1163-48db-931c-9c2a3002ee20"

BLECharacteristic *pUserInfoCharacteristic;
BLECharacteristic *pCommandCharacteristic;
BLECharacteristic *pWeightDataCharacteristic;

String currentUserId = "";
String currentMaterial = "";

// Faux weight measurement state
float fakeWeight = 0.0;
bool weightSent = false;  // Track if weight has been sent

// Callback til USER_INFO characteristic
class UserInfoCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    // value er tekst ala "USER:32329414;MAT:ALU"
    String raw = String(pCharacteristic->getValue().c_str());
    Serial.print("USER_INFO raw: ");
    Serial.println(raw);

    // Parse USER:
    int userPos = raw.indexOf("USER:");
    int matPos  = raw.indexOf("MAT:");

    if (userPos >= 0) {
      int sep = raw.indexOf(';', userPos);
      if (sep < 0) serial.println("ERROR, no seperation found");
      currentUserId = raw.substring(userPos + 5, sep); // 5 = længden af "USER:"
    }

    if (matPos >= 0) {
      int sep = raw.indexOf(';', matPos);
      if (sep < 0) serial.println("ERROR, no seperation found");
      currentMaterial = raw.substring(matPos + 4, sep); // 4 = længden af "MAT:"
    }

    Serial.print("Parsed USER ID: ");
    Serial.println(currentUserId);
    Serial.print("Parsed MATERIAL: ");
    Serial.println(currentMaterial);
  }
};

// Callback til COMMAND characteristic
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String cmd = String(pCharacteristic->getValue().c_str());
    Serial.print("COMMAND: ");
    Serial.println(cmd);

    if (cmd == "START") {
      Serial.println("-> START command received");
      Serial.print("   For USER: ");
      Serial.print(currentUserId);
      Serial.print("  MATERIAL: ");
      Serial.println(currentMaterial);

      // Generate a single random weight value
      fakeWeight = random(1000, 5000) / 10.0; // Random weight between 100.0 and 500.0g
      Serial.print("   Generated weight: ");
      Serial.println(fakeWeight);

      // Send the weight once
      if (!weightSent) {
        char weightStr[16];
        snprintf(weightStr, sizeof(weightStr), "%.1f", fakeWeight);
        pWeightDataCharacteristic->setValue(weightStr);
        pWeightDataCharacteristic->notify();
        weightSent = true;
        Serial.print("   Sent weight notification: ");
        Serial.println(weightStr);
      }
    } else if (cmd == "CONFIRM_INFO") {
      Serial.println("-> CONFIRM_INFO command received");
    } else if (cmd == "CONFIRM_RESULT") {
      Serial.println("-> CONFIRM_RESULT command received");
      Serial.print("\nConfirmed weight: ");
      Serial.print(fakeWeight, 1);
      Serial.print("g\nfor USER: ");
      Serial.print(currentUserId);
      Serial.print("\nMATERIAL: ");
      Serial.println(currentMaterial);
      // Reset for next measurement
      weightSent = false;
      // TODO: send data til database her
    } else {
      Serial.println("-> Unknown command");
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Booting BLE...");

  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();

  // Opret service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // USER_INFO characteristic (WRITE)
  pUserInfoCharacteristic = pService->createCharacteristic(
    USER_INFO_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
  pUserInfoCharacteristic->setCallbacks(new UserInfoCallbacks());

  // COMMAND characteristic (WRITE)
  pCommandCharacteristic = pService->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
  pCommandCharacteristic->setCallbacks(new CommandCallbacks());

  // WEIGHT_DATA characteristic (NOTIFY + READ)
  pWeightDataCharacteristic = pService->createCharacteristic(
    WEIGHT_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pWeightDataCharacteristic->addDescriptor(new BLE2902()); // Required for notifications

  // Optional: initial values
  pUserInfoCharacteristic->setValue("NO_USER");
  pCommandCharacteristic->setValue("READY");
  pWeightDataCharacteristic->setValue("0.0");

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // Helps with iPhone pairing
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("Ready! For your bits!");
}

void loop() {
  // No continuous weight updates - weight is sent once on START command
}