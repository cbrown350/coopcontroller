#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "SettingsManager.h"

// Define SPIFFS as LittleFS
#define SPIFFS LittleFS

class CoopControllerWebServer
{
   private:
    AsyncWebServer server;

   public:
    CoopControllerWebServer(int port = 80);
    void begin();
    void loop();
};

#endif  // WEB_SERVER_H