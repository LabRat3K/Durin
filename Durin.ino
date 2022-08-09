/*
  During.ino (the big door Server)

  based on
  ESPDEV.ino

  Project: ESPDEV - An ESP8266 generic platform 
  Copyright (c) 2022 Andrew Williams
  http://www.ratsnest.ca
  Based on..
  Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
  Copyright (c) 2016 Shelby Merrick
  http://www.forkineye.com

   This program is provided free for you to use in any way that you wish,
   subject to the laws and regulations where you are using it.  Due diligence
   is strongly suggested before using this code.  Please give credit where due.

   The Author makes no warranty of any kind, express or implied, with regard
   to this program or the documentation contained in this document.  The
   Author shall not be liable in any event for incidental or consequential
   damages in connection with, or arising out of, the furnishing, performance
   or use of these programs.

*/
extern "C" {
#include <user_interface.h>
}


/*****************************************/
/*        BEGIN - Configuration          */
/*****************************************/

/* Fallback configuration if config.json is empty or fails */
/* Leave Blank .. we can fill these in via 1st boot admin  */
const char ssid[] = "";
const char passphrase[] = "";
const char *dname = "durin";

// Youre fallback login password (can leave blank)
//const char *http_password = "";
const char *http_password = "SummoSecretum";

// NTP use - define TIMEZONE for this device
const char *TZstr="EST+5EDT,M3.2.0/2,M11.1.0/2";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <Hash.h>
#include <SPI.h>
#include "ESPDEV.h"
#include "EFUpdate.h" // Not sure we need this here vs in wshandler.h
#include "wshandler.h"

#include "lib/webSockets/WebSocketsClient.h"
#include "lib/webSockets/WebSocketsClient.cpp"
#include "lib/webSockets/WebSockets.cpp"

/////////////////////////////////////////////////////////
//
//  Globals
//
/////////////////////////////////////////////////////////

// Configuration file
const char CONFIG_FILE[] = "/config.json";
const char *http_username = "admin";

config_t            config;         // Current configuration
bool                reboot = false; // Reboot flag
AsyncWebServer      web(HTTP_PORT); // Web Server
AsyncWebSocket      ws("/ws");      // Web Socket Plugin
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
Ticker              wifiTicker;     // Ticker to handle WiFi

IPAddress           ourLocalIP;
IPAddress           ourSubnetMask;

WiFiClientSecure    client;
HTTPClient          httpsClient;

#define LED_WIFI 2

#define LED_OFF    0x0000
#define BLINK_1HZ  0xFF00
#define BLINK_2HZ  0xF0F0
#define BEAT       ~(0x0033)
#define FLICKER    ~(0x3030)
// Avoid using this, so that we have visual that the main loop is operational
#define SOLID      0xFFFF


#ifdef LED_WIFI
uint16_t led_state_wifi=LED_OFF;
uint32_t led_timeout =0;
uint16_t led_mask = 0x0001;
#endif

const int gpio_relay = 4;  // GPIO's used by the esp82666 replay module
const int gpio_door  = 5; 

bool timer_active = false;
bool trigger_relay = false;

uint8_t door_state = DOOR_UNKNOWN;

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
void initWifi();
void initWeb();
void updateConfig();

// Radio config
RF_PRE_INIT() {
    //wifi_set_phy_mode(PHY_MODE_11G);    // Force 802.11g mode
    system_phy_set_powerup_option(31);  // Do full RF calibration on power-up
    system_phy_set_max_tpw(82);         // Set max TX power
}


