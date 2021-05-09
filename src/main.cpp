#include <Arduino.h>
#include <SPI.h>         // RC522 Module uses SPI protocol
#include <MFRC522.h>     // Library for Mifare RC522 Devices
#include <WiFi.h>        // Libary for Wifi
#include <ArduinoJson.h> // Libary for parsiong Json objects
#include <EEPROM.h>      // Library for reading and writing to eeprom
#include <ArduinoHttpClient.h>  


#include "secrets.h"

// ----------- STEPPER SETTINGS -------------
#define SPT 200       //Steps per turn
#define openTime 5000 // Time before locking the door again

#define DIR 25    //Stepper Pins
#define STEP 26   //Stepper Pins
#define ENABLE 13 //Stepper Pins

//------------ LED SETTINGS -----------------
#define LED_ON HIGH
#define LED_OFF LOW

#define redLed 16 // Set Led Pins
#define greenLed 5
#define blueLed 17
#define groundLed 4

#define wipeB 3

#define EEPROM_SIZE 256

int successRead; // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4]; // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module

String cardId;

#define SS_PIN 21
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance.

#define address "http://192.168.1.116/door?card="
#define ip "192.168.1.116"
#define port 80

WiFiClient wifi;
HttpClient client = HttpClient(wifi, ip, port);
int status = WL_IDLE_STATUS;

