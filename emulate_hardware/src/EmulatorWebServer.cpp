#include "EmulatorWebServer.h"

EmulatorWebServer::EmulatorWebServer(uint16_t port)
    : _server(port) {}

void EmulatorWebServer::begin(EmulatorStateManager& stateManager) {
    _stateManager = &stateManager;

    setupRoutes();
    setupStaticFiles();
    setupOTA();

    _server.begin();
    Serial.println("[EmulatorWebServer] Started");
}

void EmulatorWebServer::loop() {
    ElegantOTA.loop();
}

// ============================================================================
// ROUTE SETUP
// ============================================================================

void EmulatorWebServer::setupRoutes() {
    // ========================================================================
    // STATUS ENDPOINTS
    // ========================================================================

    _server.on("/emulator/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });

    _server.on("/emulator/monitored", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetMonitored(request);
    });

    _server.on("/emulator/emulated", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetEmulated(request);
    });

    _server.on("/system_status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetSystemStatus(request);
    });

    // ========================================================================
    // DOOR CONTROL
    // ========================================================================

    _server.on("/emulator/door/position", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorSetPosition(request);
    });

    _server.on("/emulator/door/open", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorSetOpen(request);
    });

    _server.on("/emulator/door/close", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorSetClosed(request);
    });

    _server.on("/emulator/door/fault", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorInjectFault(request);
    });

    _server.on("/emulator/door/clear_fault", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorClearFault(request);
    });

    _server.on("/emulator/door/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleDoorConfig(request);
    });

    // ========================================================================
    // WATER CONTROL
    // ========================================================================

    _server.on("/emulator/water/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWaterConfig(request);
    });

    _server.on("/emulator/water/pulse", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWaterPulse(request);
    });

    _server.on("/emulator/water/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWaterResetCounters(request);
    });

    _server.on("/emulator/water/frozen", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWaterSimulateFrozen(request);
    });

    // ========================================================================
    // MANUAL CONTROLS
    // ========================================================================

    _server.on("/emulator/manual_switch/press", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleManualSwitchPress(request);
    });

    _server.on("/emulator/manual_switch/release", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleManualSwitchRelease(request);
    });

    _server.on("/emulator/manual_switch/pulse", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleManualSwitchPulse(request);
    });

    _server.on("/emulator/manual_switch/long_press", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleManualSwitchLongPress(request);
    });

    _server.on("/emulator/manual_switch/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleManualSwitchConfig(request);
    });

    // ========================================================================
    // MANUAL OVERRIDE MODE
    // ========================================================================

    _server.on("/emulator/override/enable", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleOverrideEnable(request);
    });

    _server.on("/emulator/override/disable", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleOverrideDisable(request);
    });

    _server.on("/emulator/override/set", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleOverrideSetState(request);
    });

    _server.on("/emulator/override/clear_all", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleOverrideClearAll(request);
    });

    // ========================================================================
    // FAULT INJECTION
    // ========================================================================

    _server.on("/emulator/fault/door_stuck", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleSetDoorStuck(request);
    });

    _server.on("/emulator/fault/clear_all", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleClearAllFaults(request);
    });

    // ========================================================================
    // SETTINGS
    // ========================================================================

    _server.on("/get_settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetSettings(request);
    });

    _server.on("/update_settings", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleUpdateSettings(request, data, len);
        }
    );

    // ========================================================================
    // SYSTEM
    // ========================================================================

    _server.on("/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleReboot(request);
    });

    _server.on("/factory_reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleFactoryReset(request);
    });

    // SPA fallback - serve index.htm for unknown routes
    _server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_GET) {
            request->send(LittleFS, "/index.htm", "text/html");
        } else {
            request->send(404, "text/plain", "Not Found");
        }
    });

    Serial.println("[EmulatorWebServer] Routes configured");
}

void EmulatorWebServer::setupStaticFiles() {
    // Serve static files from LittleFS
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.htm");
    _server.serveStatic("/assets/", LittleFS, "/assets/");

    Serial.println("[EmulatorWebServer] Static files configured");
}

void EmulatorWebServer::setupOTA() {
    ElegantOTA.begin(&_server);
    Serial.println("[EmulatorWebServer] OTA configured");
}

