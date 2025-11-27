
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <Adafruit_HX711.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClient.h>
#include <WiFiNINA.h> // Or use <WiFi.h>
#include <cassert>	  // Required for assert

LiquidCrystal_I2C lcd(0x27, 16, 2); // Display definition
const int LOADCELL_DOUT_PIN = 16;	// HX711 pin 1
const int LOADCELL_SCK_PIN = 4;		// HX711 pin 2

HX711 scale;
BluetoothSerial SerialBT;

// WiFi Configuration - We should change this to a ESP to ESP connection with
// the mother ESP connected to LAN
const char *ssid = "Your_SSID";
const char *password = "Your_PASSWORD";
WiFiServer server(80);

String wastepicker = "";  // Variable to store the name of the waste picker
bool isConnected = false; // Bluetooth connection status

void setup()
{
	Serial.begin(115200);		   // Serial communication
	SerialBT.begin("ESP32_Scale"); // Bluetooth device name
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
	if (SerialBT.hasClient())
	{
		if (!isConnected)
		{
			isConnected = true;
			Serial.println("Bluetooth connected!");
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("BT Connected!");
		}

		// EMILS NYE KODE:
		// Ser om Bluetooth er avaialabile
		// Vil læse bluetooth og så gemme det i string wastepicker_ID
		char wastepicker_ID[20] = "";
		if (SerialBT.available())
		{
			int len = SerialBT.readBytesUntil('\n', wastepicker_ID, sizeof(wastepicker_ID) - 1);
			// readByesUntil: Læser indtil \n, smider det hen i wastepicker_ID, og den stoppe lige inden \0
			wastepicker_ID[len] = '\0'; // ender strengen på \0

			Serial.print("Modtaget wastepicker_ID: ");
			Serial.println(wastepicker_ID);
		}

		// Check for incoming data from Bluetooth
		// Assumption is that data is in JSON format like this -
		//  {"trash_type": "palstic/metal/paper", "wastepicker_name": "some
		//  name", "waste_picker_ID": "22331"}
		// Assumption - Waste pickers will have to press a button in the BT App to send their data to the scale after connecting to it

		if (SerialBT.avaliable())
		{
			String json_data = "";
			char current_char = " ";
			while (current_char != "\n")
			{
				current_char = SerialBT.read();
				json_data += current_char;
			}
			JsonDocument doc;
			deserializeJson(doc, json_data);
			try
			{
				char *wastepicker_name = doc["wastepicker_name"];
				char *wastepicker_ID = doc["wastepicker_ID"];
				char *trash_type = doc["trash_type"];
				assert(wastepicker_name != "" && wastepicker_ID != "" &&
					   trash_type != "");

				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print("Welcome:");
				lcd.setCursor(0, 1);
				lcd.print(wastepicker_name);
				delay(7000); // Update delays later to make it fit as well as possible
				lcd.clear();
				lcd.setCursor(0, 0);

				char[] new_json_data;
				doc["weight"] = weightkg;
				serializeJson(doc, new_json_data);
				String response = sendPostData(String(new_json_data));
				if (response == "OK")
				{
					lcd.print("Weight registered");
					SerialBT.println("OK");
					delay(5000); // Update delays later to make it fit as well as possible
				}
				else
				{
					lcd.print("Error");
					lcd.setCursor(0, 1);
					lcd.print("Try again");
					SerialBT.println("Error");
					delay(10000); // Update delays later to make it fit as well as possible
				}
			}
		}
		catch
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("ERROR: reading BT");
		}
	}

	// Check if there is already weight placed on the scale.
	// If not, LCD will display a message that asks to place weight on the scale
	// to be measured.
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
		}
	}
}
else
{
	if (isConnected)
	{
		isConnected = false;
		Serial.println("Bluetooth disconnected!");
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("BT Disconnected");
		delay(2000);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Waiting BT Conn");
	}
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