#ifndef EMULATOR_WEB_SERVER_H
#define EMULATOR_WEB_SERVER_H

#include "CustomScenarioManager.h"
#include "EmulatorSettings.h"
#include "EmulatorStateManager.h"
#include "LogRecorder.h"
#include "TempSensorEmulator.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include <WiFi.h>

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
  void begin(EmulatorStateManager &stateManager);

  /**
   * @brief Process OTA and other async tasks
   * Call from main loop
   */
  void loop();

private:
  AsyncWebServer _server;
  EmulatorStateManager *_stateManager = nullptr;

  // ========================================================================
  // ROUTE HANDLERS
  // ========================================================================

  // Status endpoints
  void handleGetStatus(AsyncWebServerRequest *request);
  void handleGetMonitored(AsyncWebServerRequest *request);
  void handleGetEmulated(AsyncWebServerRequest *request);
  void handleGetSystemStatus(AsyncWebServerRequest *request);

  // Door control
  void handleDoorSetPosition(AsyncWebServerRequest *request);
  void handleDoorSetOpen(AsyncWebServerRequest *request);
  void handleDoorSetClosed(AsyncWebServerRequest *request);
  void handleDoorInjectFault(AsyncWebServerRequest *request);
  void handleDoorClearFault(AsyncWebServerRequest *request);
  void handleDoorConfig(AsyncWebServerRequest *request);

  // Water control
  void handleWaterConfig(AsyncWebServerRequest *request);
  void handleWaterPulse(AsyncWebServerRequest *request);
  void handleWaterResetCounters(AsyncWebServerRequest *request);
  void handleWaterSimulateFrozen(AsyncWebServerRequest *request);

  // Manual controls
  void handleManualSwitchPress(AsyncWebServerRequest *request);
  void handleManualSwitchRelease(AsyncWebServerRequest *request);
  void handleManualSwitchPulse(AsyncWebServerRequest *request);
  void handleManualSwitchLongPress(AsyncWebServerRequest *request);
  void handleManualSwitchConfig(AsyncWebServerRequest *request);

  // Manual override mode
  void handleOverrideEnable(AsyncWebServerRequest *request);
  void handleOverrideDisable(AsyncWebServerRequest *request);
  void handleOverrideSetState(AsyncWebServerRequest *request);
  void handleOverrideClearAll(AsyncWebServerRequest *request);

  // Scenarios
  void handleGetScenarios(AsyncWebServerRequest *request);
  void handleGetActiveScenario(AsyncWebServerRequest *request);
  void handleApplyScenario(AsyncWebServerRequest *request);
  void handleApplyScenarioById(AsyncWebServerRequest *request);
  void handleApplyCustomScenario(AsyncWebServerRequest *request, uint8_t *data,
                                 size_t len);

  // Custom Scenarios
  void handleGetCustomScenarios(AsyncWebServerRequest *request);
  void handleSaveCustomScenario(AsyncWebServerRequest *request, uint8_t *data,
                                size_t len);
  void handleDeleteCustomScenario(AsyncWebServerRequest *request);
  void handleApplyCustomScenarioByName(AsyncWebServerRequest *request);

  // Fault injection
  void handleSetDoorStuck(AsyncWebServerRequest *request);
  void handleClearAllFaults(AsyncWebServerRequest *request);

  // Settings
  void handleGetSettings(AsyncWebServerRequest *request);
  void handleUpdateSettings(AsyncWebServerRequest *request, uint8_t *data,
                            size_t len);
  void handleExportSettings(AsyncWebServerRequest *request);
  void handleImportSettings(AsyncWebServerRequest *request, uint8_t *data,
                            size_t len);

  // Recordings
  void handleGetRecordings(AsyncWebServerRequest *request);
  void handleGetRecordingStatus(AsyncWebServerRequest *request);
  void handleStartRecording(AsyncWebServerRequest *request);
  void handleStopRecording(AsyncWebServerRequest *request);
  void handleToggleRecordingPause(AsyncWebServerRequest *request);
  void handleStartPlayback(AsyncWebServerRequest *request);
  void handleStopPlayback(AsyncWebServerRequest *request);
  void handleTogglePlaybackPause(AsyncWebServerRequest *request);
  void handleSetPlaybackSpeed(AsyncWebServerRequest *request);
  void handleDeleteRecording(AsyncWebServerRequest *request);
  void handleDeleteAllRecordings(AsyncWebServerRequest *request);
  void handleDownloadRecording(AsyncWebServerRequest *request);

  // Temperature sensors
  void handleGetTemperature(AsyncWebServerRequest *request);
  void handleSetTemperature(AsyncWebServerRequest *request, uint8_t *data,
                            size_t len);
  void handleSetTempSensorEnabled(AsyncWebServerRequest *request);
  void handleSetTempDisconnected(AsyncWebServerRequest *request);
  void handleSetTempDrift(AsyncWebServerRequest *request);

  // System
  void handleReboot(AsyncWebServerRequest *request);
  void handleFactoryReset(AsyncWebServerRequest *request);

  // ========================================================================
  // HELPERS
  // ========================================================================

  void sendJsonResponse(AsyncWebServerRequest *request, JsonDocument &doc,
                        int code = 200);
  void sendErrorResponse(AsyncWebServerRequest *request, const char *message,
                         int code = 400);
  void sendSuccessResponse(AsyncWebServerRequest *request,
                           const char *message = "OK");

  void setupRoutes();
  void setupStaticFiles();
  void setupOTA();
};

#endif // EMULATOR_WEB_SERVER_H
