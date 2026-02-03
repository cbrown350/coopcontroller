#ifndef EMULATOR_WEB_SERVER_H
#define EMULATOR_WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ElegantOTA.h>
#include "config.h"
#include "EmulatorStateManager.h"
#include "EmulatorSettings.h"

/**
 * @brief Web server for the hardware emulator
 *
 * Provides REST API endpoints for:
 * - Real-time status monitoring
 * - Door control
 * - Water meter control
 * - Manual switch simulation
 * - Fault injection
 * - Settings management
 * - OTA updates
 */
class EmulatorWebServer {
public:
    /**
     * @brief Construct web server on specified port
     * @param port HTTP port (default 80)
     */
    explicit EmulatorWebServer(uint16_t port = 80);

    /**
     * @brief Initialize and start the web server
     * @param stateManager Reference to emulator state manager
     */
    void begin(EmulatorStateManager& stateManager);

    /**
     * @brief Process OTA and other async tasks
     * Call from main loop
     */
    void loop();

private:
    AsyncWebServer _server;
    EmulatorStateManager* _stateManager = nullptr;

    // ========================================================================
    // ROUTE HANDLERS
    // ========================================================================

    // Status endpoints
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleGetMonitored(AsyncWebServerRequest* request);
    void handleGetEmulated(AsyncWebServerRequest* request);
    void handleGetSystemStatus(AsyncWebServerRequest* request);

    // Door control
    void handleDoorSetPosition(AsyncWebServerRequest* request);
    void handleDoorSetOpen(AsyncWebServerRequest* request);
    void handleDoorSetClosed(AsyncWebServerRequest* request);
    void handleDoorInjectFault(AsyncWebServerRequest* request);
    void handleDoorClearFault(AsyncWebServerRequest* request);
    void handleDoorConfig(AsyncWebServerRequest* request);

    // Water control
    void handleWaterConfig(AsyncWebServerRequest* request);
    void handleWaterPulse(AsyncWebServerRequest* request);
    void handleWaterResetCounters(AsyncWebServerRequest* request);
    void handleWaterSimulateFrozen(AsyncWebServerRequest* request);

    // Manual controls
    void handleManualSwitchPress(AsyncWebServerRequest* request);
    void handleManualSwitchRelease(AsyncWebServerRequest* request);
    void handleManualSwitchPulse(AsyncWebServerRequest* request);

    // Fault injection
    void handleSetDoorStuck(AsyncWebServerRequest* request);
    void handleClearAllFaults(AsyncWebServerRequest* request);

    // Settings
    void handleGetSettings(AsyncWebServerRequest* request);
    void handleUpdateSettings(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // System
    void handleReboot(AsyncWebServerRequest* request);
    void handleFactoryReset(AsyncWebServerRequest* request);

    // ========================================================================
    // HELPERS
    // ========================================================================

    void sendJsonResponse(AsyncWebServerRequest* request, JsonDocument& doc, int code = 200);
    void sendErrorResponse(AsyncWebServerRequest* request, const char* message, int code = 400);
    void sendSuccessResponse(AsyncWebServerRequest* request, const char* message = "OK");

    void setupRoutes();
    void setupStaticFiles();
    void setupOTA();
};

#endif // EMULATOR_WEB_SERVER_H