void setup() {
    // Configure SDK params
    wifi_set_sleep_type(NONE_SLEEP_T);

    pinMode(gpio_relay, OUTPUT);
    digitalWrite(gpio_relay, LOW);

    pinMode(gpio_door, INPUT_PULLUP);

#if defined (LED_WIFI)
    pinMode(LED_WIFI,OUTPUT);
    digitalWrite(LED_WIFI, HIGH);
#endif
    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

    LOG_PORT.print("\n");
    LOG_PORT.print(dname);
    LOG_PORT.print(F(" v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.print(F(" ("));
    for (uint8_t i = 0; i < strlen_P(BUILD_DATE); i++)
        LOG_PORT.print((char)(pgm_read_byte(BUILD_DATE + i)));
    LOG_PORT.println(")");
    LOG_PORT.println(ESP.getFullVersion());

    // Enable SPIFFS
    if (!SPIFFS.begin())
    {
        LOG_PORT.println("File system did not initialise correctly");
    }
    else
    {
        LOG_PORT.println("File system initialised");
    }

    FSInfo fs_info;
    if (SPIFFS.info(fs_info))
    {
        LOG_PORT.print("Total bytes in file system: ");
        LOG_PORT.println(fs_info.usedBytes);
        LOG_PORT.print("Space:");
        LOG_PORT.println(fs_info.totalBytes - fs_info.usedBytes);

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
          LOG_PORT.print(dir.fileName());
          File f = dir.openFile("r");
          LOG_PORT.println(f.size());
        }
    }
    else
    {
        LOG_PORT.println("Failed to read file system details");
    }

    // Load configuration from SPIFFS and set Hostname
    loadConfig();

    updateConfig();

    // Setup WiFi Handlers
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);

    // Fallback to default SSID and passphrase if we fail to connect
    initWifi();
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PORT.println(F("*** Timeout - Reverting to default SSID ***"));
        config.ssid = ssid;
        config.passphrase = passphrase;
        initWifi();
#if defined(LED_WIFI)
        led_state_wifi = BLINK_1HZ;
#endif
    }

    // If we fail again, go SoftAP or reboot
    if (WiFi.status() != WL_CONNECTED) {
        if (config.ap_fallback) {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode(WIFI_AP);
            String ssid = String(config.hostname);
            WiFi.softAP(ssid.c_str());
            ourLocalIP = WiFi.softAPIP();
            ourSubnetMask = IPAddress(255,255,255,0);
#if defined(LED_WIFI)
            led_state_wifi = BEAT;
#endif
        } else {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart();
        }
    }

    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

    // NTP attempt to get current time
    configTime(TZstr,"pool.ntp.org","time.nist.gov");

    // Print Local Time:
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    LOG_PORT.println(asctime(timeinfo));

    LOG_PORT.print("IP : ");
    LOG_PORT.println(ourLocalIP);
    LOG_PORT.print("Subnet mask : ");
    LOG_PORT.println(ourSubnetMask);

    // Configure and start the web server
    initWeb();
    client.setFingerprint(config.slfp.c_str());
}

/////////////////////////////////////////////////////////
//
//  WiFi Section
//
/////////////////////////////////////////////////////////

void initWifi() {
  // Switch to station mode and disconnect just in case
  WiFi.mode(WIFI_STA);

  // Hostname to be assigned AFTER WiFi.mode (found through trial/error)
  if (config.hostname) {
        WiFi.hostname(config.hostname);
        LOG_PORT.print("Assigning WiFi.hostname:");
        LOG_PORT.print(config.hostname);
  }

  WiFi.disconnect();

  if (!config.ssid.isEmpty()) {
     connectWifi();
     uint32_t timeout = millis();
     while (WiFi.status() != WL_CONNECTED) {
       LOG_PORT.print(".");
       delay(500);
       if (millis() - timeout > (1000 * config.sta_timeout) ) {
         LOG_PORT.println("");
         LOG_PORT.println(F("*** Failed to connect ***"));
         break;
       }
     }
  }
}

void reconnectWifi() {
  WiFi.reconnect();
}

void connectWifi() {
  delay(secureRandom(100, 500));

  LOG_PORT.println("");
  LOG_PORT.print(F("Connecting to "));
  LOG_PORT.print(config.ssid);
  LOG_PORT.print(F(" as "));
  LOG_PORT.println(config.hostname);

  WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
  if (config.dhcp) {
    LOG_PORT.print(F("Connecting with DHCP"));
  } else {
    // We don't use DNS, so just set it to our gateway
    WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
               );
    LOG_PORT.print(F("Connecting with Static IP"));
  }
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
  LOG_PORT.println("");
  LOG_PORT.print(F("Connected with IP: "));
  LOG_PORT.println(WiFi.localIP());

  ourLocalIP = WiFi.localIP();
  ourSubnetMask = WiFi.subnetMask();

  // Setup mDNS / DNS-SD
  //TODO: Reboot or restart mdns when config.id is changed?
  String chipId = String(ESP.getChipId(), HEX);
  MDNS.setInstanceName(String(config.id + " (" + chipId + ")").c_str());
  if (MDNS.begin(config.hostname.c_str())) {
    MDNS.addService("http", "tcp", HTTP_PORT);
  } else {
    LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
  }
}