// ============================================================================
// STATUS HANDLERS
// ============================================================================

void EmulatorWebServer::handleGetStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    _stateManager->toJson(obj);
    sendJsonResponse(request, doc);
}

void EmulatorWebServer::handleGetMonitored(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    _stateManager->monitoredToJson(obj);
    sendJsonResponse(request, doc);
}

void EmulatorWebServer::handleGetEmulated(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    _stateManager->emulatedToJson(obj);
    sendJsonResponse(request, doc);
}

void EmulatorWebServer::handleGetSystemStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();

    obj["uptime_seconds"] = millis() / 1000;
    obj["heap_free"] = ESP.getFreeHeap();
    obj["heap_size"] = ESP.getHeapSize();
    obj["heap_used_percent"] = 100.0f * (1.0f - (float)ESP.getFreeHeap() / ESP.getHeapSize());
    obj["chip_model"] = ESP.getChipModel();
    obj["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
    obj["flash_size"] = ESP.getFlashChipSize();
    obj["firmware_version"] = firmwareVersion;
    obj["hostname"] = hostName;

    // Format uptime
    uint32_t uptime = millis() / 1000;
    char uptimeStr[32];
    snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02lu:%02lu:%02lu",
             uptime / 86400, (uptime / 3600) % 24, (uptime / 60) % 60, uptime % 60);
    obj["uptime_formatted"] = uptimeStr;

    // WiFi info
    if (WiFi.status() == WL_CONNECTED) {
        obj["wifi_ssid"] = WiFi.SSID();
        obj["wifi_rssi"] = WiFi.RSSI();
        obj["wifi_ip"] = WiFi.localIP().toString();
    } else if (WiFi.getMode() == WIFI_AP) {
        obj["wifi_ssid"] = String(hostName) + "_AP";
        obj["wifi_ip"] = WiFi.softAPIP().toString();
        obj["wifi_rssi"] = 0;
    }

    sendJsonResponse(request, doc);
}

// ============================================================================
// DOOR CONTROL HANDLERS
// ============================================================================

void EmulatorWebServer::handleDoorSetPosition(AsyncWebServerRequest* request) {
    if (!request->hasParam("position", true)) {
        sendErrorResponse(request, "Missing 'position' parameter");
        return;
    }

    int position = request->getParam("position", true)->value().toInt();
    if (position < 0 || position > 100) {
        sendErrorResponse(request, "Position must be 0-100");
        return;
    }

    _stateManager->setDoorPosition(position);
    sendSuccessResponse(request, "Door position set");
}

void EmulatorWebServer::handleDoorSetOpen(AsyncWebServerRequest* request) {
    _stateManager->setDoorState(DoorState::OPEN);
    sendSuccessResponse(request, "Door set to OPEN");
}

void EmulatorWebServer::handleDoorSetClosed(AsyncWebServerRequest* request) {
    _stateManager->setDoorState(DoorState::CLOSED);
    sendSuccessResponse(request, "Door set to CLOSED");
}

void EmulatorWebServer::handleDoorInjectFault(AsyncWebServerRequest* request) {
    _stateManager->setDoorFault(true);
    sendSuccessResponse(request, "Door fault injected");
}

void EmulatorWebServer::handleDoorClearFault(AsyncWebServerRequest* request) {
    _stateManager->setDoorFault(false);
    sendSuccessResponse(request, "Door fault cleared");
}

void EmulatorWebServer::handleDoorConfig(AsyncWebServerRequest* request) {
    EmulatorConfig config = _stateManager->getConfig();

    if (request->hasParam("travel_time_ms", true)) {
        config.doorTravelTimeMs = request->getParam("travel_time_ms", true)->value().toInt();
    }
    if (request->hasParam("auto_simulate", true)) {
        config.autoSimulateDoor = request->getParam("auto_simulate", true)->value() == "true";
    }

    _stateManager->setConfig(config);
    sendSuccessResponse(request, "Door config updated");
}

// ============================================================================
// WATER CONTROL HANDLERS
// ============================================================================

