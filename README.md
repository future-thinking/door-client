# esp32-doorlock
This is meant to work in combination with the backend door-access-api. They communicate via mqtt.
This will use an Esp32 in combination with a PN532 RFID/NFC reader and a generic stepper motor driver to act as a RFID-Doorlock. 
You can use most standard RFID-Tags with this. 
You can add a neopixel led strip as status display (animations may be added in the future).
The Esp will either wait for a card to be scanned or a message to be posted to a specific mqtt topic.
If it detects a card and is connected to wifi and mqtt it will publish the cards ID on the door/card topic wich the server is subscribed to.
The server then looks up the ID in a MySql database and publish a new message under the door/open topic, either true or false.
After receiving the answer the ESP saves the card to the EEPROM if the answer was true or deltes the ID from EEPROM if the answer is false and the ID exists.

All importent pins and authentication stuff can and need to be changed in the Configuration.hpp see Configuration_sample.hpp for your options.