void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event) {
  LOG_PORT.println(F("*** WiFi Disconnected ***"));

  wifiTicker.once(2, reconnectWifi);
}

String processor(const String& var){
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String doorStateValue = readDoorState();
    buttons+= "<p><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + doorStateValue + "><span class=\"slider\"></span></label></p>";
    return buttons;
  }
  if (var == "STATE"){
        switch (door_state) {
          case DOOR_CLOSED:return "CLOSED";break;
          case DOOR_OPENING:return "OPENING";break;
          case DOOR_CLOSING:return "CLOSING";break;
          case DOOR_OPEN:return "OPEN";break;
          case DOOR_UNKNOWN: 
          default: return "UNKNOWN";break;
        }
  }
  return String();
}

String readDoorState(){
  if(digitalRead(gpio_door)){
    return "checked";
  }
  return "";
}
/////////////////////////////////////////////////////////
//
//  Web Section
//
/////////////////////////////////////////////////////////


// Configure and start the web server
void initWeb() {
  // Handle OTA update from asynchronous callbacks
  Update.runAsync(true);

  // Add header for SVG plot support?
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // Setup WebSockets
  ws.onEvent(wsEvent);
  web.addHandler(&ws);

  // Heap status handler
  web.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(http_username, http_password))
       return request->requestAuthentication();
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // JSON Config Handler
  web.on("/conf", HTTP_GET, [](AsyncWebServerRequest * request) {
    String jsonString;

    if (!request->authenticate(http_username, http_password))
       return request->requestAuthentication();

    serializeConfig(jsonString, true);
    request->send(200, "text/json", jsonString);
  });

  // Firmware upload handler - only in station mode
  web.on("/updatefw", HTTP_POST, [](AsyncWebServerRequest * request) {

    if (!request->authenticate(http_username, http_password))
       return request->requestAuthentication();

    ws.textAll("X6");
  }, handle_fw_upload).setFilter(ON_STA_FILTER);

  // Static Handler
  web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html").setAuthentication(http_username, http_password);

  // Raw config file Handler - but only on station
  //web.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER).setAuthentication(http_username, http_password);

  web.onNotFound([](AsyncWebServerRequest * request) {

    if (!request->authenticate(http_username, http_password))
       return request->requestAuthentication();

    request->send(404, "text/plain", "Page not found");
  });

  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");

  // Config file upload handler - only in station mode
  web.on("/config", HTTP_POST, [](AsyncWebServerRequest * request) {
    ws.textAll("X6");
  }, handle_config_upload).setFilter(ON_STA_FILTER);

  web.begin();

  LOG_PORT.print(F("- Web Server started on port "));
  LOG_PORT.println(HTTP_PORT);
}

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

// Configuration Validations
void validateConfig() {
   // Device specific config valdation/bounds checking    
}

void updateConfig() {
    // Validate first
    validateConfig();

}

// De-Serialize Network config
void dsNetworkConfig(const JsonObject &json) {
    if (json.containsKey("network")) {
        JsonObject networkJson = json["network"];

        // Fallback to embedded ssid and passphrase if null in config
        config.ssid = networkJson["ssid"].as<String>();
        if (!config.ssid.length())
            config.ssid = ssid;

        config.passphrase = networkJson["passphrase"].as<String>();
        if (!config.passphrase.length())
            config.passphrase = passphrase;

        // Network
        for (int i = 0; i < 4; i++) {
            config.ip[i] = networkJson["ip"][i];
            config.netmask[i] = networkJson["netmask"][i];
            config.gateway[i] = networkJson["gateway"][i];
        }
        config.dhcp = networkJson["dhcp"];
        config.sta_timeout = networkJson["sta_timeout"] | CLIENT_TIMEOUT;
        if (config.sta_timeout < 5) {
            config.sta_timeout = 5;
        }

        config.ap_fallback = networkJson["ap_fallback"];
        config.ap_timeout = networkJson["ap_timeout"] | AP_TIMEOUT;
        if (config.ap_timeout < 15) {
            config.ap_timeout = 15;
        }

        // Generate default hostname if needed
        config.hostname = networkJson["hostname"].as<String>();
    }
    else {
        LOG_PORT.println("No network settings found.");
    }

    if (!config.hostname.length()) {
        config.hostname = String(dname) + String("-") + String(ESP.getChipId(), HEX);
    }
}

