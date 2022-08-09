/*
* wshandler.h
*
* Project: ESPixelStick - An ESP8266 development platform
*
* Copyright (c) 2016 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#ifndef WSHANDLER_H_
#define WSHANDLER_H_

extern config_t     config;     // Current configuration
extern bool         reboot;     // Reboot flag

extern const char CONFIG_FILE[];

// Door States - move out of wshandler
// Door states (corresponds to blink codes of the Alert LED module)
#define DOOR_CLOSED  (0)
#define DOOR_OPEN    (1)
#define DOOR_CLOSING (2)
#define DOOR_OPENING (3)
#define DOOR_UNKNOWN (4)


extern bool timer_active;
extern bool trigger_relay;
extern uint8_t door_state;
String processor(const String& var); // External declaration

/*
  Packet Commands
    E1 - Get Elements

    G1 - Get Config
    G2 - Get Config Status
    G3 - Get Door Status

    S1 - Set Network Config
    S2 - Set Device Config

    XJ - Get RSSI,heap,uptime stats in json
    X1 - Toggle Door Relay
    X6 - Reboot
*/

EFUpdate efupdate;
uint8_t * WSframetemp;
uint8_t * confuploadtemp;

void sendDoorState(AsyncWebSocketClient *client) {
   DynamicJsonDocument json(1024);
   json["door_state"] = processor("STATE");

   String response;
   serializeJson(json, response);
   client->text("G3" + response);
}

void procX(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case 'J': {

            DynamicJsonDocument json(1024);

            // system statistics
            JsonObject system = json.createNestedObject("system");
            system["rssi"] = (String)WiFi.RSSI();
            system["freeheap"] = (String)ESP.getFreeHeap();
            system["uptime"] = (String)millis();

            String response;
            serializeJson(json, response);
            client->text("XJ" + response);
            break;
        }

        case '6':  // Init 6 baby, reboot!
            reboot = true;
            break;
        case '1': // Toggle Door Relay
            if (!timer_active) {
              trigger_relay= true;       
              if (door_state == DOOR_CLOSED) 
                door_state=DOOR_OPENING;
              if (door_state == DOOR_OPEN) 
                door_state=DOOR_CLOSING;
              sendDoorState(client);
            }
            break;
    }
}

void procE(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1':
            // Create buffer and root object
            DynamicJsonDocument json(1024);

            String response;
            serializeJson(json, response);
            client->text("E1" + response);
            break;
    }
}

void procG(uint8_t *data, AsyncWebSocketClient *client) {
    switch (data[1]) {
        case '1': {
            String response;
            serializeConfig(response, false, true);
            client->text("G1" + response);
            break;
        }

        case '2': {
            // Create buffer and root object
            DynamicJsonDocument json(1024);

            json["ssid"] = (String)WiFi.SSID();
            if (WiFi.hostname().isEmpty()){
               Serial.println("hostname was empty");
               json["hostname"] = config.hostname.c_str();
            } else {
               Serial.print("Using WIFI hostname:");
               Serial.println(WiFi.hostname());
               json["hostname"] = (String)WiFi.hostname();
            }
            json["ip"] = WiFi.localIP().toString();
            json["mac"] = WiFi.macAddress();
            json["version"] = (String)VERSION;
            json["built"] = (String)BUILD_DATE;
            json["flashchipid"] = String(ESP.getFlashChipId(), HEX);
            json["usedflashsize"] = (String)ESP.getFlashChipSize();
            json["realflashsize"] = (String)ESP.getFlashChipRealSize();
            json["freeheap"] = (String)ESP.getFreeHeap();

            String response;
            serializeJson(json, response);
            client->text("G2" + response);
            break;
        }

        case '3': {
           sendDoorState(client);
        }

    }
}

void procS(uint8_t *data, AsyncWebSocketClient *client) {

    DynamicJsonDocument json(2048);
    DeserializationError error = deserializeJson(json, reinterpret_cast<char*>(data + 2));

    if (error) {
        LOG_PORT.println(F("*** procS(): Parse Error ***"));
        LOG_PORT.println(reinterpret_cast<char*>(data));
        return;
    }

    bool reboot = false;
    switch (data[1]) {
        case '1':   // Set Network Config
            dsNetworkConfig(json.as<JsonObject>());
            saveConfig();
            client->text("S1");
            break;
        case '2':   // Set Device Config
            dsDeviceConfig(json.as<JsonObject>());
            saveConfig();

            if (reboot)
                client->text("S1");
            else
                client->text("S2");
            break;
    }
}

void handle_fw_upload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Upload Started: "));
        LOG_PORT.println(filename.c_str());
        efupdate.begin();
    }

    if (!efupdate.process(data, len)) {
        LOG_PORT.print(F("*** UPDATE ERROR: "));
        LOG_PORT.println(String(efupdate.getError()));
    }

    if (efupdate.hasError())
        request->send(200, "text/plain", "Update Error: " +
                String(efupdate.getError()));

    if (final) {
        LOG_PORT.println(F("* Upload Finished."));
        efupdate.end();
        SPIFFS.begin();
        saveConfig();
        reboot = true;
    }
}

void handle_config_upload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    static File file;
    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Config Upload Started: "));
        LOG_PORT.println(filename.c_str());

        if (confuploadtemp) {
          free (confuploadtemp);
          confuploadtemp = nullptr;
        }
        confuploadtemp = (uint8_t*) malloc(CONFIG_MAX_SIZE);
    }

    LOG_PORT.printf("index %d len %d\n", index, len);
    memcpy(confuploadtemp + index, data, len);
    confuploadtemp[index + len] = 0;

    if (final) {
        int filesize = index+len;
        LOG_PORT.print(F("* Config Upload Finished:"));
        LOG_PORT.printf(" %d bytes", filesize);

        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, reinterpret_cast<char*>(confuploadtemp));

        if (error) {
            LOG_PORT.println(F("*** Parse Error ***"));
            LOG_PORT.println(reinterpret_cast<char*>(confuploadtemp));
            request->send(500, "text/plain", "Config Update Error." );
        } else {
            dsNetworkConfig(json.as<JsonObject>());
            dsDeviceConfig(json.as<JsonObject>());
            saveConfig();
            request->send(200, "text/plain", "Config Update Finished: " );
//          reboot = true;
        }

        if (confuploadtemp) {
            free (confuploadtemp);
            confuploadtemp = nullptr;
        }
    }
}


void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
        AwsEventType type, void * arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_DATA: {
            AwsFrameInfo *info = static_cast<AwsFrameInfo*>(arg);
            if (info->opcode == WS_TEXT) {
                switch (data[0]) {
                    case 'X':
                        procX(data, client);
                        break;
                    case 'E':
                        procE(data, client);
                        break;
                    case 'G':
                        procG(data, client);
                        break;
                    case 'S':
                        procS(data, client);
                        break;
                }
            } else {
                LOG_PORT.println(F("-- binary message --"));
            }
            break;
        }
        case WS_EVT_CONNECT: {
            int i;
              LOG_PORT.print(F("* WS Connect - "));
              LOG_PORT.println(client->id());
            }
            break;
        case WS_EVT_DISCONNECT:{
            int i;
              LOG_PORT.print(F("* WS Disconnect - "));
              LOG_PORT.println(client->id());
            }
            break;
        case WS_EVT_PONG:
            LOG_PORT.println(F("* WS PONG *"));
            break;
        case WS_EVT_ERROR:
            LOG_PORT.println(F("** WS ERROR **"));
            break;
    }
}

#endif
