#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#include "ESPAsyncWebServer.h"
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

#include "../Configuration.hpp"

byte *successRead;
byte readCard[4]; // Stores scanned ID read from RFID Module
byte storedCard[4]; // Stores an ID read from EEPROM
boolean accessGranted = false;
const long timeout = 3000;            // timeout for mqtt request
boolean response = false;             // if we got a response from mqtt

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
const int DELAY_BETWEEN_CARDS = 500;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;

const char* mqtt_server = MQTT_SERVER;

AsyncWebServer server{80};

WiFiClient wifi;
PubSubClient client(wifi);
int status = WL_IDLE_STATUS;


// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);


void setGranted()
{
  strip.fill(strip.Color(0, 255, 0), 0, strip.numPixels());
  strip.show();
}

void setDenied()
{
  strip.fill(strip.Color(255, 0, 0), 0, strip.numPixels());
  strip.show();
}

void setDetected() {
  strip.fill(strip.Color(0, 0, 255), 0, strip.numPixels());
  strip.show(); 
}

void setIdle()
{
  strip.fill(strip.Color(0, 0, 0), 0, strip.numPixels());
  strip.show();
}

byte * getID()
{
    // The reader will be enabled again after DELAY_BETWEEN_CARDS ms will pass.
    readerDisabled = true;
    uint8_t success = false;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    Serial.println(success ? "Read successful" : "Read failed (not a card?)");

    if (success) {
      // Display some basic information about the card
      Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      
      for (int i = 0; i < 4; i++) readCard[i] = uid[i];
      timeLastCardRead = millis();
      return readCard;
    } else return 0;

}

void stepperTurn(String direction)
{
  digitalWrite(ENABLE, LOW);
  if (direction == "left") digitalWrite(DIR, LOW);
  else digitalWrite(DIR, HIGH);
  for (int i = 0; i < SPT; i++)
  {
    digitalWrite(STEP, HIGH);
    delay(10);
    digitalWrite(STEP, LOW);
    delay(10);
  }
  digitalWrite(ENABLE, HIGH);
}

void setupLeds()
{
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

}

void open(int setDelay)
{
  accessGranted = true;
  Serial.println("Access Granted");
  setGranted();
  stepperTurn("right");
  delay(setDelay);
  stepperTurn("left");
  setIdle();
}

void denied()
{
  Serial.println("Access denied");
  setDenied();
  delay(1000);
  setIdle();
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
    else EEPROM.write(x, 0); // if not write 0 to clear, it takes 3.3mS
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
  EEPROM.commit();
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

bool checkID(byte id[4]) {
  String cardId = "0";
  for (int i = 0; i <= 3; i++) cardId += id[i];

  Serial.print("Card ID: ");
  Serial.println(cardId);

  if ((WiFi.status() == WL_CONNECTED) && client.connected())
  { //Check the current connection status
    response = false;

    char tempvar [cardId.length()];
    accessGranted = false;
    
    cardId.toCharArray(tempvar,cardId.length());

    client.publish("door/card", tempvar);

    // Wait 3 seconds for a response
    unsigned long previousMillis = millis(); 
    
    do
    {
      client.loop();
      if (millis() - previousMillis > timeout) break;
    } while (!response);
    Serial.println("Ended while response: " + response);
    if (response)
    {
      if (accessGranted)
      {   
        accessGranted = false;
        response = false;

        if(!findID(id)) writeID(id);
        return true;
      } else {
        deleteID(id);
        return false;
      }
    }
  }

  Serial.println("No response from mqtt looking in EEPROM");
  if (findID(id))
  {
    Serial.println("Found id in EEPROM");
    open(openTime);
    return true;
  }
  else
    Serial.println("Could not find id in EEPROM");
    return false;
}

void callback(char* topic, byte* message, unsigned int length) {
  response = true;
  Serial.println();
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (strcmp (topic, "door/open") == 0){
    if (messageTemp == "true")
    {
      open(openTime);
      accessGranted = true;
      
    }else if (messageTemp == "false"){
      deleteID(readCard);
    }
    
  }else if (messageTemp == "")
  {
    
  }
}

void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  Serial.println("Waiting for an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void setup() {
  Serial.begin(115200);

  pinMode(DIR, OUTPUT); //Stepper outputs
  pinMode(STEP, OUTPUT);
  pinMode(ENABLE, OUTPUT);

  strip.begin();
  strip.setBrightness(100);
  strip.show();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  digitalWrite(ENABLE, HIGH); //Turn stepper motor off

  nfc.begin();
  nfc.SAMConfig();

  EEPROM.begin(EEPROM_SIZE);

  delay(4000);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  
  Serial.println("Connected to the WiFi network");
  AsyncElegantOTA.begin(&server, SSID, PASS);
  server.begin();

  while (!client.connected())
  {
    client.connect("doorESP23", MQTT_USER, MQTT_PASS); 
    Serial.println("trying to connect to mqtt");
  }
  Serial.println("mqtt is connected");

  client.subscribe("door/open");

  //wipe EEPROM if button pressed
  if (digitalRead(wipeB) == LOW)
  { // when button pressed pin should get low, button connected to ground
    // TODO: Add led Animation 
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 15 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    delay(5000); // Give user enough time to cancel operation
    if (digitalRead(wipeB) == LOW)
      wipe();
    else
    {
      Serial.println(F("Wiping Cancelled"));
      // TODO: Add led animation
    }
  }

  setIdle();
  startListeningToNFC();
}

void loop() {
  successRead = 0;
  do
  {
    client.loop(); // mqtt

    if (readerDisabled) {
      if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
        readerDisabled = false;
        startListeningToNFC();
      }
    } else {
      irqCurr = digitalRead(PN532_IRQ);
      // When the IRQ is pulled low - the reader has got something for us.
      if (irqCurr == LOW && irqPrev == HIGH) successRead = getID();
      irqPrev = irqCurr;
    }
    
  } while (successRead == 0);
  if (!checkID(successRead)) denied();
  else Serial.println("return");
}