/////////////////////////////////////////  Rotate Stepper    ///////////////////////////////////
void stepperTurn(String direction)
{
  digitalWrite(ENABLE, LOW);
  if (direction == "left")
    digitalWrite(DIR, LOW);
  else
    digitalWrite(DIR, HIGH);
  for (int i = 0; i < SPT; i++)
  {
    digitalWrite(STEP, HIGH);
    delay(10);
    digitalWrite(STEP, LOW);
    delay(10);
  }
  digitalWrite(ENABLE, HIGH);
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted(int setDelay)
{
  Serial.println("Access Granted");
  digitalWrite(blueLed, LED_OFF); // Turn off blue LED
  digitalWrite(redLed, LED_OFF);  // Turn off red LED
  digitalWrite(greenLed, LED_ON); // Turn on green LED
  stepperTurn("right");
  delay(setDelay);
  stepperTurn("left");
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied()
{
  Serial.println("Access denied");
  digitalWrite(greenLed, LED_OFF); // Make sure green LED is off
  digitalWrite(blueLed, LED_OFF);  // Make sure blue LED is off
  digitalWrite(redLed, LED_ON);    // Turn on red LED
  delay(1000);
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID(int number)
{
  int start = (number * 4) + 2; // Figure out starting position
  for (int i = 0; i < 4; i++)
  {                                         // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i); // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo(byte a[], byte b[])
{
  bool match = false;
  if (a[0])       // Make sure there is something in the array first
    match = true; // Assume they match at first
  for (int k = 0; k < 4; k++)
  {                   // Loop 4 times
    if (a[k] != b[k]) // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if (match)
  {              // Check to see if if match is still true
    return true; // Return true
  }
  else
  {
    return false; // Return false
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID(byte find[])
{
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for (int i = 1; i <= count; i++)
  {            // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard))
    { // Check to see if the storedCard read from EEPROM
      return true;
      break; // Stop looking we found it
    }
    else
    { // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
int getID()
{
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent())
  { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial())
  { //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++)
  { //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID(byte a[])
{
  if (!findID(a))
  {                            // Before we write to the EEPROM, check to see if we have seen this card before!
    int num = EEPROM.read(0);  // Get the numer of used spaces, position 0 stores the number of ID cards
    int start = (num * 4) + 6; // Figure out where the next slot starts
    num++;                     // Increment the counter by one
    EEPROM.write(0, num);      // Write the new count to the counter
    for (int j = 0; j < 4; j++)
    {                                // Loop 4 times
      EEPROM.write(start + j, a[j]); // Write the array values to EEPROM in the right position
    }
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else
  {
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn()
{
  digitalWrite(blueLed, LED_ON);   // Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF);   // Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF); // Make sure Green LED is off
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
int findIDSLOT(byte find[])
{
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for (int i = 1; i <= count; i++)
  {            // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard))
    { // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; // The slot number of the card
      break;    // Stop looking we found it
    }
  }
  return NULL;
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID(byte a[])
{
  if (!findID(a))
  { // Before we delete from the EEPROM, check to see if we have this card!
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else
  {
    int num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    int slot;                 // Figure out the slot number of the card
    int start;                // = ( num * 4 ) + 6; // Figure out where the next slot starts
    int looping;              // The number of times the loop repeats
    int j;
    slot = findIDSLOT(a);       // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;                // Decrement the counter by one
    EEPROM.write(0, num); // Write the new count to the counter
    for (j = 0; j < looping; j++)
    {                                                      // Loop the card shift times
      EEPROM.write(start + j, EEPROM.read(start + 4 + j)); // Shift the array values to 4 places earlier in the EEPROM
    }
    for (int k = 0; k < 4; k++)
    { // Shifting loop
      EEPROM.write(start + j + k, 0);
    }
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

bool checkID(byte id[4])
{
  StaticJsonDocument<200> doc;
  cardId = "0";
  for (int i; i <= 3; i++)
  {
    cardId += id[i];
  }
  Serial.print("Card ID: ");
  Serial.println(cardId);
  if ((WiFi.status() == WL_CONNECTED))
  { //Check the current connection status
    Serial.println("making GET request");
    client.beginRequest();
    client.get("door?card=" + cardId);
    //client.sendHeader("X-CUSTOM-HEADER", "custom_value");
    client.endRequest();

    // read the status code and body of the response
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();

    Serial.print("GET Status code: ");
    Serial.println(statusCode);
    Serial.print("GET Response: ");
    Serial.println(response);

    Serial.println("Wait five seconds");
    if (statusCode > 0)
    { //Check for the returning code
      Serial.println(response);
      DeserializationError error = deserializeJson(doc, response);
      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
      }
      if (!findID(id))
      {
        writeID(id);
      }
      return doc["open"];
    }
    else
    {
      Serial.println("Error on HTTP request");
    }
  }
  if (findID(id))
  {
    return true;
  }
  else
    return false;
}

///////////////////////////////////////// Setup ///////////////////////////////////
void setup()
{
  //Arduino Pin Configuration
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(groundLed, OUTPUT);
  pinMode(DIR, OUTPUT); //Stepper outputs
  pinMode(STEP, OUTPUT);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(redLed, LED_OFF);   // Make sure led is off
  digitalWrite(greenLed, LED_OFF); // Make sure led is off
  digitalWrite(blueLed, LED_OFF);  // Make sure led is off
  digitalWrite(groundLed, LOW);

  digitalWrite(ENABLE, HIGH); //Turn stepper motor off

  //Protocol Configuration
  Serial.begin(115200); // Initialize serial communications with PC
  SPI.begin();          // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();   // Initialize MFRC522 Hardware

  EEPROM.begin(EEPROM_SIZE);

  //If you set Antenna Gain to Max it will increase reading distance
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  //whipe EEPROM if button pressed
  if (digitalRead(wipeB) == LOW)
  {                               // when button pressed pin should get low, button connected to ground
    digitalWrite(redLed, LED_ON); // Red Led stays on to inform user we are going to wipe
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 15 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    delay(5000); // Give user enough time to cancel operation
    if (digitalRead(wipeB) == LOW)
    { // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      for (int x = 0; x < EEPROM.length(); x = x + 1)
      { //Loop end of EEPROM address
        if (EEPROM.read(x) == 0)
        { //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else
        {
          EEPROM.write(x, 0); // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      digitalWrite(redLed, LED_OFF); // visualize successful wipe
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
    }
    else
    {
      Serial.println(F("Wiping Cancelled"));
      digitalWrite(redLed, LED_OFF);
    }
  }

  //wifi setup
  delay(4000);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  Serial.println(F("Access Control v3.4")); // For debugging purposes
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything Ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(200);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(200);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  normalModeOn();
}

///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop()
{
  do
  {
    successRead = getID(); // sets successRead to 1 when we get read from reader otherwise 0
  } while (!successRead);  //the program will not go further while you not get a successful read
  if (checkID(readCard))
  {
    Serial.println(F("Welcome, You shall pass"));
    granted(openTime);
  }
  else
  { // If not, show that the ID was not valid
    Serial.println(F("You shall not pass"));
    denied();
  }
  normalModeOn();
}