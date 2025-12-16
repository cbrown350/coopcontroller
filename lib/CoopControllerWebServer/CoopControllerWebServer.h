#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <ESPAsyncWebServer.h>

#include "SensorManager.h"
#include "PumpController.h"
#include "BuzzerController.h"
#include "DoorController.h"
#include "LightController.h"
#include "SunriseSunset.h"
#include "WifiController.h"
#include "SettingsManager.h"

// Define SPIFFS as LittleFS
#define SPIFFS LittleFS

class CoopControllerWebServer
{
   private:
    AsyncWebServer server;

   public:
    explicit CoopControllerWebServer(uint16_t port = 80);
    void begin(SensorManager& tempSensor,
            PumpController& pumpController,
            BuzzerController& buzzerController,
            DoorController& doorController,
            LightController& lightController,
            const WifiController& wifiController,
            SunriseSunsetCalculator& sunriseSunset);
    void loop() const;
};

#endif  // WEB_SERVER_H