void EmulatorWebServer::handleWaterConfig(AsyncWebServerRequest* request) {
    EmulatorConfig config = _stateManager->getConfig();

    if (request->hasParam("pulses_per_gallon", true)) {
        config.pulsesPerGallon = request->getParam("pulses_per_gallon", true)->value().toFloat();
    }
    if (request->hasParam("flow_rate_gpm", true)) {
        config.flowRateGPM = request->getParam("flow_rate_gpm", true)->value().toFloat();
    }
    if (request->hasParam("auto_generate", true)) {
        config.autoGeneratePulses = request->getParam("auto_generate", true)->value() == "true";
    }

    _stateManager->setConfig(config);
    _stateManager->setFlowRate(config.flowRateGPM);
    sendSuccessResponse(request, "Water config updated");
}

void EmulatorWebServer::handleWaterPulse(AsyncWebServerRequest* request) {
    uint8_t channel = 1;
    if (request->hasParam("channel", true)) {
        channel = request->getParam("channel", true)->value().toInt();
    }

    _stateManager->triggerSinglePulse(channel);
    sendSuccessResponse(request, "Water pulse triggered");
}

void EmulatorWebServer::handleWaterResetCounters(AsyncWebServerRequest* request) {
    _stateManager->resetPulseCounters();
    sendSuccessResponse(request, "Pulse counters reset");
}

void EmulatorWebServer::handleWaterSimulateFrozen(AsyncWebServerRequest* request) {
    bool frozen = true;
    if (request->hasParam("frozen", true)) {
        frozen = request->getParam("frozen", true)->value() == "true";
    }

    EmulatorConfig config = _stateManager->getConfig();
    config.simulateFrozenLine = frozen;
    _stateManager->setConfig(config);
    sendSuccessResponse(request, frozen ? "Frozen line simulation ON" : "Frozen line simulation OFF");
}

// ============================================================================
// MANUAL CONTROL HANDLERS
// ============================================================================

void EmulatorWebServer::handleManualSwitchPress(AsyncWebServerRequest* request) {
    _stateManager->pressManualSwitch();
    sendSuccessResponse(request, "Manual switch pressed");
}

void EmulatorWebServer::handleManualSwitchRelease(AsyncWebServerRequest* request) {
    _stateManager->releaseManualSwitch();
    sendSuccessResponse(request, "Manual switch released");
}

void EmulatorWebServer::handleManualSwitchPulse(AsyncWebServerRequest* request) {
    uint32_t duration = 200;
    if (request->hasParam("duration_ms", true)) {
        duration = request->getParam("duration_ms", true)->value().toInt();
    }

    _stateManager->pulseManualSwitch(duration);
    sendSuccessResponse(request, "Manual switch pulsed");
}

void EmulatorWebServer::handleManualSwitchLongPress(AsyncWebServerRequest* request) {
    uint32_t duration = DEFAULT_LONG_PRESS_MS;
    if (request->hasParam("duration_ms", true)) {
        duration = request->getParam("duration_ms", true)->value().toInt();
    }

    _stateManager->longPressManualSwitch(duration);
    sendSuccessResponse(request, "Manual switch long pressed");
}

void EmulatorWebServer::handleManualSwitchConfig(AsyncWebServerRequest* request) {
    uint32_t shortMs = DEFAULT_SHORT_PRESS_MS;
    uint32_t longMs = DEFAULT_LONG_PRESS_MS;

    if (request->hasParam("short_press_ms", true)) {
        shortMs = request->getParam("short_press_ms", true)->value().toInt();
    }
    if (request->hasParam("long_press_ms", true)) {
        longMs = request->getParam("long_press_ms", true)->value().toInt();
    }

    _stateManager->setManualSwitchThresholds(shortMs, longMs);
    sendSuccessResponse(request, "Manual switch config updated");
}

// ============================================================================
// MANUAL OVERRIDE HANDLERS
// ============================================================================

void EmulatorWebServer::handleOverrideEnable(AsyncWebServerRequest* request) {
    _stateManager->setManualOverrideEnabled(true);
    sendSuccessResponse(request, "Manual override mode enabled");
}

void EmulatorWebServer::handleOverrideDisable(AsyncWebServerRequest* request) {
    _stateManager->setManualOverrideEnabled(false);
    sendSuccessResponse(request, "Manual override mode disabled");
}

