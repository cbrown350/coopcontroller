#include "CoopControllerWebServer.h"
#include "config.h"
#include "IHAL.h"
#include "build_timestamp.h"

#include "Logger.h"
#include "SensorManager.h"
#include "PumpController.h"
#include "BuzzerController.h"
#include "DoorController.h"
#include "LightController.h"
#include "SunriseSunset.h"
#include "WifiController.h"

#include <stdint.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>

CoopControllerWebServer::CoopControllerWebServer(IHAL* hal, uint16_t port) : hal(hal), port(port) {
}

void CoopControllerWebServer::begin(SensorManager& tempSensor, // NOSONAR - complexity ok
                                      PumpController& pumpController,
                                      BuzzerController& buzzerController,
                                      DoorController& doorController,
                                      LightController& lightController,
                                      const WifiController& wifiController,
                                      SunriseSunsetCalculator& sunriseSunset,
                                      HistoricalDataManager& historyManager)
{
    hal->webServerBegin(port);

    // Get settings endpoint
    hal->webServerOn("/get_settings", HAL_WebRequestMethod::HTTP_GET,
              [](IWebRequest *request, IWebResponse *response)
              {
                  String jsonResponse = settingsManager.toJson(false);
                  // add hostName
                  jsonResponse.replace("}", R"(,"hostname":")" + String(hostName) + R"("})");
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    // Update settings endpoint - uses POST with JSON body
    hal->webServerOn("/update_settings", HAL_WebRequestMethod::HTTP_POST,
              [this, &lightController, &tempSensor, &buzzerController, &doorController, &sunriseSunset, &historyManager](IWebRequest *request, IWebResponse *response) // NOSONAR
              {
                  // Authentication check
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  // Get request body and parse JSON  
                //   String body = request->body();                
                // Serial.printf("[WebServer] /update_settings request received, %s\n", body.c_str());
                // delay(3000);
                //   JsonDocument jsonDoc;
                //   DeserializationError error = deserializeJson(jsonDoc, body);
                  
                //   if (error != DeserializationError::Ok) {
                //       response->send(400, "application/json", R"({"success":false,"error":"Invalid JSON format"})");
                //       return;
                //   }
                  
                //   JsonObject jsonObj = jsonDoc.as<JsonObject>();
                  JsonObject jsonObj = request->jsonBody().as<JsonObject>();
                  // print json
                  Serial.println("[WebServer] /update_settings request received with JSON:");
                  serializeJsonPretty(jsonObj, Serial);
                  Serial.println();
                  
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
                      logger.setLogLevel(logger.stringToLogLevel(settingsManager.getLogLevel()));
                  }
                  
                  if (jsonObj["watchdog_timeout_seconds"].is<int>()) {
                      settingsManager.setWatchdogTimeoutSeconds(jsonObj["watchdog_timeout_seconds"].as<int>());
                      logger.logInfo("Watchdog timeout updated - restart required for changes to take effect");
                  }

                  if (jsonObj["pulses_per_gallon"].is<float>()) {
                      float newCalibration = jsonObj["pulses_per_gallon"].as<float>();
                          settingsManager.setPulsesPerGallon(newCalibration);
                          tempSensor.setPulsesPerGallon(newCalibration);
                          String calibMsg = "Water meter calibration updated: " + String(newCalibration, 1) + " pulses per gallon";
                          logger.logInfo(calibMsg.c_str());
                  }
                  
                  if (jsonObj["water_meter_timeout_seconds"].is<int>()) {
                      int timeout = jsonObj["water_meter_timeout_seconds"].as<int>();
                          settingsManager.setWaterMeterTimeoutSeconds(timeout);
                          String timeoutMsg = "Water meter timeout updated: " + String(timeout) + " seconds";
                          logger.logInfo(timeoutMsg.c_str());
                  }
                  
                  if (jsonObj["wifi_led_enabled"].is<bool>()) {
                      bool enabled = jsonObj["wifi_led_enabled"].as<bool>();
                      settingsManager.setWifiLedEnabled(enabled);
                      String ledMsg = "WiFi LED enabled: " + String(enabled ? "true" : "false");
                      logger.logInfo(ledMsg.c_str());
                  }

                  if (jsonObj["wifi_bssid_preference"].is<const char*>()) {
                      settingsManager.setWifiBssidPreference(jsonObj["wifi_bssid_preference"].as<String>());
                  }

                  // Handle buzzer settings
                  if (jsonObj["buzzer_enabled"].is<bool>()) {
                      bool enabled = jsonObj["buzzer_enabled"].as<bool>();
                      settingsManager.setBuzzerEnabled(enabled);
                      buzzerController.setEnabled(enabled);
                      String buzzerMsg = "Buzzer enabled: " + String(enabled ? "true" : "false");
                      logger.logInfo(buzzerMsg.c_str());
                  }
                  
                  if (jsonObj["buzzer_type"].is<String>()) {
                      String type = jsonObj["buzzer_type"].as<String>();
                      settingsManager.setBuzzerType(type);
                      BuzzerType buzzerType = (type == "PASSIVE") ? BuzzerType::PASSIVE : BuzzerType::ACTIVE;
                      buzzerController.setBuzzerType(buzzerType);
                      String buzzerTypeMsg = "Buzzer type set to: " + type;
                      logger.logInfo(buzzerTypeMsg.c_str());
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
                      sunriseSunset.setCoordinates(settingsManager.getLatitude(),
                                          settingsManager.getLongitude(),
                                          settingsManager.getTimezoneOffsetHours());
                      sunriseSunset.forceUpdate();
                      logger.logInfo("Location settings updated, sunrise/sunset recalculated");
                  }
                  
                  // Handle door advanced features settings
                  if (jsonObj["door_auto_close_after_sunset_enabled"].is<bool>()) {
                      settingsManager.setDoorAutoCloseAfterSunsetEnabled(jsonObj["door_auto_close_after_sunset_enabled"].as<bool>());
                  }
                  if (jsonObj["door_auto_close_after_sunset_minutes"].is<int>()) {
                      settingsManager.setDoorAutoCloseAfterSunsetMinutes(jsonObj["door_auto_close_after_sunset_minutes"].as<int>());
                  }
                  if (jsonObj["door_lockout_enabled"].is<bool>()) {
                      bool enabled = jsonObj["door_lockout_enabled"].as<bool>();
                      settingsManager.setDoorLockoutEnabled(enabled);
                      doorController.setLockoutEnabled(enabled);
                  }
                  if (jsonObj["door_timeout_auto_calc_enabled"].is<bool>()) {
                      bool enabled = jsonObj["door_timeout_auto_calc_enabled"].as<bool>();
                      settingsManager.setDoorTimeoutAutoCalcEnabled(enabled);
                      doorController.setAutoCalcTimeoutEnabled(enabled);
                  }
                  
                  if (jsonObj["water_meter_per_pulse_calculation_enabled"].is<bool>()) {
                      bool enabled = jsonObj["water_meter_per_pulse_calculation_enabled"].as<bool>();
                      settingsManager.setWaterMeterPerPulseCalculationEnabled(enabled);
                      String calcMsg = "Per-pulse flow calculation: " + String(enabled ? "enabled" : "disabled");
                      logger.logInfo(calcMsg.c_str());
                  }
                  
                  if (jsonObj["pump_off_flow_monitoring_enabled"].is<bool>()) {
                      bool enabled = jsonObj["pump_off_flow_monitoring_enabled"].as<bool>();
                      settingsManager.setPumpOffFlowMonitoringEnabled(enabled);
                      String monitorMsg = "Pump OFF flow monitoring: " + String(enabled ? "enabled" : "disabled");
                      logger.logInfo(monitorMsg.c_str());
                  }
                  
                  if (jsonObj["pump_off_flow_grace_period_seconds"].is<int>()) {
                      int gracePeriod = jsonObj["pump_off_flow_grace_period_seconds"].as<int>();
                      settingsManager.setPumpOffFlowGracePeriodSeconds(gracePeriod);
                      String graceMsg = "Pump OFF flow grace period: " + String(gracePeriod) + " seconds";
                      logger.logInfo(graceMsg.c_str());
                  }

                  if (jsonObj["pump_min_daily_cycles_enabled"].is<bool>()) {
                      bool enabled = jsonObj["pump_min_daily_cycles_enabled"].as<bool>();
                      settingsManager.setPumpMinDailyCyclesEnabled(enabled);
                      logger.logInfo(enabled ? "Minimum daily pump cycles: enabled" : "Minimum daily pump cycles: disabled");
                  }

                  if (jsonObj["pump_min_daily_cycles"].is<int>()) {
                      unsigned int cycles = jsonObj["pump_min_daily_cycles"].as<unsigned int>();
                      settingsManager.setPumpMinDailyCycles(cycles);
                      String msg = "Minimum daily pump cycles: " + String(cycles);
                      logger.logInfo(msg.c_str());
                  }

                  if (jsonObj["pump_min_cycle_run_seconds"].is<int>()) {
                      unsigned int seconds = jsonObj["pump_min_cycle_run_seconds"].as<unsigned int>();
                      settingsManager.setPumpMinCycleRunSeconds(seconds);
                      String msg = "Scheduled cycle run duration: " + String(seconds) + " seconds";
                      logger.logInfo(msg.c_str());
                  }

                  // Handle API authentication settings
                  if (jsonObj["api_auth_enabled"].is<bool>()) {
                      bool enabled = jsonObj["api_auth_enabled"].as<bool>();
                      settingsManager.setApiAuthEnabled(enabled);
                      logger.logInfo(enabled ? "API authentication: enabled" : "API authentication: disabled");
                  }

                  if (jsonObj["api_username"].is<String>()) {
                      String username = jsonObj["api_username"].as<String>();
                      settingsManager.setApiUsername(username);
                      logger.logInfo("API username updated: " + username);
                  }

                  if (jsonObj["api_password"].is<String>()) {
                      String password = jsonObj["api_password"].as<String>();
                      settingsManager.setApiPassword(password);
                      logger.logInfo(password.length() > 0 ? "API password updated" : "API password cleared");
                  }

                  // Handle syslog configuration
                  bool syslogChanged = false;
                  if (jsonObj["syslog_server"].is<String>()) {
                      settingsManager.setSyslogServer(jsonObj["syslog_server"].as<String>());
                      syslogChanged = true;
                  }
                  if (jsonObj["syslog_port"].is<int>()) {
                      settingsManager.setSyslogPort(jsonObj["syslog_port"].as<int>());
                      syslogChanged = true;
                  }
                  if (syslogChanged) {
                      logger.reconfigureSyslog(settingsManager.getSyslogServer(),
                                              settingsManager.getSyslogPort(),
                                              hostName);
                  }

                  // Handle flow calculation interval
                  if (jsonObj["flow_calculation_interval_seconds"].is<int>()) {
                      unsigned int interval = jsonObj["flow_calculation_interval_seconds"].as<unsigned int>();
                      settingsManager.setFlowCalculationIntervalSeconds(interval);
                      tempSensor.setFlowCalculationIntervalSeconds(interval);
                      logger.logInfo(String("Flow calculation interval: ") + String(interval) + " seconds");
                  }

                  // Handle history data settings
                  if (jsonObj["history_enabled"].is<bool>()) {
                      settingsManager.setHistoryEnabled(jsonObj["history_enabled"].as<bool>());
                      historyManager.setEnabled(jsonObj["history_enabled"].as<bool>());
                  }
                  if (jsonObj["history_temp_min_interval_seconds"].is<int>()) {
                      unsigned int interval = jsonObj["history_temp_min_interval_seconds"].as<unsigned int>();
                      settingsManager.setHistoryTempMinIntervalSeconds(interval);
                      historyManager.setTempMinInterval(interval);
                  }
                  if (jsonObj["history_flow_min_interval_seconds"].is<int>()) {
                      unsigned int interval = jsonObj["history_flow_min_interval_seconds"].as<unsigned int>();
                      settingsManager.setHistoryFlowMinIntervalSeconds(interval);
                      historyManager.setFlowMinInterval(interval);
                  }
                  if (jsonObj["history_buffer_size"].is<int>()) {
                      settingsManager.setHistoryBufferSize(jsonObj["history_buffer_size"].as<unsigned int>());
                  }

                  // Note: 'enabled' is not sent from UI, so not handling it here to avoid defaults triggering changes

                  settingsManager.save();
                  response->send(200, "text/plain", "ok");
              });
    
    // Sensor status endpoint
    hal->webServerOn("/sensor_status", HAL_WebRequestMethod::HTTP_GET,
              [&tempSensor, &pumpController, &buzzerController, &doorController,  // NOSONAR - complexity ok
                  &lightController, &sunriseSunset](IWebRequest *request, IWebResponse *response)
              {
                  JsonDocument jsonDoc;
                  
                  // Temperature sensor data
                  JsonObject sensor1 = jsonDoc["sensor1"].to<JsonObject>();
                  sensor1["type"] = tempSensor.getSensor1Type() ==
                    SensorType::DALLAS_TEMP ? "DALLAS_TEMP" :
                        (tempSensor.getSensor1Type() == SensorType::WATER_METER ? "WATER_METER" : "UNKNOWN"); // NOSONAR - complexity ok
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
                  sensor2["type"] = tempSensor.getSensor2Type() == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" :
                            (tempSensor.getSensor2Type() == SensorType::WATER_METER ? "WATER_METER" : "UNKNOWN"); // NOSONAR - complexity ok
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
                  
                  if (float pumpTemp = pumpController.getCurrentTemperature(); isnan(pumpTemp)) {
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
                  pump["pump_off_flow_detected"] = pumpController.getPumpOffFlowDetected();
                  pump["scheduled_cycle_active"] = pumpController.isScheduledCycleActive();
                  pump["time_until_next_scheduled"] = pumpController.getTimeUntilNextScheduledCycle() / 1000;

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
                  system["pump_off_flow_monitoring_enabled"] = settingsManager.getPumpOffFlowMonitoringEnabled();
                  system["pump_off_flow_grace_period_seconds"] = settingsManager.getPumpOffFlowGracePeriodSeconds();
                  system["pump_min_daily_cycles_enabled"] = settingsManager.getPumpMinDailyCyclesEnabled();
                  system["pump_min_daily_cycles"] = settingsManager.getPumpMinDailyCycles();
                  system["pump_min_cycle_run_seconds"] = settingsManager.getPumpMinCycleRunSeconds();

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
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    // Pump control endpoints
    hal->webServerOn("/pump/on", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.turnOn();
                  logger.logInfo("Pump turned on (manual)");
                  response->send(200, "text/plain", "Pump turned on");
              });

    hal->webServerOn("/pump/off", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.turnOff();
                  logger.logInfo("Pump turned off (manual)");
                  response->send(200, "text/plain", "Pump turned off");
              });

    hal->webServerOn("/pump/auto", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.setAutoMode(true);
                  response->send(200, "text/plain", "Pump set to auto mode");
              });

    hal->webServerOn("/pump/force_cycle", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.forceCycle();
                  response->send(200, "text/plain", "Pump cycle forced");
              });

    hal->webServerOn("/pump/reset_stats", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.resetStatistics();
                  response->send(200, "text/plain", "Pump statistics reset");
              });

    hal->webServerOn("/pump/clear_error", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.clearFlowError();
                  response->send(200, "text/plain", "Pump flow error cleared");
              });

    hal->webServerOn("/pump/clear_off_flow_detected", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.clearPumpOffFlowDetected();
                  response->send(200, "text/plain", "Pump off flow detection cleared");
                  logger.logInfo("Pump off flow detection cleared via web request");
              });

        // Water meter reset endpoints
    hal->webServerOn("/water/reset/1", HAL_WebRequestMethod::HTTP_GET,
              [this, &tempSensor](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  tempSensor.resetPulseCount(1);
                  response->send(200, "text/plain", "Water meter 1 reset");
              });
    
    hal->webServerOn("/water/reset/2", HAL_WebRequestMethod::HTTP_GET,
              [this, &tempSensor](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  tempSensor.resetPulseCount(2);
                  response->send(200, "text/plain", "Water meter 2 reset");
              });
    
    // Buzzer control endpoints
    hal->webServerOn("/buzzer/silence", HAL_WebRequestMethod::HTTP_POST,
              [this, &buzzerController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  buzzerController.silenceAlerts();
                  response->send(200, "text/plain", "Buzzer alerts silenced");
              });
    
    hal->webServerOn("/buzzer/test", HAL_WebRequestMethod::HTTP_GET,
              [this, &buzzerController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  buzzerController.testAlert();
                  response->send(200, "text/plain", "Buzzer test alert triggered");
              });
    hal->webServerOn("/buzzer/clear", HAL_WebRequestMethod::HTTP_POST,
              [this, &buzzerController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  // Clear currently active alert (could be TEST_ALERT or PUMP_ERROR)
                  if (buzzerController.hasActiveAlert()) {
                      AlertType currentAlert = buzzerController.getCurrentAlertType();
                      buzzerController.clearAlert(currentAlert);
                      response->send(200, "text/plain", "Buzzer alert cleared");
                  } else {
                      response->send(200, "text/plain", "No active alert to clear");
                  }
              });
    
    // Door control endpoints
    hal->webServerOn("/door/open", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.open();
                  response->send(200, "text/plain", "Door opening");
              });
    
    hal->webServerOn("/door/close", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.close();
                  response->send(200, "text/plain", "Door closing");
              });
    
    hal->webServerOn("/door/stop", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.stop();
                  response->send(200, "text/plain", "Door stopped");
              });
    
    hal->webServerOn("/door/set_auto", HAL_WebRequestMethod::HTTP_POST,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  String autoStr = request->param("auto");
                  bool autoMode = (autoStr == "true" || autoStr == "1");
                  doorController.setAutoMode(autoMode);
                  response->send(200, "text/plain", autoMode ? "Door auto mode enabled" : "Door auto mode disabled");
              });
    
    hal->webServerOn("/door/clear_fault", HAL_WebRequestMethod::HTTP_POST,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.clearFault();
                  response->send(200, "text/plain", "Door fault cleared");
              });
    
    hal->webServerOn("/door/reset_stats", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.resetStatistics();
                  response->send(200, "text/plain", "Door statistics reset");
              });

    hal->webServerOn("/door/lockout/on", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.setLockoutEnabled(true);
                  settingsManager.setDoorLockoutEnabled(true);
                  settingsManager.save();
                  response->send(200, "text/plain", "Door lockout enabled");
              });

    hal->webServerOn("/door/lockout/off", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.setLockoutEnabled(false);
                  settingsManager.setDoorLockoutEnabled(false);
                  settingsManager.save();
                  response->send(200, "text/plain", "Door lockout disabled");
              });

    // Light control endpoints
    hal->webServerOn("/light/on", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.turnOn();
                  logger.logInfo("Light turned on (manual)");
                  response->send(200, "text/plain", "Light turned on");
              });
    
    hal->webServerOn("/light/off", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.turnOff();
                  logger.logInfo("Light turned off (manual)");
                  response->send(200, "text/plain", "Light turned off");
              });
    
    hal->webServerOn("/light/fade_in", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.fadeIn();
                  response->send(200, "text/plain", "Light fading in");
              });
    
    hal->webServerOn("/light/fade_out", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.fadeOut();
                  response->send(200, "text/plain", "Light fading out");
              });
    
    hal->webServerOn("/light/set_brightness", HAL_WebRequestMethod::HTTP_POST,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  const JsonVariant &json = request->jsonBody();
                  int brightness = json.isNull() ? -1 : (json["brightness"] | -1);
                  if (brightness < 0 || brightness > 100) {
                      response->send(400, "text/plain", "Brightness must be 0-100");
                      return;
                  }
                  
                  lightController.setBrightness(brightness);
                  String msg = "Light brightness set to " + String(brightness) + "%";
                  response->send(200, "text/plain", msg.c_str());
              });
    
    hal->webServerOn("/light/set_auto", HAL_WebRequestMethod::HTTP_POST,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  String autoStr = request->param("auto");
                  bool autoMode = (autoStr == "true" || autoStr == "1");
                  lightController.setAutoMode(autoMode);
                  settingsManager.setLightAutoMode(autoMode);
                  settingsManager.save();
                  response->send(200, "text/plain", autoMode ? "Light auto mode enabled" : "Light auto mode disabled");
              });
    
    hal->webServerOn("/light/reset_stats", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.resetStatistics();
                  response->send(200, "text/plain", "Light statistics reset");
              });
    
    hal->webServerOn("/light/test_mode", HAL_WebRequestMethod::HTTP_POST,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  String enabledStr = request->param("enabled");
                  bool enabled = (enabledStr == "true" || enabledStr == "1");
                  lightController.setTestMode(enabled);
                  response->send(200, "text/plain", enabled ? "Light test mode enabled" : "Light test mode disabled");
              });

    // Logs endpoint
    hal->webServerOn("/logs", HAL_WebRequestMethod::HTTP_GET,
              [](IWebRequest *request, IWebResponse *response)
              {
                  String jsonResponse = logger.getLogsAsJson();

                  // Check if JSON generation was successful
                  if (jsonResponse.length() == 0) {
                      // Return empty logs array if generation failed
                      response->send(200, "application/json", "{\"logs\":[]}");
                      return;
                  }

                  response->send(200, "application/json", jsonResponse.c_str());
              });
    
    // Sun times endpoint
    hal->webServerOn("/sun/times", HAL_WebRequestMethod::HTTP_GET,
              [&sunriseSunset](IWebRequest *request, IWebResponse *response) // NOSONAR - complexity ok
              {
                  JsonDocument doc;
                  
                  // Format sunrise time without leading zero
                  int sunriseMin = sunriseSunset.getSunriseMinutes();
                  int sunriseHour = sunriseMin / 60;
                  int sunriseMinute = sunriseMin % 60;
                  String sunrisePeriod = (sunriseHour >= 12) ? "PM" : "AM";
                  sunriseHour = (sunriseHour > 12) ? sunriseHour - 12 : sunriseHour;
                  String sunriseFormatted = String(sunriseHour) + ":" +
                                         (sunriseMinute < 10 ? "0" : "") +
                                         String(sunriseMinute) + " " + sunrisePeriod;
                  doc["sunrise"] = sunriseFormatted;
                  
                  // Format sunset time without leading zero
                  int sunsetMin = sunriseSunset.getSunsetMinutes();
                  int sunsetHour = sunsetMin / 60;
                  int sunsetMinute = sunsetMin % 60;
                  String sunsetPeriod = (sunsetHour >= 12) ? "PM" : "AM";
                  sunsetHour = (sunsetHour > 12) ? sunsetHour - 12 : sunsetHour;
                  String sunsetFormatted = String(sunsetHour) + ":" +
                                         (sunsetMinute < 10 ? "0" : "") +
                                         String(sunsetMinute) + " " + sunsetPeriod;
                  doc["sunset"] = sunsetFormatted;
                  
                  doc["sunrise_minutes"] = sunriseSunset.getSunriseMinutes();
                  doc["sunset_minutes"] = sunriseSunset.getSunsetMinutes();
                  doc["latitude"] = settingsManager.getLatitude();
                  doc["longitude"] = settingsManager.getLongitude();
                  doc["timezone_offset"] = settingsManager.getTimezoneOffsetHours();
                  
                  String jsonResponse;
                  serializeJson(doc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });
    
    // System status endpoint
    hal->webServerOn("/system_status", HAL_WebRequestMethod::HTTP_GET,
              [&wifiController, this](IWebRequest *request, IWebResponse *response) // NOSONAR - complexity ok
              {
                  JsonDocument jsonDoc;
                  
                  // Memory information
                  jsonDoc["heap_free"] = hal->getFreeHeap();
                  jsonDoc["heap_size"] = hal->getHeapSize();
                  jsonDoc["heap_used_percent"] = 100.0 - (100.0 * hal->getFreeHeap() / hal->getHeapSize());
                  
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
                  if (uptimeSeconds > 0 || hours > 0 || minutes > 0 || days > 0) formatted += String(uptimeSeconds) + "s ";
                  formatted += String(uptimeSeconds) + "s";
                  
                  jsonDoc["uptime_formatted"] = formatted;
                  
                  // Chip information
                  jsonDoc["chip_model"] = hal->getChipModel();
                  // Note: getCpuFreqMHz and getFlashChipSize are not in HAL, keeping as ESP calls for now
                  jsonDoc["cpu_freq_mhz"] = hal->getCpuFreqMHz();
                  jsonDoc["flash_size"] = hal->getFlashChipSize();
                  
                  // WiFi information (if connected)
                  if (wifiController.isConnected()) {
                      jsonDoc["wifi_rssi"] = wifiController.getRSSI();
                      jsonDoc["wifi_ssid"] = wifiController.getSSID();
                      jsonDoc["wifi_ip"] = wifiController.getIPAddress();
                      jsonDoc["wifi_mac"] = hal->wifiGetMacAddress();
                      jsonDoc["wifi_bssid"] = hal->wifiGetBSSID();
                  } else {
                      jsonDoc["wifi_rssi"] = 0;
                      jsonDoc["wifi_ssid"] = "Not Connected";
                      jsonDoc["wifi_ip"] = "N/A";
                      jsonDoc["wifi_mac"] = hal->wifiGetMacAddress();
                      jsonDoc["wifi_bssid"] = "N/A";
                  }
                  
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });
    
    // Version endpoint
    hal->webServerOn("/version", HAL_WebRequestMethod::HTTP_GET,
              [](IWebRequest *request, IWebResponse *response)
              {
                  JsonDocument jsonDoc;
                  jsonDoc["firmware_version"] = firmwareVersion;
                  jsonDoc["chip_family"]      = chipFamily;
                  jsonDoc["build_date"]       = BUILD_TIMESTAMP_DATE;
                  jsonDoc["build_time"]       = BUILD_TIMESTAMP_TIME;
                  
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });
    
    // Factory reset endpoint
    hal->webServerOn("/factory_reset", HAL_WebRequestMethod::HTTP_POST,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  // Authentication check
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  // Check for confirmation parameter (from JSON body)
                  const JsonVariant &json = request->jsonBody();
                  String confirm = json.isNull() ? "" : (json["confirm"] | "");
                  if (confirm != "RESET") {
                      response->send(400, "text/plain", "Invalid confirmation value");
                      return;
                  }
                  
                  logger.logWarning("Factory reset requested via web interface");
                  
                  // Perform factory reset
                  settingsManager.factoryReset();
                  
                  response->send(200, "text/plain", "Factory reset complete. Device will restart in 3 seconds.");
                  
                  // Schedule restart
                  delay(3000);
                  hal->restart();
              });
    
    // Reboot endpoint
    hal->webServerOn("/reboot", HAL_WebRequestMethod::HTTP_POST,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  // Authentication check
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  // Check for confirmation parameter (from JSON body)
                  const JsonVariant &json = request->jsonBody();
                  String confirm = json.isNull() ? "" : (json["confirm"] | "");
                  if (confirm != "REBOOT") {
                      response->send(400, "text/plain", "Invalid confirmation value");
                      return;
                  }
                  
                  logger.logWarning("Reboot requested via web interface");
                  
                  response->send(200, "text/plain", "Device will reboot in 3 seconds.");
                  
                  // Schedule restart
                  delay(3000);
                  hal->restart();
              });
    
    // Backup settings endpoint
    hal->webServerOn("/settings/backup", HAL_WebRequestMethod::HTTP_GET,
              [](IWebRequest *request, IWebResponse *response)
              {
                  String jsonResponse = settingsManager.toJson(false);
                  response->send(200, "application/json", jsonResponse.c_str());
              });
    
    // Restore settings endpoint - Note: Using manual JSON parsing instead of AsyncCallbackJsonWebHandler
    hal->webServerOn("/settings/restore", HAL_WebRequestMethod::HTTP_POST,
              [this, &tempSensor, &buzzerController, &lightController](IWebRequest *request, IWebResponse *response) // NOSONAR - complexity ok
              {
                  // Authentication check
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  // Get request body and parse JSON
                  String body = request->body();
                  JsonDocument jsonDoc;
                  DeserializationError error = deserializeJson(jsonDoc, body);
                  
                  if (error != DeserializationError::Ok) {
                      response->send(400, "application/json", R"({"success":false,"error":"Invalid JSON format"})");
                      return;
                  }
                  
                  JsonObject jsonObj = jsonDoc.as<JsonObject>();
                  
                  // Basic validation - check for required fields
                  if (jsonObj.isNull()) {
                    response->send(400, "application/json", R"({"success":false,"error":"Invalid JSON format"})");
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
                  if (jsonObj["water_meter_per_pulse_calculation_enabled"].is<bool>()) {
                      bool enabled = jsonObj["water_meter_per_pulse_calculation_enabled"].as<bool>();
                      settingsManager.setWaterMeterPerPulseCalculationEnabled(enabled);
                      String calcMsg = "Per-pulse flow calculation: " + String(enabled ? "enabled" : "disabled");
                      logger.logInfo(calcMsg.c_str());
                  }
                  
                  // Save settings to persistent storage
                  settingsManager.save();
                  
                  response->send(200, "application/json", R"({"success":true,"message":"Settings restored successfully"})");
              });
    
    // SPA URL rewriting - redirect client-side routes to root before static file serving
    // This prevents filesystem errors when navigating to SPA routes like /update, /settings, /logs    
    hal->webServerAddRewrite("/status", "/index.htm");
    hal->webServerAddRewrite("/settings", "/index.htm");
    hal->webServerAddRewrite("/log", "/index.htm");
    hal->webServerAddRewrite("/updates", "/index.htm");
    hal->webServerAddRewrite("/about", "/index.htm");
    
        
    // Setup ArduinoOTA - Note: ArduinoOTA is ESP32-specific, not part of HAL
    if (hostName && strlen(hostName) > 0) {
        ArduinoOTA.setHostname(hostName); // Need to set hostname in all places for mDNS to work
    }
    if (otaPasswd && strlen(otaPasswd) > 0) {
        ArduinoOTA.setPassword(otaPasswd); // Optional for authentication
        logger.logInfo("OTA password configured");
    }
    ArduinoOTA.begin();

    // Setup ElegantOTA - Note: ElegantOTA is ESP32-specific, not part of HAL
    if (otaPasswd && strlen(otaPasswd) > 0) {
        ElegantOTA.setAuth("admin", otaPasswd); // Optional: add authentication
        logger.logInfo("ElegantOTA authentication configured");
    }
    // Configure ElegantOTA for filesystem updates
    ElegantOTA.onProgress([](unsigned int progress, unsigned int total) {
        String progressMsg = "OTA Update Progress: " + String(progress / (total / 100)) + "%";
        logger.logInfo(progressMsg.c_str());
    });
    
    // Add custom callback for filesystem updates - capture this to access hal
    ElegantOTA.onStart([this]() {
        logger.logInfo("Start OTA updating ");
        hal->fsEnd();
    });
    
    ElegantOTA.onEnd([this](bool success) {
        logger.logInfo("OTA update End");
        if(success) {
            logger.logInfo("OTA update completed successfully, restarting...");
            hal->restart();
        } else {
            logger.logError("OTA update failed");
            if(!hal->fsBegin()) {
                logger.logError("Failed to re-initialize filesystem after OTA failure");
            }
        }
    });

    // TODO: username/password for ElegantOTA?
    // ElegantOTA.begin(server_);
    hal->webServerAddElegantOTA();
    

    // Static and not found placed last to catch all unmatched routes

    // Serve static files from LittleFS - LittleFS kept for AsyncWebServer serveStatic() only
    // Web assets are served from /www/ subdirectory to protect sensitive files in root
    // Historical Data Endpoints
    hal->webServerOn("/data/history", HAL_WebRequestMethod::HTTP_GET,
              [&historyManager](IWebRequest *request, IWebResponse *response)
              {
                  String jsonResponse = historyManager.getDataAsJson();
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    hal->webServerOn("/data/export_csv", HAL_WebRequestMethod::HTTP_GET,
              [&historyManager](IWebRequest *request, IWebResponse *response)
              {
                  String csvData = historyManager.getDataAsCsv();
                  response->addHeader("Content-Disposition", "attachment; filename=coop_history.csv");
                  response->send(200, "text/csv", csvData.c_str());
              });

    hal->webServerOn("/data/clear", HAL_WebRequestMethod::HTTP_POST,
              [this, &historyManager](IWebRequest *request, IWebResponse *response)
              {
                  // Authentication check
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  historyManager.clear();
                  response->send(200, "application/json", R"({"success":true})");
              });

    if(!hal->fsBegin()) {
        logger.logError("Failed to initialize filesystem for web server static file serving");
    }
    hal->webServerServeStatic("/assets/", "/www/assets/");
    hal->webServerServeStatic("/", "/www/");

    // SPA catch-all handler - serves index.htm for client-side routes
    // This prevents filesystem errors when navigating to SPA routes like /update, /settings, /logs
    hal->webServerOnNotFound([](IWebRequest *request, IWebResponse *response) {
        String uri = request->url();

        Serial.println("Handling SPA route for URI: " + uri);

        // Check if this is an API endpoint - return 404 for unknown API routes
        if (uri.startsWith("/get_") || uri.startsWith("/get_") || uri.startsWith("/pump/") || uri.startsWith("/water/") ||
            uri.startsWith("/update_settings") || uri.equals("/logs") || uri.equals("/version") ||
            uri.equals("/update") || uri.startsWith("/buzzer/") || uri.startsWith("/door/") ||
            uri.startsWith("/light/") || uri.equals("/sun/times") ||
            uri.equals("/system_status") || uri.equals("/factory_reset") ||
            uri.equals("/reboot") || uri.startsWith("/settings/") || uri.startsWith("/data/")) {
            response->send(404, "text/plain", "Not Found");
            return;
        }

        // For all other requests (client-side routes), serve index.htm
        // This allows SolidJS router to handle the route on the client side
        // Note: request->url() returns only the path (e.g., "/update"), not the full URL
        response->sendFile("/www/index.htm", "text/html");
    });
}
void CoopControllerWebServer::loop() const
{
  // Process ArduinoOTA events (network OTA)
  ArduinoOTA.handle();

  // Process ElegantOTA events (web OTA)
  ElegantOTA.loop();
}

// ============================================================================
// Authentication Methods
// ============================================================================

String CoopControllerWebServer::base64Decode(const String& input) {
    // Base64 decode table
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    String output = "";
    int val = 0;
    int valb = -8;

    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input[i];
        if (c == '=') break; // Padding character

        const char* p = strchr(base64_table, c);
        if (p == nullptr) continue; // Skip invalid characters

        val = (val << 6) | (p - base64_table);
        valb += 6;

        if (valb >= 0) {
            output += char((val >> valb) & 0xFF);
            valb -= 8;
        }
    }

    return output;
}

bool CoopControllerWebServer::isAuthenticated(void* request) {
    IWebRequest* req = static_cast<IWebRequest*>(request);

    // If authentication is disabled in settings, allow all requests
    if (!settingsManager.getApiAuthEnabled()) {
        return true;
    }

    // If password is empty, authentication is effectively disabled
    if (settingsManager.getApiPassword().length() == 0) {
        return true;
    }

    // Check for Authorization header
    if (!req->hasHeader("Authorization")) {
        return false;
    }

    // Parse "Basic <base64>" header
    String authHeader = req->header("Authorization");
    if (!authHeader.startsWith("Basic ")) {
        return false;
    }

    // Decode base64 credentials (format: "username:password")
    String credentials = base64Decode(authHeader.substring(6));
    int colonIndex = credentials.indexOf(':');
    if (colonIndex == -1) {
        return false;
    }

    String username = credentials.substring(0, colonIndex);
    String password = credentials.substring(colonIndex + 1);

    // Validate credentials against settings
    bool isValid = (username == settingsManager.getApiUsername() &&
                   password == settingsManager.getApiPassword());

    if (!isValid) {
        logger.logWarning("Failed authentication attempt from user: " + username);
    }

    return isValid;
}

void CoopControllerWebServer::sendAuthRequired(void* response) {
    IWebResponse* resp = static_cast<IWebResponse*>(response);

    // Send 401 Unauthorized with WWW-Authenticate header
    resp->addHeader("WWW-Authenticate", "Basic realm=\"Coop Controller\"");
    resp->send(401, "text/plain", "Authentication required");
}
