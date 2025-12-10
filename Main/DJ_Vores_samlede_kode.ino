
// 0) Biblioteker
#include <HX711.h> //--Henter bibliotek til vægtsensoren HX711
#include <LiquidCrystal_I2C.h> //--Henter bibliotek til LCD displayet via I2C
#include <WiFiClient.h> //Gør det muligt at lave WiFi-forbindelser
#include <WiFiNINA.h> //-- Man kan enten brug <WiFi.h> eller bare bruge <WiFiNINA.h>, som er WiFi-bibliotek (bruges til ESP med NINA-chip)
#include <cassert>	  //-- Giver mulighed for at bruge assert() i koden
#include <BLEDevice.h> // --Henter Bluetooth-bibliotek til ESP32
#include <BLEUtils.h> //--Ekstra Bluetooth-funktioner
#include <BLEServer.h> //--Muliggør oprettelse af en Bluetooth-server
#include <BLE2902.h> //???? Påkrævet for at lave notifikationer via Bluetooth (subscribe?)

// 1) Bluetooth objekt-pointere: steder i computerens hukommelse, hvor Bluetooth-objekter/data gemmes

BLEServer *pServer; // Pointer der repræsenterer Bluetooth-serveren
BLECharacteristic *pUserInfoCharacteristic; //Et pointer til en Bluetooth-egenskab (characteristic) til brugerinformation
BLECharacteristic *pCommandCharacteristic; // Et pointer til en Bluetooth-egenskab (characteristic) til kommandoer fra appen
BLECharacteristic *pWeightDataCharacteristic; //Et pointer til en Bluetooth-egenskab (characteristic) til vægtdata, notificeret til appen


/*
   2) Navne og UUIDs: Disse linjer definerer enhedens identitet og de unikke UUID'er (Universally Unique Identifiers).
    Navnet gør den synlig for brugeren, mens de unikke UUIDs fungerer som adresser henholdsvis servicen (mappen) og de specifikke datafelter.
    Disse adresser hjælper med at styre brugerinfo og vægtdata
*/

#define DEVICE_NAME            "ESP32_Weigh_1" // Navn på Bluetooth-enheden som mobiltelefonen ser
#define SERVICE_UUID           "ab49b033-1163-48db-931c-9c2a3002ee1d" // Unikt ID for service (mappe) i Bluetooth
#define USER_INFO_CHAR_UUID    "ab49b033-1163-48db-931c-9c2a3002ee1f" // Unikt ID for brugerinfo-feltet
#define COMMAND_CHAR_UUID      "ab49b033-1163-48db-931c-9c2a3002ee1e" // Unkikt ID for kommando-feltet
#define WEIGHT_DATA_CHAR_UUID  "ab49b033-1163-48db-931c-9c2a3002ee20" // Unikt ID for vægtdata-feltet

/*
    3) Enheder og sensorer: IKKE SÅ VIGTIGT FOR OS!
*/


LiquidCrystal_I2C lcd(0x27, 16, 2); // Display definition -> opretter LCD-display med I2C-adresse 0x27
const int LOADCELL_DOUT_PIN = 16;	// HX711 pin 1 -> Data output pin til HX711 vægtsensor
const int LOADCELL_SCK_PIN = 4;		// HX711 pin 2 - > Clock pin til HX711 vægtsensor

HX711 scale; // Opretter et HX711-objekt til vægtsensoren


const char *ssid = "Your_SSID";
const char *password = "Your_PASSWORD";
WiFiServer server(80);


// VIGTIGT for OS!

String currentUserId = ""; // Opretter en global string variabel til at gemme den nuværende bruger-ID som modtages via Bluetooth (starter tom)
String currentMaterial = ""; // Samme som oven, men bruges til at gemme det valgte materiale
volatile bool weight_start = false; // En global boolean der fortæller, om en vejning er startet.
volatile bool weight_stable = false; // Fortæller om vægten er stabil og klar til bekræftelse
bool isConnected = false; // Bruges til at gemme, om Bluetooth er forbundet
volatile float weightkg = 0; // Global variabel, hvor den aktuelle vægt i kg gemmes
float oldWeight = 0; // Gemmer forrige stabile vægt, så du kan se om vægten ændrer sig

// Callback til USER_INFO characteristic (User info + Vægt) -> opretter en klasse der håndterer skrivning til USER_INFO-feltet (onWrite). void OnWrite kører automatisk hver gang appen skriver data til denne characteristic.
class UserInfoCallbacks : public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic *pCharacteristic) override {

    /*
        1. pCharacteristic -> GetValue() returnerer den data, som appen har sendt til USER_INFO-feltet.
        2. .c_str() konverterer denne data til en C-style streng (char array).
        3. String(...) laver dem til en Arduino String.
        Value er tekst som fx "USER:32329414;MAT:ALU"
    */ 
	
