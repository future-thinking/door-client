// Rename this file to Configuration.hpp and change all the values to your liking

#define openTime 5000 // Time before locking the door again
#define wipeB 3 // Pin for EEPROM wipe button set low to wipe

// PN532 Pins
#define PN532_IRQ 19
#define PN532_RESET 18

// Stepper
#define SPT 200     // Steps per turn
#define DIR 25      // DIR Pin
#define STEP 26     // STEP Pin
#define ENABLE 13   // ENABLE Pin

#define EEPROM_SIZE 256

#define LED_PIN 5
#define LED_COUNT 116

#define MQTT_SERVER ""  // The IP of your MQTT Broker 
#define MQTT_USER ""    // The Authentication for MQTT
#define MQTT_PASS ""

#define SSID "Your Wifi SSID"       // This will also be used to access the OTA Webinterface
#define PASS "Your Wifi Password"