// De-serialize Device Config
void dsDeviceConfig(const JsonObject &json) {
    if (json.containsKey("device")) {
        config.dname  = json["device"]["dname"].as<String>();
        config.id     = json["device"]["id"].as<String>();
        config.vled   = json["device"]["vled"];
        config.lampid = json["device"]["lampid"];
    }
    else
    {
        LOG_PORT.println("No device settings found.");
    }

    if (json.containsKey("slack")) {
        config.sltxt = json["slack"]["txt"]["enable"];
        config.slwht = json["slack"]["txt"]["webhook"].as<String>();
        config.slapi = json["slack"]["lamp"]["enable"];
        config.slwha = json["slack"]["lamp"]["webhook"].as<String>();
        config.slfp =  json["slack"]["fingerprint"].as<String>();
    } else {
        LOG_PORT.println("No SLACK settings found.");
    }

    if (json.containsKey("udp")) {
        config.udp = json["udp"]["enable"];
        config.port = json["udp"]["port"];
        for (int i = 0; i < 4; i++) {
            config.uip[i] = json["udp"]["uip"][i];
        }
    } else {
        LOG_PORT.println("No UDP settings found.");
    }
}

// Load configugration JSON file
void loadConfig() {
    // Zeroize Config struct
    memset(&config, 0, sizeof(config));

    // Load CONFIG_FILE json. Create and init with defaults if not found
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        config.ssid = ssid;
        config.passphrase = passphrase;
        config.password   = http_password;
        config.hostname = "Durin-" + String(ESP.getChipId(), HEX);
        config.ap_fallback = true;
       /* If not found .. LAMP ID = 0, vLED=3 */
        config.id = "No Config Found";
        config.dname="Unknown";
        config.lampid = 0;
        config.vled = 3;
        config.sltxt = false;
        config.slapi = false;
        saveConfig();
    } else {
        // Parse CONFIG_FILE json
        size_t size = file.size();
        if (size > CONFIG_MAX_SIZE) {
            LOG_PORT.println(F("*** Configuration File too large ***"));
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);

        DynamicJsonDocument json(2048);
        DeserializationError error = deserializeJson(json, buf.get());
        if (error) {
            LOG_PORT.println(F("*** Configuration File Format Error ***"));
            return;
        }

        dsNetworkConfig(json.as<JsonObject>());
        dsDeviceConfig(json.as<JsonObject>());

        LOG_PORT.println(F("- Configuration loaded."));
    }

    // Validate it
    validateConfig();
}

// Serialize the current config into a JSON string
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    // Create buffer and root object
    DynamicJsonDocument json(2048);

    // Device
    JsonObject device = json.createNestedObject("device");
       device["dname"]=config.dname;
       device["id"] = config.id;
       device["vled"] = config.vled;
       device["lampid"] = config.lampid;

    JsonObject slack = json.createNestedObject("slack");
    
    JsonObject whtxt = slack.createNestedObject("txt");
       whtxt["enable"] = config.sltxt;
       if (creds) {
          whtxt["webhook"] = config.slwht;
       }

    JsonObject whapi = slack.createNestedObject("lamp");
       whapi["enable"] = config.slapi;
       if (creds) {
          whapi["webhook"] = config.slwha;
       }
       
       slack["fingerprint"] = config.slfp;

    JsonObject udp = json.createNestedObject("udp");
       udp["enable"] = config.udp;
       udp["port"]    = config.port;
       JsonArray uip = udp.createNestedArray("uip");
       for (int i = 0; i < 4; i++) {
           uip.add(config.uip[i]);
       }

    // Network
    JsonObject network = json.createNestedObject("network");
       network["ssid"] = config.ssid.c_str();
       if (creds) {
           network["passphrase"] = config.passphrase.c_str();
           device["password"] = config.password.c_str();
       }

       network["hostname"] = config.hostname.c_str();
       JsonArray ip = network.createNestedArray("ip");
       JsonArray netmask = network.createNestedArray("netmask");
       JsonArray gateway = network.createNestedArray("gateway");
       for (int i = 0; i < 4; i++) {
           ip.add(config.ip[i]);
           netmask.add(config.netmask[i]);
           gateway.add(config.gateway[i]);
       }
       network["dhcp"] = config.dhcp;
       network["sta_timeout"] = config.sta_timeout;

       network["ap_fallback"] = config.ap_fallback;
       network["ap_timeout"] = config.ap_timeout;


   // Pretty or not
    if (pretty)
        serializeJsonPretty(json, jsonString);
    else
        serializeJson(json, jsonString);
}