void EmulatorWebServer::handleOverrideSetState(AsyncWebServerRequest* request) {
    if (!_stateManager->isManualOverrideEnabled()) {
        sendErrorResponse(request, "Manual override mode is not enabled");
        return;
    }

    // Parse which signals to set
    if (request->hasParam("hall_open", true)) {
        _stateManager->setOverrideHallOpen(request->getParam("hall_open", true)->value() == "true");
    }
    if (request->hasParam("hall_close", true)) {
        _stateManager->setOverrideHallClose(request->getParam("hall_close", true)->value() == "true");
    }
    if (request->hasParam("door_fault", true)) {
        _stateManager->setOverrideDoorFault(request->getParam("door_fault", true)->value() == "true");
    }
    if (request->hasParam("manual_switch", true)) {
        _stateManager->setOverrideManualSwitch(request->getParam("manual_switch", true)->value() == "true");
    }
    if (request->hasParam("water_pulse_1", true)) {
        _stateManager->setOverrideWaterPulse(1, request->getParam("water_pulse_1", true)->value() == "true");
    }
    if (request->hasParam("water_pulse_2", true)) {
        _stateManager->setOverrideWaterPulse(2, request->getParam("water_pulse_2", true)->value() == "true");
    }

    sendSuccessResponse(request, "Override states updated");
}

void EmulatorWebServer::handleOverrideClearAll(AsyncWebServerRequest* request) {
    _stateManager->clearAllOverrides();
    sendSuccessResponse(request, "All overrides cleared");
}

// ============================================================================
// FAULT INJECTION HANDLERS
// ============================================================================

void EmulatorWebServer::handleSetDoorStuck(AsyncWebServerRequest* request) {
    bool stuck = true;
    if (request->hasParam("stuck", true)) {
        stuck = request->getParam("stuck", true)->value() == "true";
    }

    EmulatorConfig config = _stateManager->getConfig();
    config.simulateDoorStuck = stuck;
    _stateManager->setConfig(config);
    sendSuccessResponse(request, stuck ? "Door stuck simulation ON" : "Door stuck simulation OFF");
}

void EmulatorWebServer::handleClearAllFaults(AsyncWebServerRequest* request) {
    EmulatorConfig config = _stateManager->getConfig();
    config.injectDoorFault = false;
    config.simulateFrozenLine = false;
    config.simulateDoorStuck = false;
    _stateManager->setConfig(config);
    _stateManager->setDoorFault(false);
    sendSuccessResponse(request, "All faults cleared");
}

// ============================================================================
// SETTINGS HANDLERS
// ============================================================================

void EmulatorWebServer::handleGetSettings(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    emulatorSettings.toJson(obj, false);  // Don't include password
    sendJsonResponse(request, doc);
}

void EmulatorWebServer::handleUpdateSettings(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        sendErrorResponse(request, "Invalid JSON");
        return;
    }

    JsonObject obj = doc.as<JsonObject>();
    emulatorSettings.fromJson(obj);
    emulatorSettings.save();
    emulatorSettings.applyToStateManager(*_stateManager);

    sendSuccessResponse(request, "Settings updated");
}

// ============================================================================
// SYSTEM HANDLERS
// ============================================================================

void EmulatorWebServer::handleReboot(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, "Rebooting...");
    delay(500);
    ESP.restart();
}

void EmulatorWebServer::handleFactoryReset(AsyncWebServerRequest* request) {
    emulatorSettings.resetToDefaults();
    emulatorSettings.save();
    sendSuccessResponse(request, "Factory reset complete. Rebooting...");
    delay(500);
    ESP.restart();
}

// ============================================================================
// HELPERS
// ============================================================================

void EmulatorWebServer::sendJsonResponse(AsyncWebServerRequest* request, JsonDocument& doc, int code) {
    String response;
    serializeJson(doc, response);
    request->send(code, "application/json", response);
}

void EmulatorWebServer::sendErrorResponse(AsyncWebServerRequest* request, const char* message, int code) {
    JsonDocument doc;
    doc["error"] = message;
    doc["success"] = false;
    sendJsonResponse(request, doc, code);
}

void EmulatorWebServer::sendSuccessResponse(AsyncWebServerRequest* request, const char* message) {
    JsonDocument doc;
    doc["message"] = message;
    doc["success"] = true;
    sendJsonResponse(request, doc);
}
