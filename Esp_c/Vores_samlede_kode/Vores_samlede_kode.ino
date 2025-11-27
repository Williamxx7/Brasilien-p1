
#include <Adafruit_HX711.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClient.h>
#include <WiFiNINA.h> // Or use <WiFi.h>
#include <cassert>	  // Required for assert
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


LiquidCrystal_I2C lcd(0x27, 16, 2); // Display definition
const int LOADCELL_DOUT_PIN = 16;	// HX711 pin 1
const int LOADCELL_SCK_PIN = 4;		// HX711 pin 2

HX711 scale;

// WiFi Configuration - We should change this to a ESP to ESP connection with
// the mother ESP connected to LAN
const char *ssid = "Your_SSID";
const char *password = "Your_PASSWORD";
WiFiServer server(80);

String currentUserId = "";
String currentMaterial = "";
bool weight_start = false;
bool isConnected = false; // Bluetooth connection status

void setup()
{
	Serial.begin(115200);		   // Serial communication
	BLEDevice::init(DEVICE_NAME);
	pServer = BLEDevice::createServer();
	BLEService *pService = pServer->createService(SERVICE_UUID);
	pUserInfoCharacteristic = pService->createCharacteristic(
    USER_INFO_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
  pCommandCharacteristic = pService->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
	  pWeightDataCharacteristic = pService->createCharacteristic(
    WEIGHT_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );

  pCommandCharacteristic->setCallbacks(new CommandCallbacks());
  pWeightDataCharacteristic->addDescriptor(new BLE2902()); // Required for notifications

  pUserInfoCharacteristic->setValue("NO_USER");
  pCommandCharacteristic->setValue("READY");
  pWeightDataCharacteristic->setValue("0.0");

	  // Start service
  pService->start();


	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(SERVICE_UUID); //tilfjøer mappen til advertising
  // Helps with iPhone pairing
  pAdvertising->setScanResponse(true); //add a reply to scans
  pAdvertising->setMinPreferred(0x12); //min ms between scans

  BLEDevice::startAdvertising(); //starter faktisk advertising med den nye information, så mobiler kan finde den

	 // Bluetooth device name
	Serial.println("Bluetooth started, waiting for connection...");

	WiFi.begin(ssid, password);
	lcd.init(); // Display initialization
	lcd.backlight();

	lcd.setCursor(0, 0);
	lcd.print("Connecting WiFi");
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("WiFi Connected!");
	lcd.setCursor(0, 1);
	lcd.print(WiFi.localIP());
	delay(2000);

	server.begin();

	scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); // Scale setup
	scale.set_scale(20.21);							  // Correction factor
	scale.tare();									  // Tare

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Waiting BT Conn");

	// Initialising variables for later use
	float weightg = 0;
}

class UserInfoCallbacks : public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic *pCharacteristic) override {

    // value er tekst som fx "USER:32329414;MAT:ALU"
		String raw = String(pCharacteristic->getValue().c_str()); //gemmer dataen i en string "raw"
    Serial.print("USER_INFO raw: ");
    Serial.println(raw);

    // Parse USER:
    int userPos = raw.indexOf("USER:"); //get int of the starting position 
    int matPos  = raw.indexOf("MAT:"); //get int of the starting position

		if (userPos >= 0) {
			int sep = raw.indexOf(';', userPos);
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

}

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
			weight_start = true;

			

String sendPostData(String post_data)
{
	String response = "";

	// Send data to your website
	// Update with actual server info
	if (client.connect(server, 1234))
	{
		Serial.println("Connected to server");

		// Prepare HTTP POST request
		client.println("POST /data_endpoint HTTP/1.1"); // Make "/data_endpoint" into a variable instead
		client.println("Host: vistimalik.com:1234");
		client.println("Content-Type: application/JSON");
		client.print("Content-Length: ");
		client.println(post_data.length());
		client.println();
		client.println(post_data); // Send the data

		// Wait for a response
		while (client.connected())
		{
			if (client.available())
			{
				char c = client.read();
				Serial.print(response);
				response += c;
			}
		}

		client.stop(); // Close the connection
	}
	else
	{
		Serial.println("Connection failed");
		response = "Connection failed";
	}

	return response;
}

void loop()
{
/*	Check connection til bluetooth
	//if (SerialBT.hasClient())
	//{
		if (!isConnected)
		{
			isConnected = true;
			Serial.println("Bluetooth connected!");
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("BT Connected!");
		}
*/
		// Check for incoming data from Bluetooth
		// Assumption is that data is in JSON format like this -
		//  {"trash_type": "palstic/metal/paper", "wastepicker_name": "some
		//  name", "waste_picker_ID": "22331"}
		// Assumption - Waste pickers will have to press a button in the BT App to send their data to the scale after connecting to it

		//Den er forbundet og den få dataen. Alt kommunikationen



	// Check if there is already weight placed on the scale.
	// If not, LCD will display a message that asks to place weight on the scale
	// to be measured.
	if(weight_start){
		float weightg = scale.get_units(10);

		if (weightg < 10000)
		{
			lcd.print("Place weight...");
		}
		else
		{
			// Measure weight
			
			float oldWeight = weightg;
			float weightg = scale.get_units(10);
			float weightkg = weightg / 1000;

			if (weightg != oldWeight)
			{
				lcd.clear();
				lcd.setCursor(0, 1);
				lcd.print(weightkg);
				lcd.print(" Kg ");
				char buffer[16];
				dtostrf(weightkg,1,2,buffer);
				pWeightDataCharacteristic->setValue(buffer);
				pWeightDataCharacteristic->notify();
			}
			else{
				lcd.clear();
				lcd.setCursor(0, 1);
				lcd.print(" DONE ");
				lcd.clear();
				lcd.setCursor(0, 1);
				lcd.print(weightkg);
				lcd.print(" Kg ");
				char buffer[16];
				pWeightDataCharacteristic->setValue(buffer);
				pWeightDataCharacteristic->notify();
				weight_start = false;
				Serial.println("Weight is OFF, data is sent");
			}<
			delay(100);
		}
	}
	
	// else
	// {
	// 	if (isConnected)
	// 	{
	// 		isConnected = false;
	// 		Serial.println("Bluetooth disconnected!");
	// 		lcd.clear();
	// 		lcd.setCursor(0, 0);
	// 		lcd.print("BT Disconnected");
	// 		delay(2000);
	// 		lcd.clear();
	// 		lcd.setCursor(0, 0);
	// 		lcd.print("Waiting BT Conn");
	// 	}
	// }
}
// Make HTTP Post Request instead
// Will send data in JSON format, too, like this -
// {"trash_type": "palstic/metal/paper", "wastepicker_name":
// "some name", "waste_picker_ID": "22331", "weight": 123}

// TO DO:
// - HTTP POST request to API -- DONE
// - Fix Wifi Client handling
// - App via BT sends signal to weight and send data - Make a button in the app that sends their data when pressed.
// - BT response to App about the weight send to API
// - Error handling and responses thru BT to App