// Save configuration JSON file
void saveConfig() {
    // Update Config
    updateConfig();

    // Serialize Config
    String jsonString;
    serializeConfig(jsonString, true, true);

    // Save Config
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

#if defined(LED_WIFI) 
void blink_led() {
  // Divide timeout into 16 slices
  if (millis()-led_timeout > (2000/16)) {
     led_timeout = millis();
#if defined(LED_WIFI)
     digitalWrite(LED_WIFI,(led_state_wifi & led_mask)>0);
#endif
     led_mask<<=1;
     if (!led_mask)
       led_mask = 1;
  }
}
#endif


// Slack Bot Integration ---------
// set to true for dev or debugging
#define DEBUG_SERIAL_PRINT true


/**
  Establishes a webhook connection to Slack:
*/
AsyncUDP udp;
void clientNotify(uint8_t lamp_state) {

   // Send Human Readable SLACK notification
   if (config.sltxt) {
       String data = "{\"text\":\"Durin "+config.dname+":"+processor("STATE")+"\"}";
       httpsClient.begin(client, config.slwht);
       httpsClient.addHeader("Content-Type", "application/json");
       httpsClient.POST(data);
       httpsClient.end();
   }
  
   // Send LAMP update (machine2machine) 
   if (config.slapi){
       char data[40];
        sprintf( data,"{\"text\":\"*0,%d,%d,%d\"}",config.lampid,config.vled,lamp_state);
        httpsClient.begin(client, config.slwha);
        httpsClient.addHeader("Content-Type", "application/json");
        httpsClient.POST(data);
        httpsClient.end();
   }

   if (config.udp) {
      char data[20];
      sprintf( data,"*0,%d,%d,%d",config.lampid, config.vled, lamp_state);
      if (udp.connect(IPAddress(config.uip[0],config.uip[1],config.uip[2],config.uip[3]),config.port)) {
         udp.write((uint8_t*)data,strlen(data));
      } else{
         Serial.println("Error on TX of UDP payload\n");
      }
   }

   #ifdef DEBUG_SERIAL_PRINT
       Serial.print("Status Update:");
       Serial.println(processor("STATE"));
   #endif
}


/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////

uint32_t timeout = 0;
uint32_t doorOpenTimer= 0;

void loop() {
    uint8_t prevState = door_state;

    ws.cleanupClients();
    // Reboot handler
    if (reboot) {
        delay(REBOOT_DELAY);
        ESP.restart();
    }

#if defined(LED_WIFI) 
    blink_led();
#endif

    // Trigger Command detected - toggle relay for 1 second
    if (trigger_relay) {
        digitalWrite(gpio_relay, HIGH);
        clientNotify(door_state);
        delay(1000);
        digitalWrite(gpio_relay, LOW);
        timeout = millis();
        timer_active = true;
        trigger_relay = false;  
    }

    if (timer_active) {
      if (millis()-timeout < 15000) {// ** need to time the door open procedure
        return;
      }
      else 
        timer_active = false;
    }
 

    if (digitalRead(gpio_door)) {
      if (door_state == DOOR_CLOSED) {
        door_state = DOOR_OPENING;
        timeout = millis();
        timer_active= true;
      } else {
         door_state = DOOR_OPEN;
      }
    } else {
      door_state = DOOR_CLOSED;
      doorOpenTimer = millis();
    }

    if (millis()-doorOpenTimer > (1000*60*60) ) {
      String data = "{\"text\":\"Durin "+config.dname+": DOOR OPEN FOR 60 MINUTES\"}";
      httpsClient.begin(client, config.slwht);
      httpsClient.addHeader("Content-Type", "application/json");
      httpsClient.POST(data);
      httpsClient.end();
      doorOpenTimer = millis();
    }
  

    if (prevState != door_state) {
      { // Duplicate Code .. could use a restructure but this is proof of concept
        DynamicJsonDocument json(1024);
        json["door_state"] = processor("STATE");
  
        String response;
        serializeJson(json, response);
        ws.textAll("G3"+response);
       }
       clientNotify(door_state); // Send Status, Lamp Update, and/or Serial Debug
     }

    // workaround crash - consume incoming bytes on serial port
    if (LOG_PORT.available()) {
      LOG_PORT.read();
    }// LOG PORT AVAILABLE

}
