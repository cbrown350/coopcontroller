#include "CoopControllerWebServer.h"
#include "config.h"

#include <AsyncJson.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>

#include "Logger.h"
#include "SensorManager.h"
#include "PumpController.h"
#include "BuzzerController.h"
#include "DoorController.h"
#include "LightController.h"
#include "SunriseSunset.h"
#include "WifiController.h"

#define SPIFFS LittleFS

// External references to coop controller components
extern SensorManager tempSensor;
extern PumpController pumpController;
extern BuzzerController buzzerController;
extern DoorController doorController;
extern LightController lightController;
extern WifiController wifiController;

// External reference to sunrise/sunset calculator
extern SunriseSunsetCalculator sunriseSunset;

CoopControllerWebServer::CoopControllerWebServer(int port) : server(port) {}

void CoopControllerWebServer::begin()
{
    server.begin();

    // Get settings endpoint
    server.on("/get_settings", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = settingsManager.toJson(false);
                  // add hostName 
                  jsonResponse.replace("}", ",\"hostname\":\"" + String(hostName) + "\"}");
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
                lightController.setAutoMode(jsonObj["light_auto_mode"].as<bool>());
            }
            if (jsonObj["light_on_hour"].is<int>()) {
                settingsManager.setLightOnHour(jsonObj["light_on_hour"].as<int>());
                lightController.setOnHour(jsonObj["light_on_hour"].as<int>());
            }
            if (jsonObj["light_off_hour"].is<int>()) {
                settingsManager.setLightOffHour(jsonObj["light_off_hour"].as<int>());
                lightController.setOffHour(jsonObj["light_off_hour"].as<int>());
            }
            if (jsonObj["light_brightness_percent"].is<int>()) {
                settingsManager.setLightBrightnessPercent(jsonObj["light_brightness_percent"].as<int>());
                lightController.setMaxBrightness(jsonObj["light_brightness_percent"].as<int>());
            }
            if (jsonObj["light_transition_duration_minutes"].is<int>()) {
                settingsManager.setLightTransitionDurationMinutes(jsonObj["light_transition_duration_minutes"].as<int>());
                lightController.setTransitionDurationMinutes(jsonObj["light_transition_duration_minutes"].as<int>());
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
                logger.logInfo("Watchdog timeout updated - restart required for changes to take effect");
            }

            if (jsonObj["pulses_per_gallon"].is<float>()) {
                float newCalibration = jsonObj["pulses_per_gallon"].as<float>();
                settingsManager.setPulsesPerGallon(newCalibration);
                tempSensor.setPulsesPerGallon(newCalibration);
                logger.logInfo("Water meter calibration updated: " + String(newCalibration, 1) + " pulses per gallon");
            }
            
            if (jsonObj["water_meter_timeout_seconds"].is<int>()) {
                int timeout = jsonObj["water_meter_timeout_seconds"].as<int>();
                settingsManager.setWaterMeterTimeoutSeconds(timeout);
                logger.logInfo("Water meter timeout updated: " + String(timeout) + " seconds");
            }
            
            if (jsonObj["wifi_led_enabled"].is<bool>()) {
                bool enabled = jsonObj["wifi_led_enabled"].as<bool>();
                settingsManager.setWifiLedEnabled(enabled);
                logger.logInfo("WiFi LED enabled: " + String(enabled ? "true" : "false"));
            }
            
            // Handle buzzer settings
            if (jsonObj["buzzer_enabled"].is<bool>()) {
                bool enabled = jsonObj["buzzer_enabled"].as<bool>();
                settingsManager.setBuzzerEnabled(enabled);
                buzzerController.setEnabled(enabled);
                logger.logInfo("Buzzer enabled: " + String(enabled ? "true" : "false"));
            }
            
            if (jsonObj["buzzer_type"].is<String>()) {
                String type = jsonObj["buzzer_type"].as<String>();
                settingsManager.setBuzzerType(type);
                BuzzerType buzzerType = (type == "PASSIVE") ? BuzzerType::PASSIVE : BuzzerType::ACTIVE;
                buzzerController.setBuzzerType(buzzerType);
                logger.logInfo("Buzzer type set to: " + type);
            }
            
            // Handle door control settings
            if (jsonObj["door_auto_mode"].is<bool>()) {
                settingsManager.setDoorAutoMode(jsonObj["door_auto_mode"].as<bool>());
            }
            if (jsonObj["door_open_timeout_seconds"].is<int>()) {
                settingsManager.setDoorOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
            }
            if (jsonObj["door_close_timeout_seconds"].is<int>()) {
                settingsManager.setDoorCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
            }
            if (jsonObj["sunrise_offset_minutes"].is<int>()) {
                settingsManager.setSunriseOffsetMinutes(jsonObj["sunrise_offset_minutes"].as<int>());
            }
            if (jsonObj["sunset_offset_minutes"].is<int>()) {
                settingsManager.setSunsetOffsetMinutes(jsonObj["sunset_offset_minutes"].as<int>());
            }
            
            // Handle location settings
            bool locationChanged = false;
            if (jsonObj["latitude"].is<float>()) {
                settingsManager.setLatitude(jsonObj["latitude"].as<float>());
                locationChanged = true;
            }
            if (jsonObj["longitude"].is<float>()) {
                settingsManager.setLongitude(jsonObj["longitude"].as<float>());
                locationChanged = true;
            }
            if (jsonObj["timezone_offset_hours"].is<int>()) {
                settingsManager.setTimezoneOffsetHours(jsonObj["timezone_offset_hours"].as<int>());
                locationChanged = true;
            }
            
            // Recalculate sunrise/sunset if location changed
            if (locationChanged) {
                sunriseSunset.begin(settingsManager.getLatitude(),
                                   settingsManager.getLongitude(),
                                   settingsManager.getTimezoneOffsetHours());
                sunriseSunset.forceUpdate();
                logger.logInfo("Location settings updated, sunrise/sunset recalculated");
            }
            
            // Handle Task 3.5k preparation settings
            if (jsonObj["door_auto_close_after_sunset_enabled"].is<bool>()) {
                settingsManager.setDoorAutoCloseAfterSunsetEnabled(jsonObj["door_auto_close_after_sunset_enabled"].as<bool>());
            }
            if (jsonObj["door_auto_close_after_sunset_minutes"].is<int>()) {
                settingsManager.setDoorAutoCloseAfterSunsetMinutes(jsonObj["door_auto_close_after_sunset_minutes"].as<int>());
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
        logger.logInfo("OTA password set: " + String(otaPasswd));
    }
    ArduinoOTA.begin();

    // Setup ElegantOTA
    ElegantOTA.begin(&server);
    if (otaPasswd && strlen(otaPasswd) > 0) {
        ElegantOTA.setAuth("admin", otaPasswd);  // Optional: add authentication
        logger.logInfo("ElegantOTA admin password set: " + String(otaPasswd));
    } 
    // Configure ElegantOTA for filesystem updates
    ElegantOTA.onProgress([](unsigned int progress, unsigned int total) {
        logger.logInfo("OTA Update Progress: " + String(progress / (total / 100)) + "%");
    });
    
    // Add custom callback for filesystem updates
    ElegantOTA.onStart([]() {
        logger.logInfo("Start OTA updating ");
        LittleFS.end();
    });
    
    ElegantOTA.onEnd([](bool success) {
        logger.logInfo("OTA update End");
        if(success) {
            logger.logInfo("OTA update completed successfully, restarting...");
            ESP.restart();
        } else {
            logger.logError("OTA update failed");
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
                  float pumpTemp = pumpController.getCurrentTemperature();
                  if (isnan(pumpTemp)) {
                      pump["temperature_f"] = nullptr;
                  } else {
                      pump["temperature_f"] = pumpTemp;
                  }
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
                  system["water_flow_error_timeout_seconds"] = settingsManager.getWaterFlowErrorTimeoutSeconds();

                   // Buzzer status
                   JsonObject buzzer = jsonDoc["buzzer"].to<JsonObject>();
                   buzzerController.toJson(buzzer);
                   
                   // Door status
                   JsonObject door = jsonDoc["door"].to<JsonObject>();
                   doorController.toJson(door);
                   
                   // Light status
                   JsonObject light = jsonDoc["light"].to<JsonObject>();
                   lightController.toJson(light);
                   
                   // Sun times
                   JsonObject sun = jsonDoc["sun"].to<JsonObject>();
                   sun["sunrise"] = sunriseSunset.getSunriseTime();
                   sun["sunset"] = sunriseSunset.getSunsetTime();
                   sun["sunrise_minutes"] = sunriseSunset.getSunriseMinutes();
                   sun["sunset_minutes"] = sunriseSunset.getSunsetMinutes();
                  
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

    // Buzzer control endpoints
    server.on("/buzzer/silence", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  buzzerController.silenceAlerts();
                  request->send(200, "text/plain", "Buzzer alerts silenced");
              });

    server.on("/buzzer/test", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  buzzerController.testAlert();
                  request->send(200, "text/plain", "Buzzer test alert triggered");
              });
    server.on("/buzzer/clear", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  // Clear the currently active alert (could be TEST_ALERT or PUMP_ERROR)
                  if (buzzerController.hasActiveAlert()) {
                      AlertType currentAlert = buzzerController.getCurrentAlertType();
                      buzzerController.clearAlert(currentAlert);
                      request->send(200, "text/plain", "Buzzer alert cleared");
                  } else {
                      request->send(200, "text/plain", "No active alert to clear");
                  }
              });

    // Door control endpoints
    server.on("/door/open", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  doorController.open();
                  request->send(200, "text/plain", "Door opening");
              });

    server.on("/door/close", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  doorController.close();
                  request->send(200, "text/plain", "Door closing");
              });

    server.on("/door/stop", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  doorController.stop();
                  request->send(200, "text/plain", "Door stopped");
              });

    server.on("/door/set_auto", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  if (!request->hasParam("auto", true)) {
                      request->send(400, "text/plain", "Missing auto parameter");
                      return;
                  }
                  
                  String autoStr = request->getParam("auto", true)->value();
                  bool autoMode = (autoStr == "true" || autoStr == "1");
                  doorController.setAutoMode(autoMode);
                  request->send(200, "text/plain", autoMode ? "Door auto mode enabled" : "Door auto mode disabled");
              });

    server.on("/door/clear_fault", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  doorController.clearFault();
                  request->send(200, "text/plain", "Door fault cleared");
              });

    server.on("/door/reset_stats", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  doorController.resetStatistics();
                  request->send(200, "text/plain", "Door statistics reset");
              });

    // Light control endpoints
    server.on("/light/on", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  lightController.turnOn();
                  request->send(200, "text/plain", "Light turned on");
              });

    server.on("/light/off", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  lightController.turnOff();
                  request->send(200, "text/plain", "Light turned off");
              });

    server.on("/light/fade_in", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  lightController.fadeIn();
                  request->send(200, "text/plain", "Light fading in");
              });

    server.on("/light/fade_out", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  lightController.fadeOut();
                  request->send(200, "text/plain", "Light fading out");
              });

    server.on("/light/set_brightness", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  if (!request->hasParam("brightness", true)) {
                      request->send(400, "text/plain", "Missing brightness parameter");
                      return;
                  }
                  
                  int brightness = request->getParam("brightness", true)->value().toInt();
                  if (brightness < 0 || brightness > 100) {
                      request->send(400, "text/plain", "Brightness must be 0-100");
                      return;
                  }
                  
                  lightController.setBrightness(brightness);
                  request->send(200, "text/plain", "Light brightness set to " + String(brightness) + "%");
              });

    server.on("/light/set_auto", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  if (!request->hasParam("auto", true)) {
                      request->send(400, "text/plain", "Missing auto parameter");
                      return;
                  }
                  
                  String autoStr = request->getParam("auto", true)->value();
                  bool autoMode = (autoStr == "true" || autoStr == "1");
                  lightController.setAutoMode(autoMode);
                  settingsManager.setLightAutoMode(autoMode);
                  settingsManager.save();
                  request->send(200, "text/plain", autoMode ? "Light auto mode enabled" : "Light auto mode disabled");
              });

    server.on("/light/reset_stats", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  lightController.resetStatistics();
                  request->send(200, "text/plain", "Light statistics reset");
              });

    server.on("/light/test_mode", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  if (!request->hasParam("enabled", true)) {
                      request->send(400, "text/plain", "Missing enabled parameter");
                      return;
                  }
                  
                  String enabledStr = request->getParam("enabled", true)->value();
                  bool enabled = (enabledStr == "true" || enabledStr == "1");
                  lightController.setTestMode(enabled);
                  request->send(200, "text/plain", enabled ? "Light test mode enabled" : "Light test mode disabled");
              });

    // Logs endpoint

    // Logs endpoint
    server.on("/logs", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = logger.getLogsAsJson();
                  request->send(200, "application/json", jsonResponse);
              });

   // Sun times endpoint
   server.on("/sun/times", HTTP_GET,
             [](AsyncWebServerRequest *request)
             {
                 JsonDocument doc;
                 
                 // Format sunrise time without leading zero
                 int sunriseMin = sunriseSunset.getSunriseMinutes();
                 int sunriseHour = sunriseMin / 60;
                 int sunriseMinute = sunriseMin % 60;
                 String sunrisePeriod = (sunriseHour >= 12) ? "PM" : "AM";
                 sunriseHour = (sunriseHour > 12) ? sunriseHour - 12 : sunriseHour;
                 sunriseHour = (sunriseHour == 0) ? 12 : sunriseHour;
                 char sunriseBuffer[16];
                 sprintf(sunriseBuffer, "%d:%02d %s", sunriseHour, sunriseMinute, sunrisePeriod.c_str());
                 doc["sunrise"] = String(sunriseBuffer);
                 
                 // Format sunset time without leading zero
                 int sunsetMin = sunriseSunset.getSunsetMinutes();
                 int sunsetHour = sunsetMin / 60;
                 int sunsetMinute = sunsetMin % 60;
                 String sunsetPeriod = (sunsetHour >= 12) ? "PM" : "AM";
                 sunsetHour = (sunsetHour > 12) ? sunsetHour - 12 : sunsetHour;
                 sunsetHour = (sunsetHour == 0) ? 12 : sunsetHour;
                 char sunsetBuffer[16];
                 sprintf(sunsetBuffer, "%d:%02d %s", sunsetHour, sunsetMinute, sunsetPeriod.c_str());
                 doc["sunset"] = String(sunsetBuffer);
                 
                 doc["sunrise_minutes"] = sunriseSunset.getSunriseMinutes();
                 doc["sunset_minutes"] = sunriseSunset.getSunsetMinutes();
                 doc["latitude"] = settingsManager.getLatitude();
                 doc["longitude"] = settingsManager.getLongitude();
                 doc["timezone_offset"] = settingsManager.getTimezoneOffsetHours();
                 
                 String response;
                 serializeJson(doc, response);
                 request->send(200, "application/json", response);
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
                  if (wifiController.isConnected()) {
                      jsonDoc["wifi_rssi"] = wifiController.getRSSI();
                      jsonDoc["wifi_ssid"] = wifiController.getSSID();
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
    // Reboot endpoint
    server.on("/reboot", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  // Check for confirmation parameter
                  if (!request->hasParam("confirm", true)) {
                      request->send(400, "text/plain", "Missing confirmation parameter");
                      return;
                  }
                  
                  String confirm = request->getParam("confirm", true)->value();
                  if (confirm != "REBOOT") {
                      request->send(400, "text/plain", "Invalid confirmation value");
                      return;
                  }
                  
                  logger.logWarning("Reboot requested via web interface");
                  
                  request->send(200, "text/plain", "Device will reboot in 3 seconds.");
                  
                  // Schedule reboot
                  delay(3000);
                  ESP.restart();
              });

    // Backup settings endpoint
    server.on("/settings/backup", HTTP_GET,
      [](AsyncWebServerRequest *request)
      {
        String jsonResponse = settingsManager.toJson(false);
        request->send(200, "application/json", jsonResponse);
      });

    // Restore settings endpoint
    server.addHandler(new AsyncCallbackJsonWebHandler(
      "/settings/restore",
      [](AsyncWebServerRequest *request, JsonVariant &json)
      {
        JsonObject jsonObj = json.as<JsonObject>();
        
        // Basic validation - check for required fields
        if (jsonObj.isNull()) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON format\"}");
          return;
        }
        
        // Update settings with validation
        if (jsonObj["temp_threshold_on_f"].is<float>()) {
          settingsManager.setTempThresholdOnF(jsonObj["temp_threshold_on_f"].as<float>());
        }
        if (jsonObj["temp_threshold_off_f"].is<float>()) {
          settingsManager.setTempThresholdOffF(jsonObj["temp_threshold_off_f"].as<float>());
        }
        if (jsonObj["pump_on_time_seconds"].is<int>()) {
          settingsManager.setPumpOnTimeSeconds(jsonObj["pump_on_time_seconds"].as<int>());
        }
        if (jsonObj["pump_off_time_seconds"].is<int>()) {
          settingsManager.setPumpOffTimeSeconds(jsonObj["pump_off_time_seconds"].as<int>());
        }
        if (jsonObj["light_auto_mode"].is<bool>()) {
          settingsManager.setLightAutoMode(jsonObj["light_auto_mode"].as<bool>());
        }
        if (jsonObj["light_on_hour"].is<int>()) {
          settingsManager.setLightOnHour(jsonObj["light_on_hour"].as<int>());
        }
        if (jsonObj["light_off_hour"].is<int>()) {
          settingsManager.setLightOffHour(jsonObj["light_off_hour"].as<int>());
        }
        if (jsonObj["light_brightness_percent"].is<int>()) {
          settingsManager.setLightBrightnessPercent(jsonObj["light_brightness_percent"].as<int>());
        }
        if (jsonObj["light_transition_duration_minutes"].is<int>()) {
          settingsManager.setLightTransitionDurationMinutes(jsonObj["light_transition_duration_minutes"].as<int>());
        }
        if (jsonObj["water_flow_error_timeout_seconds"].is<int>()) {
          settingsManager.setWaterFlowErrorTimeoutSeconds(jsonObj["water_flow_error_timeout_seconds"].as<int>());
        }
        if (jsonObj["pulses_per_gallon"].is<float>()) {
          float newCalibration = jsonObj["pulses_per_gallon"].as<float>();
          settingsManager.setPulsesPerGallon(newCalibration);
          tempSensor.setPulsesPerGallon(newCalibration);
          logger.logInfo("Water meter calibration restored: " + String(newCalibration, 1) + " pulses per gallon");
        }
        if (jsonObj["water_meter_timeout_seconds"].is<int>()) {
          settingsManager.setWaterMeterTimeoutSeconds(jsonObj["water_meter_timeout_seconds"].as<int>());
        }
        if (jsonObj["wifi_led_enabled"].is<bool>()) {
          settingsManager.setWifiLedEnabled(jsonObj["wifi_led_enabled"].as<bool>());
        }
        if (jsonObj["buzzer_enabled"].is<bool>()) {
          bool enabled = jsonObj["buzzer_enabled"].as<bool>();
          settingsManager.setBuzzerEnabled(enabled);
          buzzerController.setEnabled(enabled);
          logger.logInfo("Buzzer enabled restored: " + String(enabled ? "true" : "false"));
        }
        if (jsonObj["buzzer_type"].is<String>()) {
          String type = jsonObj["buzzer_type"].as<String>();
          settingsManager.setBuzzerType(type);
          BuzzerType buzzerType = (type == "PASSIVE") ? BuzzerType::PASSIVE : BuzzerType::ACTIVE;
          buzzerController.setBuzzerType(buzzerType);
          logger.logInfo("Buzzer type restored: " + type);
        }
        if (jsonObj["door_auto_mode"].is<bool>()) {
          settingsManager.setDoorAutoMode(jsonObj["door_auto_mode"].as<bool>());
        }
        if (jsonObj["door_open_timeout_seconds"].is<int>()) {
          settingsManager.setDoorOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
        }
        if (jsonObj["door_close_timeout_seconds"].is<int>()) {
          settingsManager.setDoorCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
        }
        if (jsonObj["sunrise_offset_minutes"].is<int>()) {
          settingsManager.setSunriseOffsetMinutes(jsonObj["sunrise_offset_minutes"].as<int>());
        }
        if (jsonObj["sunset_offset_minutes"].is<int>()) {
          settingsManager.setSunsetOffsetMinutes(jsonObj["sunset_offset_minutes"].as<int>());
        }
        if (jsonObj["door_auto_mode"].is<bool>()) {
          settingsManager.setDoorAutoMode(jsonObj["door_auto_mode"].as<bool>());
        }
        if (jsonObj["door_open_timeout_seconds"].is<int>()) {
          settingsManager.setDoorOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
        }
        if (jsonObj["door_close_timeout_seconds"].is<int>()) {
          settingsManager.setDoorCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
        }
        if (jsonObj["sunrise_offset_minutes"].is<int>()) {
          settingsManager.setSunriseOffsetMinutes(jsonObj["sunrise_offset_minutes"].as<int>());
        }
        if (jsonObj["sunset_offset_minutes"].is<int>()) {
          settingsManager.setSunsetOffsetMinutes(jsonObj["sunset_offset_minutes"].as<int>());
        }
        
        // Save settings to persistent storage
        settingsManager.save();
        
        jsonObj.clear();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings restored successfully\"}");
      }));
      
    // SPA route rewrites (avoid FS open errors for client-side routes)
    server.addRewrite(new AsyncWebRewrite("/status", "/index.htm"));
    server.addRewrite(new AsyncWebRewrite("/settings", "/index.htm"));
    server.addRewrite(new AsyncWebRewrite("/log", "/index.htm"));
    server.addRewrite(new AsyncWebRewrite("/updates", "/index.htm"));
    server.addRewrite(new AsyncWebRewrite("/about", "/index.htm"));

    // Serve static files from SPIFFS
    server.serveStatic("/assets/", SPIFFS, "/assets/");
    server.serveStatic("/", SPIFFS, "/");
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.htm", "text/html");
    });
}

void CoopControllerWebServer::loop()
{
    ArduinoOTA.handle();
    ElegantOTA.loop();
}