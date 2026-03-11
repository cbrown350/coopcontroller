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
#include "HistoricalDataManager.h"
#include "UpdateManager.h"
#include "NotificationManager.h"
#include "TelegramBot.h"
#include "MQTTManager.h"

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
        UpdateManager* updateManager_ = nullptr; ///< OTA update manager (optional)
        NotificationManager* notificationManager_ = nullptr; ///< Notification manager (optional)
        TelegramBot* telegramBot_ = nullptr; ///< Telegram bot for command polling (optional)
        MQTTManager* mqttManager_ = nullptr; ///< MQTT manager for Home Assistant (optional)

        /**
         * @brief Check if HTTP request has valid authentication credentials
         *
         * Validates HTTP Basic Authentication header against configured credentials.
         * If authentication is disabled in settings, all requests are allowed.
         *
         * @param request The incoming HTTP request to validate
         * @return true if authenticated or auth disabled, false otherwise
         */
        bool isAuthenticated(void* request);

        /**
         * @brief Send 401 Unauthorized response with WWW-Authenticate header
         *
         * Returns HTTP 401 response with Basic authentication challenge,
         * prompting browser to request credentials.
         *
         * @param request The incoming HTTP request to respond to
         */
        void sendAuthRequired(void* request);

        /**
         * @brief Decode Base64 encoded string
         *
         * Utility function for decoding HTTP Basic Auth credentials.
         * Implements RFC 4648 Base64 decoding.
         *
         * @param input Base64 encoded string
         * @return Decoded string
         */
        String base64Decode(const String& input);

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
     * @param historyManager Reference to historical data manager
     */
    void begin(SensorManager& tempSensor,
            PumpController& pumpController,
            BuzzerController& buzzerController,
            DoorController& doorController,
            LightController& lightController,
            const WifiController& wifiController,
            SunriseSunsetCalculator& sunriseSunset,
            HistoricalDataManager& historyManager);

    /**
     * @brief Set UpdateManager for OTA update endpoints
     * @param updateManager Pointer to UpdateManager instance
     */
    void setUpdateManager(UpdateManager* updateManager);

    /**
     * @brief Set NotificationManager for notification endpoints
     * @param notificationManager Pointer to NotificationManager instance
     */
    void setNotificationManager(NotificationManager* notificationManager);

    /**
     * @brief Set TelegramBot for bot command and config endpoints
     * @param telegramBot Pointer to TelegramBot instance
     */
    void setTelegramBot(TelegramBot* telegramBot) { telegramBot_ = telegramBot; }

    /**
     * @brief Set MQTTManager for MQTT/Home Assistant endpoints
     * @param mqttManager Pointer to MQTTManager instance
     */
    void setMQTTManager(MQTTManager* mqttManager) { mqttManager_ = mqttManager; }

    /**
     * @brief Process web server events (call in main loop)
     *
     * Handles async web server operations, client requests, and OTA updates.
     * Should be called frequently in the main loop.
     */
    void loop() const;
};

#endif  // WEB_SERVER_H