    String raw = String(pCharacteristic->getValue().c_str()); //gemmer dataen i en string "raw"
    Serial.print("USER_INFO raw: ");
    Serial.println(raw);

    // Parse USER:
    int userPos = raw.indexOf("USER:"); //Finder positionen i strengen, hvor "USER:" starter. HVIS IKKE FUNDET -> returnerer -1
    int matPos  = raw.indexOf("MAT:"); //Finder postion for "MAT:"

		if (userPos >= 0) {
			int sep = raw.indexOf(';', userPos); //Hvis user findes -> leder efter semikolon efter "USER:" for at finde slutningen af værdien
      if (sep < 0) {
        Serial.println("ERROR, no separation found for USER");
        return;
      }
      currentUserId = raw.substring(userPos + 5, sep); // 5 tegne = længden af "USER:" -> udrager det, der står efter "USER:"
		}
    if (matPos >= 0) {
      int sep = raw.indexOf(';', matPos);
      if (sep < 0) sep = raw.length(); // For begge. Hvis materialet står sidst uden semikolon, bruger vi bare slutningen af strengen
      currentMaterial = raw.substring(matPos + 4, sep); // 4 = længden af "MAT:" -> uddrager materialet efter "MAT:"
    }

    // Debug print -> udskriver det som er blevet modtaget. (Parse: program analyserer rå data eller kode og omdanner det til et struktureret format)
    Serial.print("Parsed USER ID: ");
    Serial.println(currentUserId);
    Serial.print("Parsed MATERIAL: ");
    Serial.println(currentMaterial);

	}

};

// "CommandCallbacks" er en C++ klasse, der ved at arve fra "BLECharacteristicCallbacks" muliggør reaktion på BLE-skrivehandlinger. "onWrite()"" er metoden, der automatisk køres, når en telefonen skriver data til den tilknyttede "COMMAND-characteristic"
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
		String cmd = String(pCharacteristic->getValue().c_str()); // Henter bytes/stren som klienten skrev -> konverterer den til et C-stil const char -> laver en arduino string (nemt sammenligne tekst)
        // Linje ovenpå henter kommandoen sendt fra telefonen ("START" eller "CONFIRM_RESULT") og gemmed den i cmd
    Serial.print("COMMAND: ");
    Serial.println(cmd);

    if (cmd == "START") {
      Serial.println("-> START command received"); // Hvis cmd er "START", så printer den at start-kommando er modtaget
      Serial.print("   For USER: ");
      Serial.print(currentUserId);
      Serial.print("  MATERIAL: ");
      Serial.println(currentMaterial); // De 4 linjer ovenfor printer den bruger-ID og materiale som blev modtaget tidligere via USER_INFO characteristic og udskriver ekstra kontekst -> hvilket currentUserId og currentMaterial der er gældende for denne start-kommando.
			weight_start = true; // Sætter global variabel weight_start til true, hvilket signalerer til hovedprogrammet at en vejning skal starte
			weight_stable = false; // Sætter weight_stable til false, da vægten endnu ikke er stabil ved start
    }
		else if (cmd == "CONFIRM_RESULT") //Hvis kommandoen ikke var START men "CONFIRM_RESULT", køres den blok. Det er kommandoen klienten bruger til at bede enheden om at "bekræfte" den målte vægt -> færdiggøre processen{
      if (!weight_stable) {
        Serial.println("-> ERROR: Cannot confirm - weight not stable yet"); // Hvis vægten ikke er stabil endnu (weight_stable er false), printer den en fejlmeddelelse
        pCommandCharacteristic->setValue("ERROR_NOT_READY"); // Sætter værdien af COMMAND-characteristic til "ERROR_NOT_READY", som klienten kan læse for at forstå at bekræftelsen mislykkedes fordi vægten ikke er klar
        return; // Afslutter funktionen tidligt, så resten af koden i denne blok ikke køres
      }

      Serial.println("-> CONFIRM_RESULT command received"); // Hvis vægten er stabil, printer den at bekræftelseskommandoen er modtaget
      Serial.print("\nConfirmed weight: "); // Printer den bekræftede vægt
      Serial.print(weightkg, 1); // Viser vægten med 1 decimal
      Serial.print("kg\nfor USER: "); // Printer "kg for USER:"
      Serial.print(currentUserId); //Printer bruger ID
      Serial.print("\nMATERIAL: "); //Printer "MATERIAL:"
      Serial.println(currentMaterial); //Printer materiale

      // Reset for next measurement
      weight_start = false; // Stopper måletilstand
      weight_stable = false; // Rydder stabilitetsflag
      oldWeight = 0; //nulstiller gammel vægt

      // TODO: send data til database her
    }
  }
};

