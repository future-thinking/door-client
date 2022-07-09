# esp32-doorlock

## RFID-doorlock communicating over mqtt with an api to check a database for access control

This is the code for an Esp32 to controll a stepper motor to unlock a door. It scans RFID-tags sends their ID over MQTT to a server and waits for a response in order to unlock a door.

## Features

- use phone to unlock
- status display with neopixels (animations are wip)
- mysql database
- logging of people unlocking the door

## Usage

This is meant to work in combination with the backend [door-access-api](https://github.com/future-thinking/door-access-api), which you will need to deploy on a local server together with a MQTT broker like mosquitto in order for them to communicate.

1. Download this repository
2. Open with VS Code with the PIO plugin installed
3. Copy the contents of Configuration_sample.hpp into Configuration.hpp and change the values according to your setup
4. Connect your board and hit upload and you're done!

## Parts

- ESP32 DOIT DEVKIT V1 (or any other esp32 board)
- PN532 RFID/NFC reader
- A4988 (or any other generic stepper driver)
- Nema17 Stepper
- 12v 3A Power supply
- Neopixel strip of any length (for status display)


The Esp will either wait for a card to be scanned or a message to be posted to a specific mqtt topic.
If it detects a card and is connected to wifi and mqtt it will publish the cards ID on the door/card topic wich the server is subscribed to.
The server then looks up the ID in a MySql database and publish a new message under the door/open topic, either true or false.
After receiving the answer the ESP saves the card to the EEPROM if the answer was true or deltes the ID from EEPROM if the answer is false and the ID exists.
If the esp does't have a connection to either wifi or mqtt and a card is scanned it looks up it's ID in the EEPROM. This way if any of the other components fail the lock will still function.
The esp also reacts to published messages under door/open topic when no card is scanned, this way the door can be opened with a smartphone or other devices loged in to the same mqtt network.

