/*
* ESPDEV.h
*
* Project: ESPDEV - An ESP8266 development platform
* Based on...
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
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

#ifndef ESPDEV_H_
#define ESPDEV_H_

const char VERSION[] = "0.3";
const char BUILD_DATE[] = __DATE__;

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncE131.h>
#include <ArduinoJson.h>

#define HTTP_PORT       80      /* Default web server port */
#define CLIENT_TIMEOUT  15      /* In station/client mode try to connection for 15 seconds */
#define AP_TIMEOUT      60      /* In AP mode, wait 60 seconds for a connection or reboot */
#define REBOOT_DELAY    100     /* Delay for rebooting once reboot flag is set */
#define LOG_PORT        Serial  /* Serial port for console logging */


// Configuration file params
#define CONFIG_MAX_SIZE 4096    /* Sanity limit for config file */

// Configuration structure
typedef struct {
    /* Device */
    String      dname;          /* Device Name */
    String      id;             /* Device Identifier */
    uint8_t     lampid;         /* LAMP Identifier */
    uint8_t     vled; 		/* Virtual LED index (color) */

    /* Network */
    String      ssid;
    String      passphrase;
    String      hostname;
    String 	password;       /* ADMIN password */
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    bool        dhcp;           /* Use DHCP? */
    bool        ap_fallback;    /* Fallback to AP if fail to associate? */
    uint32_t    sta_timeout;    /* Timeout when connection as client (station) */
    uint32_t    ap_timeout;     /* How long to wait in AP mode with no connection before rebooting */

    /* Door Server */
    bool    sltxt; // Slack Text Enable/Disable
    String  slwht; // Slack WebHook TEXT
    bool    slapi; // Slack Lamp Enable/Disable
    String  slwha; // Slack WebHook API
    String  slfp;  // Slack FingerPrint

    bool     udp;  // UDP Enable
    uint16_t port; // UDP Port
    uint8_t  uip[4];// IP Address to send to
} config_t;

// Forward Declarations
void serializeConfig(String &jsonString, bool pretty = false, bool creds = false);
void dsNetworkConfig(const JsonObject &json);
void dsDeviceConfig(const JsonObject &json);
void saveConfig();

void connectWifi();
void onWifiConnect(const WiFiEventStationModeGotIP &event);
void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event);


#endif  // ESPIXELSTICK_H_