void setup() // Setup-funktionen kører en gang ved opstart af enheden
{
	Serial.begin(115200);		   // Serial kommunication ved hastighed 115200 baud, så man kan se debug-beskeder i Serial Monitor
	BLEDevice::init(DEVICE_NAME);  // Initialiserer BLE med et bestemt enhedsnavn (DEVICE_NAME)
	pServer = BLEDevice::createServer(); // Opretter en BLE-server, som mobilen kan forbinde til
	BLEService *pService = pServer->createService(SERVICE_UUID); // Opretter en BLE-service (mappe) med en unik UUID (SERVICE_UUID)
	pUserInfoCharacteristic = pService->createCharacteristic( // Opretter en characteristic (en "fil" i mappen), denne kan læses og skrives af mobil-appen, bruges til at sende/bruge user info
    USER_INFO_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
  pCommandCharacteristic = pService->createCharacteristic( //En anden characteristic til kommandoer fra appen (START, CONFIRM_RESULT), Mobilen må læse og skrive
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
  );
	  pWeightDataCharacteristic = pService->createCharacteristic( //Characteristic til vægtdata, som enheden sender notifikationer om til appen (når vægten ændres), Kan læses og notifies, så mobilen automatisk får nye målresultater
    WEIGHT_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );

  // Callbacks - sætter de klasser der håndterer skrivninger til de forskellige characteristics
  pUserInfoCharacteristic->setCallbacks(new UserInfoCallbacks()); //Sætter en callback, der reagerer når user info skrives
  pCommandCharacteristic->setCallbacks(new CommandCallbacks()); //Sætter callback for kommandoer (fx når appen sender "START")
  pWeightDataCharacteristic->addDescriptor(new BLE2902()); // Gør det muligt for smartphones at aktivere notifikationer (meget vigtigt især for iPhone)

  //Standardværdier 
  pUserInfoCharacteristic->setValue("NO_USER"); // Standardværdi: ingen bruger valgt
  pCommandCharacteristic->setValue("READY"); // Standardværdi: enheden er klar
  pWeightDataCharacteristic->setValue("0.0"); // Sender vægten som 0.0, før målinger starter

	  // Start "mappen"
  pService->start(); // Starter BLE-servicen, så den er klar til at modtage forbindelser og data (karakteristiks kan bruges)

  // Advertising - Gør enheden synlig for mobiltelefoner
	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising(); //Henter et advertising-objekt til at konfigurere udsendelsen af Bluetooth-signaler
	pAdvertising->addServiceUUID(SERVICE_UUID); //Tilfjøer mappen til advertising (service UUID), så telefonen kan se, hvilken service enheden tilbyder
  // Helps with iPhone pairing
  pAdvertising->setScanResponse(true); //Tilføjer et ekstra svar, som gør pairing bedre - især på iPhones
  pAdvertising->setMinPreferred(0x12); //Sætter minimum pause mellem scan-requests, forbedrer stabilitet

  BLEDevice::startAdvertising(); //Starter faktisk advertising med den nye information, så mobiler kan finde den

	 // Bluetooth device name
	Serial.println("Bluetooth started, waiting for connection..."); //Skriver til Serial Monitor, at Bluetooth nu er aktiv og venter på en mobilforbindelse







	//gammel kode sidste år
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

}

String sendPostData(String post_data)
// {
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
		weightkg = weightg / 1000; // Update global weightkg

		if (weightg < 10000)
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Place weight...");
			oldWeight = 0; // Reset when no weight
			weight_stable = false;
		}
		else
		{
			// Measure weight
			if (abs(weightg - oldWeight) > 5) // If weight changed by more than 5g
			{
				oldWeight = weightg;
				weight_stable = false; // Weight is changing, not ready
				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print("Measuring...");
				lcd.setCursor(0, 1);
				lcd.print(weightkg);
				lcd.print(" Kg ");
				char buffer[16];
				dtostrf(weightkg,1,2,buffer);
				pWeightDataCharacteristic->setValue(buffer);
				pWeightDataCharacteristic->notify();
			}
			else{
				// Weight is stable
				if (!weight_stable) {
					// First time stable - mark as ready
					weight_stable = true;
					Serial.println("Weight is stable, ready for confirmation");
				}
				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print(" DONE ");
				lcd.setCursor(0, 1);
				lcd.print(weightkg);
				lcd.print(" Kg ");
				char buffer[16];
				dtostrf(weightkg,1,2,buffer);
				pWeightDataCharacteristic->setValue(buffer);
				pWeightDataCharacteristic->notify();
				// Don't reset weight_start - wait for CONFIRM_RESULT command
			}
		}
		delay(100);
	}
	
}
