# Durin Firmware - based on ESPDEV Firmware

Durin - The Portal Server (garage door monitor)


This is the Arduino firmware for the ESP8266 garage door server.  Re-worked to use the WEMOS ESP8266 relay module (off the shelf hardware solution).

## Hardware

Original project used an ESP-01 relay module, but then ported to an ESP-12 based solution that includes input pin for monitoring the door status, as well as relay control to trigger the door opening. 

## Requirements

Along with the Arduino IDE, you'll need the following software to build this project:

- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266
- [Arduino ESP8266 Filesystem Uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [gulp](http://gulpjs.com/) - Build system required to process web sources.  Refer to the html [README](html/README.md) for more information.

The following libraries are required:

Extract the folder in each of these zip files and place it in the "library" folder under your arduino environment

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncUDP](https://github.com/me-no-dev/ESPAsyncUDP) - Asynchronous UDP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library

## Important Notes on Compiling and Flashing

- In order to upload your code to the ESP8266 you must put it in flash mode and then take it out of flash mode to run the code. To place your ESP8266 in flash mode your GPIO-0 pin must be connected to ground.
- Device mode is now a compile time option to set your device type and is configured in the top of the main sketch file.  Current options are ```ESPS_MODE_PIXEL``` and ```ESPS_MODE_SERIAL```.  The default is ```ESPS_MODE_PIXEL``` for the ESPixelStick hardware.
- Web pages **must** be processed, placed into ```data/www```, and uploaded with the upload plugin. Gulp will process the pages and put them in ```data/www``` for you. Refer to the html [README](html/README.md) for more information.
- In order to use the upload plugin, the ESP8266 **must** be placed into programming mode and the Arduino serial monitor **must** be closed.
- ESP-01 modules **must** be configured for 1M flash and 128k SPIFFS within the Arduino IDE for OTA updates to work.
- For best performance, set the CPU frequency to 160MHz (Tools->CPU Frequency).  You may experience lag and other issues if running at 80MHz.
- The upload must be redone each time after you rebuild and upload the software

## Supported Outputs

This projected uses SLACK messaging to inform you of the door status, and/or to illuminate an LED in virtual LED alarm lamp. (aka AmonDin beacon).

## Resources

- Firmware: [http://github.com/labrat_3k/Durin](http://github.com/labrat_3k/Durin)

## Credits

- Based on the ESPDEV starting point, which is based on the ESPixelStick project. 
