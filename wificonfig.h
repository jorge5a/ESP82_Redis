#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "config.h"
#define ESP_getChipId()   (WiFi.macAddress())

extern char redisIP[15];
extern char redisPW[15];
extern char redisModule[3];
void setupwifi();
void resetwifi();
void check_status();
