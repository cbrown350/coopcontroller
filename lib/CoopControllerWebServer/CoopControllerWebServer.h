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

/**
 * @brief Web server for chicken coop controller
 *
 * Provides HTTP REST API and web interface for monitoring and controlling
 * the chicken coop system. Serves static assets for the web UI and handles
 * API endpoints for all controllers (pump, light, door, buzzer, sensors).
 *
 * Features:
 * - RESTful JSON API for all system components
 * - Static file serving for web UI
 * - Real-time status monitoring
 * - Configuration management
 * - OTA update support via ElegantOTA
 *
 * The web server runs on the specified port (default 80) and provides
 * endpoints for getting/setting system parameters and controller states.
 */
class CoopControllerWebServer
{
    private:
        IHAL* hal;          ///< Hardware abstraction layer interface
        uint16_t port;      ///< HTTP server port number

   public:
    /**
     * @brief Constructor for CoopControllerWebServer
     *
     * Initializes the web server with HAL interface and port configuration.
     * Must call begin() before use.
     *
     * @param hal Pointer to hardware abstraction layer
     * @param port TCP port for HTTP server (default: 80)
     */
    explicit CoopControllerWebServer(IHAL* hal, uint16_t port = 80);

    /**
     * @brief Initialize web server with controller references
     *
     * Registers all controllers and sets up HTTP endpoints.
     * Creates API routes for monitoring and controlling each component.
     * Configures static file serving and OTA updates.
     *
     * @param tempSensor Reference to temperature sensor manager
     * @param pumpController Reference to pump controller
     * @param buzzerController Reference to buzzer controller
     * @param doorController Reference to door controller
     * @param lightController Reference to light controller
     * @param wifiController Reference to WiFi controller
     * @param sunriseSunset Reference to sunrise/sunset calculator
     */
    void begin(SensorManager& tempSensor,
            PumpController& pumpController,
            BuzzerController& buzzerController,
            DoorController& doorController,
            LightController& lightController,
            const WifiController& wifiController,
            SunriseSunsetCalculator& sunriseSunset);

    /**
     * @brief Process web server events (call in main loop)
     *
     * Handles async web server operations, client requests, and OTA updates.
     * Should be called frequently in the main loop.
     */
    void loop() const;
};

#endif  // WEB_SERVER_H
