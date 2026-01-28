#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>

#include "IHAL.h"
#include "SensorManager.h"
#include "PumpController.h"
#include "BuzzerController.h"
#include "DoorController.h"
#include "LightController.h"
#include "SunriseSunset.h"
#include "WifiController.h"
#include "SettingsManager.h"

class CoopControllerWebServer
{
    private:
        IHAL* hal;
        uint16_t port;

   public:
    explicit CoopControllerWebServer(IHAL* hal, uint16_t port = 80);
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
