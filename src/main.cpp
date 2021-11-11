#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>
#include "ESPAsyncWebServer.h"
#include <EEPROM.h>
#include <MFRC522.h>

#define openTime 5000 // Time before locking the door again
#define wipeB 3

byte *successRead;

#define SS_PIN 21
#define RST_PIN 22

byte readCard[4]; // Stores scanned ID read from RFID Module

MFRC522 mfrc522(SS_PIN, RST_PIN);

#define LED_ON HIGH
#define LED_OFF LOW

//pins
#define redLed 16
#define greenLed 5
#define blueLed 17
#define groundLed 4

int period = 100000;
unsigned long time_now = 0;

#define SPT 200
#define DIR 25
#define STEP 26
#define ENABLE 13

#define EEPROM_SIZE 256
byte storedCard[4]; // Stores an ID read from EEPROM

 //"http://192.168.1.116/door?card="
#define url "192.168.3.2"
#define port 80

WiFiClient wifi;
HttpClient client = HttpClient(wifi, url, port);

#define SECRET_SSID "GO-FT"    // replace MySSID with your WiFi network name
#define SECRET_PASS "GOtech!!" // replace MyPassword with your WiFi password

AsyncWebServer server{80};

int status = WL_IDLE_STATUS;

void setIdle()
{
  digitalWrite(blueLed, LED_ON);   // Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF);   // Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF); // Make sure Green LED is off
}

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

void open(int setDelay)
{
  Serial.println("Access Granted");
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_ON);
  stepperTurn("right");
  delay(setDelay);
  stepperTurn("left");
  setIdle();
}

void denied()
{
  Serial.println("Access denied");
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_ON);
  delay(1000);
  setIdle();
}

byte *getID()
{
  if (!mfrc522.PICC_IsNewCardPresent())
    return 0;
  if (!mfrc522.PICC_ReadCardSerial())
    return 0;
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++)
  {
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return readCard;
}

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

void wipe()
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
  //blink led red
}

void readID(int number)
{
  int start = (number * 4) + 2; // Figure out starting position
  for (int i = 0; i < 4; i++)
  {                                         // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i); // Assign values read from EEPROM to array
  }
}

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
  return -1;
}

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
    slot = findIDSLOT(a); // Figure out the slot number of the card to delete
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
  String cardId = "0";
  for (int i = 0; i <= 3; i++) cardId += id[i];
  Serial.print("Card ID: ");
  Serial.println(cardId);
  if ((WiFi.status() == WL_CONNECTED))
  {
    Serial.println("making GET request");
    client.beginRequest();
    client.get("/door?card="+cardId);
    client.endRequest();

    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    Serial.print("GET Status code: ");
    Serial.println(statusCode);

    if (statusCode == 200)
    {
      Serial.println(response);
      DeserializationError error = deserializeJson(doc, response);
      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
      }
      if (doc["open"])
      {
        Serial.print(F("Permission allowed"));
        writeID(id);
      }
      else if (!doc["open"]) {
        Serial.print(F("Permission denied, deleting card from EEPROM"));
        deleteID(id);
      }
      return doc["open"];
    }
    else
    {
      Serial.println("Http error " + statusCode);
    }
  }
  Serial.print("Not connected to Wifi. Searching in EEPROM");
  if (findID(id))
  {
    Serial.println("Found id in EEPROM");
    return true;
  }
  else
    Serial.println("Could not find id in EEPROM");
    return false;
}

void setup()
{
  Serial.begin(115200);

  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  client.setHttpResponseTimeout(2000);

  pinMode(DIR, OUTPUT); //Stepper outputs
  pinMode(STEP, OUTPUT);
  pinMode(ENABLE, OUTPUT);

  digitalWrite(ENABLE, HIGH); //Turn stepper motor off

  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(groundLed, OUTPUT);

  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(groundLed, LOW);

  EEPROM.begin(EEPROM_SIZE);

  delay(4000);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  for (int i = 0; i < 10; i++) {
        delay(1000);
    Serial.println("Connecting to WiFi..");
    if (WiFi.status() == WL_CONNECTED) break;
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  if(WiFi.status() == WL_CONNECTED) Serial.println("Connected to the WiFi network");
  else Serial.println("Could not connect to the WiFi network");

  AsyncElegantOTA.begin(&server, SECRET_SSID, SECRET_PASS);
  server.begin();

  //wipe EEPROM if button pressed
  if (digitalRead(wipeB) == LOW)
  {
    digitalWrite(redLed, LED_ON);
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 15 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    delay(5000); // Give user enough time to cancel operation
    if (digitalRead(wipeB) == LOW)
      wipe();
    else
    {
      Serial.println(F("Wiping Cancelled"));
      digitalWrite(redLed, LED_OFF);
    }
  }
  setIdle();
}

void loop()
{
  do
  {
    if(millis() >= time_now + period){
        time_now += period;
          mfrc522.PCD_Init();
          mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
          if (WiFi.status() != WL_CONNECTED)   WiFi.begin(SECRET_SSID, SECRET_PASS);
    }
    if (WiFi.status() == WL_CONNECTED) digitalWrite(LED_BUILTIN, HIGH);
    else digitalWrite(LED_BUILTIN, LOW);
    AsyncElegantOTA.loop();
    successRead = getID();

  } while (successRead == 0);
    digitalWrite(blueLed, LED_OFF);   // Blue LED ON and ready to read card
  if (checkID(successRead))
    open(openTime);
  else
    denied();
}