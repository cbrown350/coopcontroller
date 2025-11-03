#include "WebServer.h"

#include <AsyncJson.h>

#include "Logger.h"
#include "SensorManager.h"
#include "PumpController.h"

#include <ElegantOTA.h>
#include <ArduinoOTA.h>

#define SPIFFS LittleFS

// External references to firmware version from main.cpp
extern const char *firmwareVersion;
extern const char *chipFamily;

extern const char* hostName;
extern const char* otaPasswd;

// External references to coop controller components
extern SensorManager tempSensor;
extern PumpController pumpController;

WebServer::WebServer(int port) : server(port) {}

void WebServer::begin()
{
    server.begin();

    // Get settings endpoint
    server.on("/get_settings", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = settingsManager.toJson(false);
                  request->send(200, "application/json", jsonResponse);
              });

    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/update_settings",
        [](AsyncWebServerRequest *request, JsonVariant &json)
        {
            JsonObject jsonObj = json.as<JsonObject>();
            
            // Only set WiFi settings if provided (i.e., when changing WiFi)
            if (jsonObj["ssid"].is<const char*>()) {
                settingsManager.setSSID(jsonObj["ssid"].as<String>());
            }
            if (jsonObj["passwd"].is<const char*>()) {
                settingsManager.setPassword(jsonObj["passwd"].as<String>());
            }
            if (jsonObj["ap_mode"].is<bool>()) {
                settingsManager.setAPMode(jsonObj["ap_mode"].as<bool>());
            }
            
            // Handle coop controller settings (these don't trigger WiFi changes)
            if (jsonObj["temp_threshold_on_f"].is<float>()) {
                settingsManager.setTempThresholdOnF(jsonObj["temp_threshold_on_f"].as<float>());
            }
            if (jsonObj["temp_threshold_off_f"].is<float>()) {
                settingsManager.setTempThresholdOffF(jsonObj["temp_threshold_off_f"].as<float>());
            }
            if (jsonObj["water_flow_error_timeout_seconds"].is<int>()) {
                settingsManager.setWaterFlowErrorTimeoutSeconds(jsonObj["water_flow_error_timeout_seconds"].as<int>());
            }
            if (jsonObj["pump_on_time_seconds"].is<int>()) {
                settingsManager.setPumpOnTimeSeconds(jsonObj["pump_on_time_seconds"].as<int>());
            }
            if (jsonObj["pump_off_time_seconds"].is<int>()) {
                settingsManager.setPumpOffTimeSeconds(jsonObj["pump_off_time_seconds"].as<int>());
            }
            if (jsonObj["pump_auto_mode"].is<bool>()) {
                settingsManager.setPumpAutoMode(jsonObj["pump_auto_mode"].as<bool>());
            }
            if (jsonObj["light_auto_mode"].is<bool>()) {
                settingsManager.setLightAutoMode(jsonObj["light_auto_mode"].as<bool>());
            }
            if (jsonObj["log_level"].is<String>()) {
                settingsManager.setLogLevel(jsonObj["log_level"].as<String>());
                // Update logger's current log level
                String levelStr = jsonObj["log_level"].as<String>();
                if (levelStr == "VERBOSE") {
                    logger.setLogLevel(LogLevel::VERBOSE);
                } else if (levelStr == "DEBUG") {
                    logger.setLogLevel(LogLevel::DEBUG);
                } else if (levelStr == "INFO") {
                    logger.setLogLevel(LogLevel::INFO);
                } else if (levelStr == "WARNING") {
                    logger.setLogLevel(LogLevel::WARNING);
                } else if (levelStr == "ERROR") {
                    logger.setLogLevel(LogLevel::ERROR);
                } else {
                    logger.setLogLevel(LogLevel::INFO); // Default fallback
                }
            }
            
            if (jsonObj["watchdog_timeout_seconds"].is<int>()) {
                settingsManager.setWatchdogTimeoutSeconds(jsonObj["watchdog_timeout_seconds"].as<int>());
                logger.log("Watchdog timeout updated - restart required for changes to take effect");
            }

            if (jsonObj["pulses_per_gallon"].is<float>()) {
                float newCalibration = jsonObj["pulses_per_gallon"].as<float>();
                settingsManager.setPulsesPerGallon(newCalibration);
                tempSensor.setPulsesPerGallon(newCalibration);
                logger.logf("Water meter calibration updated: %.1f pulses per gallon", newCalibration);
            }
            
            if (jsonObj["water_meter_timeout_seconds"].is<int>()) {
                int timeout = jsonObj["water_meter_timeout_seconds"].as<int>();
                settingsManager.setWaterMeterTimeoutSeconds(timeout);
                logger.logf("Water meter timeout updated: %d seconds", timeout);
            }
            
            // Note: 'enabled' is not sent from UI, so not handling it here to avoid defaults triggering changes
            
            settingsManager.save();
            jsonObj.clear();
            request->send(200, "text/plain", "ok");
        }));
        
    // Setup ArduinoOTA

    if (hostName && strlen(hostName) > 0) {
        ArduinoOTA.setHostname(hostName); // Need to set hostname in all places for mDNS to work
    } 
    if (otaPasswd && strlen(otaPasswd) > 0) {
        ArduinoOTA.setPassword(otaPasswd); // Optional for authentication
        Serial.println("OTA password set: " + String(otaPasswd));
    }
    ArduinoOTA.begin();

    // Setup ElegantOTA
    ElegantOTA.begin(&server);
    if (otaPasswd && strlen(otaPasswd) > 0) {
        ElegantOTA.setAuth("admin", otaPasswd);  // Optional: add authentication
        Serial.println("ElegantOTA admin password set: " + String(otaPasswd));
    } 
    // Configure ElegantOTA for filesystem updates
    ElegantOTA.onProgress([](unsigned int progress, unsigned int total) {
        logger.logf("OTA Update Progress: %u%%\r", (progress / (total / 100)));
    });
    
    // Add custom callback for filesystem updates
    ElegantOTA.onStart([]() {
        logger.log("Start OTA updating ");
        LittleFS.end();
    });
    
    ElegantOTA.onEnd([](bool success) {
        logger.log("\nOTA update End");
        if(success) {
            logger.log("OTA update completed successfully, restarting...");
            ESP.restart();
        } else {
            logger.log("OTA update failed");
            LittleFS.begin();
        }
    });

    // Sensor status endpoint
    server.on("/sensor_status", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  JsonDocument jsonDoc;
                  
                  // Temperature sensor data
                  JsonObject sensor1 = jsonDoc["sensor1"].to<JsonObject>();
                  sensor1["type"] = tempSensor.getSensor1Type() == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" : (tempSensor.getSensor1Type() == SensorType::WATER_METER ? "WATER_METER" : "UNKNOWN");
                  sensor1["connected"] = tempSensor.isSensor1Detected() && 
                                      (tempSensor.getSensor1Type() == SensorType::WATER_METER ? 
                                       tempSensor.isActivelyConnected(tempSensor.getSensor1Data()) : 
                                       tempSensor.isSensor1Connected());
                  if (isnan(tempSensor.getTemperature1F())) {
                      sensor1["temperature_f"] = nullptr;
                  } else {
                      sensor1["temperature_f"] = tempSensor.getTemperature1F();
                  }
                  sensor1["flow_rate"] = tempSensor.getFlowRate1();
                  sensor1["pulse_count"] = tempSensor.getPulseCount1();
                  sensor1["last_pulse_time"] = tempSensor.getTimeSinceLastPulse(tempSensor.getSensor1Data());
                  sensor1["actively_connected"] = tempSensor.isActivelyConnected(tempSensor.getSensor1Data());
                  sensor1["status"] = tempSensor.getSensorStatusString(tempSensor.getSensor1Data());
                  
                  JsonObject sensor2 = jsonDoc["sensor2"].to<JsonObject>();
                  sensor2["type"] = tempSensor.getSensor2Type() == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" : (tempSensor.getSensor2Type() == SensorType::WATER_METER ? "WATER_METER" : "UNKNOWN");
                  sensor2["connected"] = tempSensor.isSensor2Detected() && 
                                      (tempSensor.getSensor2Type() == SensorType::WATER_METER ? 
                                       tempSensor.isActivelyConnected(tempSensor.getSensor2Data()) : 
                                       tempSensor.isSensor2Connected());
                  if (isnan(tempSensor.getTemperature2F())) {
                      sensor2["temperature_f"] = nullptr;
                  } else {
                      sensor2["temperature_f"] = tempSensor.getTemperature2F();
                  }
                  sensor2["flow_rate"] = tempSensor.getFlowRate2();
                  sensor2["pulse_count"] = tempSensor.getPulseCount2();
                  sensor2["last_pulse_time"] = tempSensor.getTimeSinceLastPulse(tempSensor.getSensor2Data());
                  sensor2["actively_connected"] = tempSensor.isActivelyConnected(tempSensor.getSensor2Data());
                  sensor2["status"] = tempSensor.getSensorStatusString(tempSensor.getSensor2Data());
                  
                  // Pump controller data
                  JsonObject pump = jsonDoc["pump"].to<JsonObject>();
                  pump["state"] = pumpController.getStateString();
                  pump["is_active"] = pumpController.isPumpOn();
                  pump["temperature_f"] = pumpController.getCurrentTemperature();
                  pump["temperature_below_threshold"] = tempSensor.isTemperatureBelowThreshold();
                  pump["flow_error"] = pumpController.hasFlowError();
                  pump["current_cycle_time"] = pumpController.getCurrentCycleTime() / 1000;
                  pump["time_until_next_switch"] = pumpController.getTimeUntilNextSwitch() / 1000;
                  pump["total_on_time"] = pumpController.getTotalOnTime() / 1000;
                  pump["total_off_time"] = pumpController.getTotalOffTime() / 1000;
                  pump["total_cycles"] = pumpController.getTotalCycles();
                  pump["time_until_retry"] = pumpController.getTimeUntilRetry() / 1000;
                  
                  // System status
                  JsonObject system = jsonDoc["system"].to<JsonObject>();
                  system["temp_threshold_on_f"] = settingsManager.getTempThresholdOnF();
                  system["temp_threshold_off_f"] = settingsManager.getTempThresholdOffF();
                  system["pump_on_time_seconds"] = settingsManager.getPumpOnTimeSeconds();
                  system["pump_off_time_seconds"] = settingsManager.getPumpOffTimeSeconds();
                  system["pump_auto_mode"] = settingsManager.getPumpAutoMode();
                  system["light_auto_mode"] = settingsManager.getLightAutoMode();
                  system["log_level"] = settingsManager.getLogLevel();
                  system["watchdog_timeout_seconds"] = settingsManager.getWatchdogTimeoutSeconds();
                  system["water_meter_timeout_seconds"] = settingsManager.getWaterMeterTimeoutSeconds();
                  system["water_meter_timeout_seconds"] = settingsManager.getWaterMeterTimeoutSeconds();
                  
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Pump control endpoints
    server.on("/pump/on", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.turnOn();
                  request->send(200, "text/plain", "Pump turned on");
              });

    server.on("/pump/off", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.turnOff();
                  request->send(200, "text/plain", "Pump turned off");
              });

    server.on("/pump/auto", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.setAutoMode(true);
                  request->send(200, "text/plain", "Pump set to auto mode");
              });

    server.on("/pump/force_cycle", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.forceCycle();
                  request->send(200, "text/plain", "Pump cycle forced");
              });

    server.on("/pump/reset_stats", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.resetStatistics();
                  request->send(200, "text/plain", "Pump statistics reset");
              });

    server.on("/pump/clear_error", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  pumpController.clearFlowError();
                  request->send(200, "text/plain", "Pump flow error cleared");
              });

    // Water meter reset endpoints
    server.on("/water/reset/1", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  tempSensor.resetPulseCount(1);
                  request->send(200, "text/plain", "Water meter 1 reset");
              });

    server.on("/water/reset/2", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  tempSensor.resetPulseCount(2);
                  request->send(200, "text/plain", "Water meter 2 reset");
              });

    // Logs endpoint
    server.on("/logs", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = logger.getLogsAsJson();
                  request->send(200, "application/json", jsonResponse);
              });

    // System status endpoint
    server.on("/system_status", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  JsonDocument jsonDoc;
                  
                  // Memory information
                  jsonDoc["heap_free"] = ESP.getFreeHeap();
                  jsonDoc["heap_size"] = ESP.getHeapSize();
                  jsonDoc["heap_used_percent"] = 100.0 - (100.0 * ESP.getFreeHeap() / ESP.getHeapSize());
                  
                  // Uptime
                  unsigned long uptimeSeconds = millis() / 1000;
                  jsonDoc["uptime_seconds"] = uptimeSeconds;
                  
                  // Format uptime as human-readable string
                  unsigned long days = uptimeSeconds / 86400;
                  uptimeSeconds %= 86400;
                  unsigned long hours = uptimeSeconds / 3600;
                  uptimeSeconds %= 3600;
                  unsigned long minutes = uptimeSeconds / 60;
                  uptimeSeconds %= 60;
                  
                  String formatted = "";
                  if (days > 0) formatted += String(days) + "d ";
                  if (hours > 0 || days > 0) formatted += String(hours) + "h ";
                  if (minutes > 0 || hours > 0 || days > 0) formatted += String(minutes) + "m ";
                  formatted += String(uptimeSeconds) + "s";
                  
                  jsonDoc["uptime_formatted"] = formatted;
                  
                  // Chip information
                  jsonDoc["chip_model"] = ESP.getChipModel();
                  jsonDoc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
                  jsonDoc["flash_size"] = ESP.getFlashChipSize();
                  
                  // WiFi information (if connected)
                  if (WiFi.status() == WL_CONNECTED) {
                      jsonDoc["wifi_rssi"] = WiFi.RSSI();
                      jsonDoc["wifi_ssid"] = WiFi.SSID();
                  } else {
                      jsonDoc["wifi_rssi"] = 0;
                      jsonDoc["wifi_ssid"] = "Not Connected";
                  }
                  
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Version endpoint
    server.on("/version", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  JsonDocument jsonDoc;
                  jsonDoc["firmware_version"] = firmwareVersion;
                  jsonDoc["chip_family"]      = chipFamily;
                  jsonDoc["build_date"]       = __DATE__;
                  jsonDoc["build_time"]       = __TIME__;

                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Factory reset endpoint
    server.on("/factory_reset", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  // Check for confirmation parameter
                  if (!request->hasParam("confirm", true)) {
                      request->send(400, "text/plain", "Missing confirmation parameter");
                      return;
                  }
                  
                  String confirm = request->getParam("confirm", true)->value();
                  if (confirm != "RESET") {
                      request->send(400, "text/plain", "Invalid confirmation value");
                      return;
                  }
                  
                  logger.logWarning("Factory reset requested via web interface");
                  
                  // Perform factory reset
                  settingsManager.factoryReset();
                  
                  request->send(200, "text/plain", "Factory reset complete. Device will restart in 3 seconds.");
                  
                  // Schedule restart
                  delay(3000);
                  ESP.restart();
              });

    // Serve static files from SPIFFS
    server.serveStatic("/assets/", SPIFFS, "/assets/");
    server.serveStatic("/", SPIFFS, "/");
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.htm", "text/html");
    });
}

void WebServer::loop()
{
    ArduinoOTA.handle();
    ElegantOTA.loop();
}