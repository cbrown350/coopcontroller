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
#include "TriggerSource.h"
#include "NotificationManager.h"

#include <stdint.h>
#include <memory>
#include <algorithm>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>

// Validate a WiFi BSSID preference string. Accepts colon or hyphen separated
// hex (XX:XX:XX:XX:XX:XX / XX-XX-... ) or 12 contiguous hex digits. Empty
// string is valid (means auto-select). Returns true and fills normalized
// (colon-separated, uppercase) on success; returns false otherwise.
static bool isValidBssidPref(const String& in, String& normalized) {
    normalized = "";
    String trimmed = in;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return true;  // empty = auto-select
    }
    // Collect only the hex digits, ignore ':' and '-' separators.
    String hex = "";
    for (unsigned int i = 0; i < trimmed.length(); i++) {
        char c = trimmed.charAt(i);
        if (c == ':' || c == '-' || c == ' ') continue;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            hex += (char)toupper(c);
        } else {
            return false;  // invalid character
        }
    }
    if (hex.length() != 12) return false;  // must be exactly 6 bytes
    char buf[18];
    for (int i = 0; i < 6; i++) {
        buf[i * 3] = hex.charAt(i * 2);
        buf[i * 3 + 1] = hex.charAt(i * 2 + 1);
        buf[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
    normalized = String(buf);
    return true;
}

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

                  // Validate BSSID preference early so a malformed value never
                  // reaches NVS (a bad BSSID strands the board off the network
                  // until manually cleared). Empty is allowed (auto-select).
                  if (jsonObj["wifi_bssid_preference"].is<const char*>()) {
                      String normalized;
                      String raw = jsonObj["wifi_bssid_preference"].as<String>();
                      if (!isValidBssidPref(raw, normalized)) {
                          logger.logWarning(("Rejecting invalid wifi_bssid_preference: " + raw).c_str());
                          response->send(400, "application/json",
                              R"({"success":false,"error":"Invalid BSSID format. Use XX:XX:XX:XX:XX:XX (or leave empty for auto-select)."})");
                          return;
                      }
                      jsonObj["wifi_bssid_preference"] = normalized;  // store normalized form
                  }

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

                  if (jsonObj["hostname"].is<const char*>()) {
                      settingsManager.setHostname(jsonObj["hostname"].as<String>());
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
                  if (jsonObj["door_open_timeout_seconds"].is<int>()) {
                      settingsManager.setDoorOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
                      doorController.setOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
                  }
                  if (jsonObj["door_close_timeout_seconds"].is<int>()) {
                      settingsManager.setDoorCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
                      doorController.setCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
                  }
                  if (jsonObj["door_auto_open_enabled"].is<bool>()) {
                      bool en = jsonObj["door_auto_open_enabled"].as<bool>();
                      settingsManager.setDoorAutoOpenEnabled(en);
                      doorController.setAutoOpenEnabled(en, TriggerSource::WEB_UI);
                  }
                  if (jsonObj["door_auto_open_offset_minutes"].is<int>()) {
                      int m = jsonObj["door_auto_open_offset_minutes"].as<int>();
                      settingsManager.setDoorAutoOpenOffsetMinutes(m);
                      doorController.setAutoOpenOffsetMinutes(m);
                  }
                  if (jsonObj["door_auto_open_days"].is<JsonArray>()) {
                      JsonArrayConst arr = jsonObj["door_auto_open_days"].as<JsonArrayConst>();
                      for (int i = 0; i < 7 && i < (int)arr.size(); i++) {
                          if (arr[i].is<bool>()) {
                              settingsManager.setDoorAutoOpenDay(i, arr[i].as<bool>());
                              doorController.setAutoOpenDay(i, arr[i].as<bool>());
                          }
                      }
                  }
                  if (jsonObj["door_auto_close_enabled"].is<bool>()) {
                      bool en = jsonObj["door_auto_close_enabled"].as<bool>();
                      settingsManager.setDoorAutoCloseEnabled(en);
                      doorController.setAutoCloseEnabled(en, TriggerSource::WEB_UI);
                  }
                  if (jsonObj["door_auto_close_offset_minutes"].is<int>()) {
                      int m = jsonObj["door_auto_close_offset_minutes"].as<int>();
                      settingsManager.setDoorAutoCloseOffsetMinutes(m);
                      doorController.setAutoCloseOffsetMinutes(m);
                  }
                  if (jsonObj["door_auto_close_days"].is<JsonArray>()) {
                      JsonArrayConst arr = jsonObj["door_auto_close_days"].as<JsonArrayConst>();
                      for (int i = 0; i < 7 && i < (int)arr.size(); i++) {
                          if (arr[i].is<bool>()) {
                              settingsManager.setDoorAutoCloseDay(i, arr[i].as<bool>());
                              doorController.setAutoCloseDay(i, arr[i].as<bool>());
                          }
                      }
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
                  if (jsonObj["timezone_posix"].is<const char*>()) {
                      settingsManager.setTimezonePosix(jsonObj["timezone_posix"].as<String>());
                      locationChanged = true;
                  }

                  // Recalculate sunrise/sunset if location changed
                  if (locationChanged) {
                      String tzPosix = settingsManager.getTimezonePosix();
#ifdef ESP32
                      // Auto-detect timezone from coordinates if not explicitly set
                      if (tzPosix.length() == 0) {
                          float lat = settingsManager.getLatitude();
                          float lon = settingsManager.getLongitude();
                          if (lat >= 24.0f && lat <= 50.0f && lon >= -125.0f && lon <= -66.0f) {
                              if (lon >= -87.5f)       tzPosix = "EST5EDT,M3.2.0,M11.1.0";
                              else if (lon >= -104.0f) tzPosix = "CST6CDT,M3.2.0,M11.1.0";
                              else if (lon >= -115.0f) tzPosix = "MST7MDT,M3.2.0,M11.1.0";
                              else                     tzPosix = "PST8PDT,M3.2.0,M11.1.0";
                          } else if (lat >= 51.0f && lon <= -130.0f) {
                              tzPosix = "AKST9AKDT,M3.2.0,M11.1.0";
                          } else if (lat >= 18.0f && lat <= 23.0f && lon >= -161.0f && lon <= -154.0f) {
                              tzPosix = "HST10";
                          }
                          if (tzPosix.length() > 0) {
                              settingsManager.setTimezonePosix(tzPosix);
                              logger.logfInfo("Timezone auto-detected from coordinates: %s", tzPosix.c_str());
                          }
                      }
                      // Apply timezone via configTzTime for DST-aware local time
                      if (tzPosix.length() > 0) {
                          configTzTime(tzPosix.c_str(), "pool.ntp.org");
                      } else {
                          int offset = settingsManager.getTimezoneOffsetHours();
                          String tz = "UTC" + String(-offset);
                          configTzTime(tz.c_str(), "pool.ntp.org");
                      }
#endif
                      sunriseSunset.setCoordinates(settingsManager.getLatitude(),
                                          settingsManager.getLongitude(),
                                          settingsManager.getTimezoneOffsetHours());
                      sunriseSunset.forceUpdate();
                      logger.logInfo("Location settings updated, sunrise/sunset recalculated");
                  }
                  
                  // Handle door advanced features settings
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

                  if (jsonObj["pump_off_flow_pulse_threshold"].is<int>()) {
                      unsigned int threshold = jsonObj["pump_off_flow_pulse_threshold"].as<unsigned int>();
                      settingsManager.setPumpOffFlowPulseThreshold(threshold);
                      String threshMsg = "Pump OFF flow pulse threshold: " + String(threshold);
                      logger.logInfo(threshMsg.c_str());
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
                                              settingsManager.getHostname().c_str());
                  }

                  // Handle flow calculation interval
                  if (jsonObj["flow_calculation_interval_seconds"].is<int>()) {
                      unsigned int interval = jsonObj["flow_calculation_interval_seconds"].as<unsigned int>();
                      settingsManager.setFlowCalculationIntervalSeconds(interval);
                      tempSensor.setFlowCalculationIntervalSeconds(interval);
                      logger.logfInfo("Flow calculation interval: %u seconds", interval);
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
                      unsigned int bufSize = jsonObj["history_buffer_size"].as<unsigned int>();
                      settingsManager.setHistoryBufferSize(bufSize);
                      historyManager.setBufferSize(bufSize);
                  }

                  // OTA update settings
                  if (jsonObj["auto_update_enabled"].is<bool>()) {
                      settingsManager.setAutoUpdateEnabled(jsonObj["auto_update_enabled"].as<bool>());
                  }
                  if (jsonObj["update_check_interval_hours"].is<int>()) {
                      settingsManager.setUpdateCheckIntervalHours(jsonObj["update_check_interval_hours"].as<unsigned int>());
                  }
                  if (jsonObj["manifest_url"].is<String>()) {
                      settingsManager.setManifestUrl(jsonObj["manifest_url"].as<String>());
                      // Apply at runtime so it takes effect without a reboot.
                      if (updateManager_) updateManager_->setManifestUrl(settingsManager.getManifestUrl());
                  }

                  // Handle MQTT settings
                  if (jsonObj["mqtt_enabled"].is<bool>()) {
                      settingsManager.setMqttEnabled(jsonObj["mqtt_enabled"].as<bool>());
                      if (mqttManager_) mqttManager_->setEnabled(jsonObj["mqtt_enabled"].as<bool>());
                  }
                  if (jsonObj["mqtt_server"].is<String>()) {
                      settingsManager.setMqttServer(jsonObj["mqtt_server"].as<String>());
                  }
                  if (jsonObj["mqtt_port"].is<int>()) {
                      settingsManager.setMqttPort(jsonObj["mqtt_port"].as<uint16_t>());
                  }
                  if (jsonObj["mqtt_username"].is<String>()) {
                      settingsManager.setMqttUsername(jsonObj["mqtt_username"].as<String>());
                  }
                  if (jsonObj["mqtt_password"].is<String>()) {
                      settingsManager.setMqttPassword(jsonObj["mqtt_password"].as<String>());
                  }
                  // Apply MQTT config changes at runtime
                  if (mqttManager_ && (jsonObj["mqtt_server"].is<String>() || jsonObj["mqtt_port"].is<int>() ||
                      jsonObj["mqtt_username"].is<String>() || jsonObj["mqtt_password"].is<String>())) {
                      MQTTConfig mqttConfig;
                      mqttConfig.server = settingsManager.getMqttServer();
                      mqttConfig.port = settingsManager.getMqttPort();
                      mqttConfig.username = settingsManager.getMqttUsername();
                      mqttConfig.password = settingsManager.getMqttPassword();
                      mqttConfig.device_id = mqttManager_->getConfig().device_id;
                      mqttConfig.device_name = mqttManager_->getConfig().device_name;
                      mqttConfig.fw_version = mqttManager_->getConfig().fw_version;
                      mqttManager_->setConfig(mqttConfig);
                  }

                  // Handle weather (OpenWeatherMap) settings
                  {
                      bool weatherChanged = false;
                      if (jsonObj["weather_enabled"].is<bool>()) {
                          bool en = jsonObj["weather_enabled"].as<bool>();
                          settingsManager.setWeatherEnabled(en);
                          if (weatherManager_) weatherManager_->setEnabled(en);
                          weatherChanged = true;
                      }
                      if (jsonObj["weather_api_key"].is<String>()) {
                          String key = jsonObj["weather_api_key"].as<String>();
                          settingsManager.setWeatherApiKey(key);
                          if (weatherManager_) weatherManager_->setApiKey(key);
                          weatherChanged = true;
                      }
                      if (jsonObj["weather_units"].is<String>()) {
                          settingsManager.setWeatherUnits(jsonObj["weather_units"].as<String>());
                          if (weatherManager_) weatherManager_->setUnits(settingsManager.getWeatherUnits());
                          weatherChanged = true;
                      }
                      if (jsonObj["weather_update_interval_minutes"].is<int>()) {
                          settingsManager.setWeatherUpdateIntervalMinutes(jsonObj["weather_update_interval_minutes"].as<unsigned int>());
                          if (weatherManager_) weatherManager_->setUpdateIntervalMinutes(settingsManager.getWeatherUpdateIntervalMinutes());
                          weatherChanged = true;
                      }
                      // Keep the weather location in sync with the coop location.
                      if (weatherManager_ && locationChanged) {
                          weatherManager_->setLocation(settingsManager.getLatitude(), settingsManager.getLongitude());
                      }
                      // Note: do NOT trigger a synchronous forceRefresh() from this
                      // async web handler — TLS/JSON work must stay on the loop
                      // task (issue #4). The next loop tick will refresh if enabled.
                      (void)weatherChanged;
                  }

                  // Handle LLM weather-decider settings (issue #6)
                  {
                      bool llmChanged = false;
                      bool llmEnabled = settingsManager.getLlmEnabled();
                      String llmBaseUrl = settingsManager.getLlmBaseUrl();
                      String llmApiKey = settingsManager.getLlmApiKey();
                      String llmModel = settingsManager.getLlmModel();
                      String llmType = settingsManager.getLlmProviderType();
                      unsigned int llmTimeout = settingsManager.getLlmTimeoutSeconds();
                      String llmPrompt = settingsManager.getLlmPromptOverride();
                      if (jsonObj["llm_enabled"].is<bool>()) {
                          llmEnabled = jsonObj["llm_enabled"].as<bool>();
                          settingsManager.setLlmEnabled(llmEnabled);
                          llmChanged = true;
                      }
                      if (jsonObj["llm_provider_type"].is<String>()) {
                          llmType = jsonObj["llm_provider_type"].as<String>();
                          settingsManager.setLlmProviderType(llmType);
                          llmChanged = true;
                      }
                      if (jsonObj["llm_base_url"].is<String>()) {
                          llmBaseUrl = jsonObj["llm_base_url"].as<String>();
                          settingsManager.setLlmBaseUrl(llmBaseUrl);
                          llmChanged = true;
                      }
                      if (jsonObj["llm_api_key"].is<String>()) {
                          String key = jsonObj["llm_api_key"].as<String>();
                          // Empty string from UI means "keep existing" — only
                          // overwrite when a real value is sent (matches the
                          // weather_api_key convention above).
                          if (key.length() > 0) {
                              llmApiKey = key;
                              settingsManager.setLlmApiKey(key);
                              llmChanged = true;
                          }
                      }
                      if (jsonObj["llm_model"].is<String>()) {
                          llmModel = jsonObj["llm_model"].as<String>();
                          settingsManager.setLlmModel(llmModel);
                          llmChanged = true;
                      }
                      if (jsonObj["llm_timeout_seconds"].is<int>()) {
                          llmTimeout = jsonObj["llm_timeout_seconds"].as<unsigned int>();
                          settingsManager.setLlmTimeoutSeconds(llmTimeout);
                          llmChanged = true;
                      }
                      // Custom judgment-guidance override (issue #8). Unlike the
                      // api_key, an empty value is meaningful here — it means
                      // "use the firmware default," so we always accept the field
                      // when the UI sends it (Reset to Default sends "").
                      if (jsonObj["llm_prompt_override"].is<String>()) {
                          llmPrompt = jsonObj["llm_prompt_override"].as<String>();
                          settingsManager.setLlmPromptOverride(llmPrompt);
                          llmChanged = true;
                      }
                      // Reconfigure the active decider immediately on change.
                      // configureLlmDecider() runs entirely on this async task
                      // but only does string copies + a unique_ptr swap (no
                      // TLS/JSON work), so it's safe here, unlike forceRefresh().
                      if (weatherManager_ && llmChanged) {
                          weatherManager_->configureLlmDecider(llmEnabled, llmBaseUrl, llmApiKey,
                                                               llmModel, llmType, llmTimeout,
                                                               llmPrompt);
                      }
                  }

                  // Handle notification settings - Telegram
                  if (jsonObj["telegram_enabled"].is<bool>()) {
                      settingsManager.setTelegramEnabled(jsonObj["telegram_enabled"].as<bool>());
                      if (telegramBot_) {
                          telegramBot_->setEnabled(jsonObj["telegram_enabled"].as<bool>());
                          telegramBot_->setPollingEnabled(jsonObj["telegram_enabled"].as<bool>());
                      }
                  }
                  if (jsonObj["telegram_bot_token"].is<String>()) {
                      settingsManager.setTelegramBotToken(jsonObj["telegram_bot_token"].as<String>());
                      if (telegramBot_) telegramBot_->setBotToken(jsonObj["telegram_bot_token"].as<String>());
                  }
                  if (jsonObj["telegram_chat_id"].is<String>()) {
                      settingsManager.setTelegramChatId(jsonObj["telegram_chat_id"].as<String>());
                      if (telegramBot_) telegramBot_->setChatId(jsonObj["telegram_chat_id"].as<String>());
                  }
                  if (jsonObj["telegram_polling_interval_seconds"].is<int>()) {
                      unsigned int seconds = jsonObj["telegram_polling_interval_seconds"].as<unsigned int>();
                      settingsManager.setTelegramPollingIntervalSeconds(seconds);
                      if (telegramBot_ && seconds >= 10 && seconds <= 300) {
                          telegramBot_->setPollingIntervalMs(seconds * 1000UL);
                      }
                  }

                  // Handle notification settings - Email
                  if (jsonObj["email_enabled"].is<bool>()) {
                      settingsManager.setEmailEnabled(jsonObj["email_enabled"].as<bool>());
                      if (notificationManager_) notificationManager_->setEmailEnabled(jsonObj["email_enabled"].as<bool>());
                  }
                  if (jsonObj["email_smtp_server"].is<String>()) {
                      settingsManager.setEmailSmtpServer(jsonObj["email_smtp_server"].as<String>());
                      if (notificationManager_) notificationManager_->setSmtpServer(jsonObj["email_smtp_server"].as<String>());
                  }
                  if (jsonObj["email_smtp_port"].is<int>()) {
                      settingsManager.setEmailSmtpPort(jsonObj["email_smtp_port"].as<uint16_t>());
                      if (notificationManager_) notificationManager_->setSmtpPort(jsonObj["email_smtp_port"].as<uint16_t>());
                  }
                  if (jsonObj["email_smtp_username"].is<String>()) {
                      settingsManager.setEmailSmtpUsername(jsonObj["email_smtp_username"].as<String>());
                      if (notificationManager_) notificationManager_->setSmtpUsername(jsonObj["email_smtp_username"].as<String>());
                  }
                  if (jsonObj["email_smtp_password"].is<String>()) {
                      settingsManager.setEmailSmtpPassword(jsonObj["email_smtp_password"].as<String>());
                      if (notificationManager_) notificationManager_->setSmtpPassword(jsonObj["email_smtp_password"].as<String>());
                  }
                  if (jsonObj["email_from"].is<String>()) {
                      settingsManager.setEmailFrom(jsonObj["email_from"].as<String>());
                      if (notificationManager_) notificationManager_->setEmailFrom(jsonObj["email_from"].as<String>());
                  }
                  if (jsonObj["email_to"].is<String>()) {
                      settingsManager.setEmailTo(jsonObj["email_to"].as<String>());
                      if (notificationManager_) notificationManager_->setEmailTo(jsonObj["email_to"].as<String>());
                  }

                  // Handle notification preferences
                  if (jsonObj["notify_pump_error"].is<bool>()) {
                      settingsManager.setNotifyPumpError(jsonObj["notify_pump_error"].as<bool>());
                      if (notificationManager_) notificationManager_->setNotifyOnPumpError(jsonObj["notify_pump_error"].as<bool>());
                  }
                  if (jsonObj["notify_sensor_error"].is<bool>()) {
                      settingsManager.setNotifySensorError(jsonObj["notify_sensor_error"].as<bool>());
                      if (notificationManager_) notificationManager_->setNotifyOnSensorError(jsonObj["notify_sensor_error"].as<bool>());
                  }
                  if (jsonObj["notify_door_fault"].is<bool>()) {
                      settingsManager.setNotifyDoorFault(jsonObj["notify_door_fault"].as<bool>());
                      if (notificationManager_) notificationManager_->setNotifyOnDoorFault(jsonObj["notify_door_fault"].as<bool>());
                  }
                  if (jsonObj["notify_wifi_disconnect"].is<bool>()) {
                      settingsManager.setNotifyWifiDisconnect(jsonObj["notify_wifi_disconnect"].as<bool>());
                      if (notificationManager_) notificationManager_->setNotifyOnWifiDisconnect(jsonObj["notify_wifi_disconnect"].as<bool>());
                  }
                  if (jsonObj["notify_system_error"].is<bool>()) {
                      settingsManager.setNotifySystemError(jsonObj["notify_system_error"].as<bool>());
                      if (notificationManager_) notificationManager_->setNotifyOnSystemError(jsonObj["notify_system_error"].as<bool>());
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
                  system["pump_off_flow_pulse_threshold"] = settingsManager.getPumpOffFlowPulseThreshold();
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

                  pumpController.turnOn(TriggerSource::WEB_UI);
                  logger.logInfo("Pump turned on via web UI");
                  response->send(200, "text/plain", "Pump turned on");
              });

    hal->webServerOn("/pump/off", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.turnOff(TriggerSource::WEB_UI);
                  logger.logInfo("Pump turned off via web UI");
                  response->send(200, "text/plain", "Pump turned off");
              });

    hal->webServerOn("/pump/auto", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.setAutoMode(true, TriggerSource::WEB_UI);
                  response->send(200, "text/plain", "Pump set to auto mode");
              });

    hal->webServerOn("/pump/force_cycle", HAL_WebRequestMethod::HTTP_GET,
              [this, &pumpController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  pumpController.forceCycle(TriggerSource::WEB_UI);
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

                  doorController.open(TriggerSource::WEB_UI);
                  response->send(200, "text/plain", "Door opening");
              });
    
    hal->webServerOn("/door/close", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.close(TriggerSource::WEB_UI);
                  response->send(200, "text/plain", "Door closing");
              });
    
    hal->webServerOn("/door/stop", HAL_WebRequestMethod::HTTP_GET,
              [this, &doorController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  doorController.stop(TriggerSource::WEB_UI);
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
                  // Legacy single-toggle endpoint: enables both auto-open and auto-close
                  // together for backward compatibility with existing UI/MQTT callers.
                  doorController.setAutoOpenEnabled(autoMode, TriggerSource::WEB_UI);
                  doorController.setAutoCloseEnabled(autoMode, TriggerSource::WEB_UI);
                  settingsManager.setDoorAutoOpenEnabled(autoMode);
                  settingsManager.setDoorAutoCloseEnabled(autoMode);
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

                  lightController.turnOn(TriggerSource::WEB_UI);
                  logger.logInfo("Light turned on via web UI");
                  response->send(200, "text/plain", "Light turned on");
              });
    
    hal->webServerOn("/light/off", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.turnOff(TriggerSource::WEB_UI);
                  logger.logInfo("Light turned off via web UI");
                  response->send(200, "text/plain", "Light turned off");
              });
    
    hal->webServerOn("/light/fade_in", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.fadeIn(TriggerSource::WEB_UI);
                  response->send(200, "text/plain", "Light fading in");
              });
    
    hal->webServerOn("/light/fade_out", HAL_WebRequestMethod::HTTP_GET,
              [this, &lightController](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }

                  lightController.fadeOut(TriggerSource::WEB_UI);
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
                  
                  lightController.setBrightness(brightness, TriggerSource::WEB_UI);
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
                  lightController.setAutoMode(autoMode, TriggerSource::WEB_UI);
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

    // Logs endpoint (chunked streaming to avoid memory exhaustion)
    hal->webServerOn("/logs", HAL_WebRequestMethod::HTTP_GET,
              [](IWebRequest *request, IWebResponse *response)
              {
                  size_t totalEntries = logger.getLogCount();

                  struct LogStreamState {
                      size_t total;
                      size_t current;
                      String overflow;
                      bool started;
                      bool closedBracket;
                      int startIndex;
                  };
                  auto state = std::make_shared<LogStreamState>();
                  state->total = totalEntries;
                  state->current = 0;
                  state->started = false;
                  state->closedBracket = false;
                  state->startIndex = logger.getStartIndex();

                  response->sendChunked(200, "application/json",
                      [state](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                          // This callback runs on the async_tcp task, where an
                          // uncaught std::bad_alloc (heap-allocating String ops
                          // under memory pressure) panics and reboots the board.
                          // Catch any exception, end the stream gracefully, and
                          // mark closed so the client gets a clean termination
                          // instead of a crash. Returns 0 = end of chunked body.
                          try {
                          if (state->closedBracket && state->overflow.length() == 0) return 0;

                          size_t written = 0;

                          // Flush overflow from previous call
                          if (state->overflow.length() > 0) {
                              size_t toWrite = std::min(static_cast<size_t>(state->overflow.length()), maxLen);
                              memcpy(buffer, state->overflow.c_str(), toWrite);
                              written += toWrite;
                              if (toWrite < state->overflow.length()) {
                                  state->overflow = state->overflow.substring(toWrite);
                              } else {
                                  state->overflow = "";
                              }
                              if (written >= maxLen) return written;
                          }

                          // Opening
                          if (!state->started) {
                              const char* prefix = "{\"logs\":[";
                              size_t prefixLen = strlen(prefix);
                              size_t toWrite = std::min(prefixLen, maxLen - written);
                              memcpy(buffer + written, prefix, toWrite);
                              written += toWrite;
                              if (toWrite < prefixLen) {
                                  state->overflow = String(prefix + toWrite);
                              }
                              state->started = true;
                              if (written >= maxLen) return written;
                          }

                          // Write log entries one at a time
                          while (state->current < state->total && written < maxLen) {
                              int bufIdx = (state->startIndex + static_cast<int>(state->current)) % 150;
                              // getLogEntryAt returns a by-value snapshot taken under
                              // the log mutex; binding it to a const ref extends the
                              // temporary's lifetime for this iteration so the main
                              // loop can't tear this entry mid-read.
                              const LogEntry entry = logger.getLogEntryAt(bufIdx);

                              String point;
                              if (state->current > 0) point += ",";
                              point += "{\"uuid\":\"";
                              point += entry.uuid;
                              point += "\",\"timestamp\":";
                              point += String(entry.timestamp);
                              point += ",\"message\":\"";
                              // Escape quotes and backslashes in message
                              for (size_t i = 0; entry.message[i] != '\0'; i++) {
                                  char c = entry.message[i];
                                  if (c == '"' || c == '\\') point += '\\';
                                  point += c;
                              }
                              point += "\",\"level\":\"";
                              point += logger.logLevelToString(entry.level);
                              point += "\"}";

                              state->current++;

                              size_t available = maxLen - written;
                              size_t toWrite = std::min(static_cast<size_t>(point.length()), available);
                              memcpy(buffer + written, point.c_str(), toWrite);
                              written += toWrite;

                              if (toWrite < point.length()) {
                                  state->overflow = point.substring(toWrite);
                                  return written;
                              }
                          }

                          // Closing
                          if (state->current >= state->total && !state->closedBracket) {
                              const char* suffix = "]}";
                              size_t suffixLen = strlen(suffix);
                              if (written + suffixLen <= maxLen) {
                                  memcpy(buffer + written, suffix, suffixLen);
                                  written += suffixLen;
                                  state->closedBracket = true;
                              } else {
                                  size_t available = maxLen - written;
                                  memcpy(buffer + written, suffix, available);
                                  written += available;
                                  state->overflow = String(suffix + available);
                                  state->closedBracket = true;
                              }
                          }

                          return written;
                          } catch (...) {
                              // Out-of-memory or other allocation failure on async_tcp:
                              // end the stream so the framework tears the connection
                              // down cleanly instead of aborting the task.
                              state->closedBracket = true;
                              state->overflow = "";
                              return 0;
                          }
                      });
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
                  doc["timezone_posix"] = settingsManager.getTimezonePosix();
                  
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
                  formatted += String(uptimeSeconds) + "s";
                  
                  jsonDoc["uptime_formatted"] = formatted;
                  
                  // Chip information
                  jsonDoc["chip_model"] = hal->getChipModel();
                  // Note: getCpuFreqMHz and getFlashChipSize are not in HAL, keeping as ESP calls for now
                  jsonDoc["cpu_freq_mhz"] = hal->getCpuFreqMHz();
                  jsonDoc["flash_size"] = hal->getFlashChipSize();

                  // Last reboot reason. esp_reset_reason() reflects the most
                  // recent reset (kept in RTC memory across a reboot), so this
                  // is why the board came back up this boot. BROWNOUT=9,
                  // PANIC=4, *_WDT=5/6/7, POWERON=1, SW=3.
                  uint8_t resetCode = hal->getResetReason();
                  jsonDoc["reset_reason_code"] = resetCode;
                  jsonDoc["reset_reason"] = resetReasonToString(resetCode);

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
                  jsonDoc["github_repo"]      = githubRepo;

                  // Add git commit SHA if available
                  if (strlen(gitCommitSha) > 0) {
                      jsonDoc["git_commit_sha"] = gitCommitSha;
                  }

                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    // ========================================================================
    // OTA UPDATE ENDPOINTS
    // ========================================================================

    // Check for updates: enqueue only. GitHub TLS must run from the main loop,
    // never in the async_tcp callback (concurrent TLS caused heap collapse and
    // ESP_RST_PANIC on production).
    hal->webServerOn("/update/check", HAL_WebRequestMethod::HTTP_GET,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  if (!updateManager_) {
                      response->send(503, "application/json", "{\"error\":\"Update manager not available\"}");
                      return;
                  }
                  updateManager_->requestCheck();
                  response->send(202, "application/json", "{\"status\":\"checking\"}");
              });

    // Return the most recent check result without starting network I/O.
    hal->webServerOn("/update/check_result", HAL_WebRequestMethod::HTTP_GET,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  if (!updateManager_) {
                      response->send(503, "application/json", "{\"error\":\"Update manager not available\"}");
                      return;
                  }
                  JsonDocument doc = updateManager_->getCheckResponseJson();
                  String jsonResponse;
                  serializeJson(doc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    // Get update status
    hal->webServerOn("/update/status", HAL_WebRequestMethod::HTTP_GET,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  if (!updateManager_) {
                      response->send(503, "application/json", "{\"error\":\"Update manager not available\"}");
                      return;
                  }
                  JsonDocument doc = updateManager_->getStatusResponseJson();
                  String jsonResponse;
                  serializeJson(doc, jsonResponse);
                  response->send(200, "application/json", jsonResponse.c_str());
              });

    // Install update (requires authentication)
    hal->webServerOn("/update/install", HAL_WebRequestMethod::HTTP_POST,
              [this](IWebRequest *request, IWebResponse *response)
              {
                  if (!isAuthenticated(request)) {
                      sendAuthRequired(response);
                      return;
                  }
                  if (!updateManager_) {
                      response->send(503, "application/json", R"({"error":"Update manager not available"})");
                      return;
                  }
                  const JsonVariant &json = request->jsonBody();
                  bool skipFs = false;
                  bool force = false;
                  if (!json.isNull()) {
                      skipFs = json["skip_filesystem"] | false;
                      force = json["force"] | false;
                  }

                  if (!force && !updateManager_->isUpdateAvailable()) {
                      response->send(400, "application/json", R"({"error":"No update available"})");
                      return;
                  }

                  // Request deferred install (runs from main loop so web server can serve status)
                  updateManager_->requestInstall(skipFs, force);

                  response->send(200, "application/json", R"({"status":"installing","message":"Update starting..."})");

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
                  if (jsonObj["door_open_timeout_seconds"].is<int>()) {
                    settingsManager.setDoorOpenTimeoutSeconds(jsonObj["door_open_timeout_seconds"].as<int>());
                  }
                  if (jsonObj["door_close_timeout_seconds"].is<int>()) {
                    settingsManager.setDoorCloseTimeoutSeconds(jsonObj["door_close_timeout_seconds"].as<int>());
                  }
                  if (jsonObj["door_auto_open_enabled"].is<bool>()) {
                    settingsManager.setDoorAutoOpenEnabled(jsonObj["door_auto_open_enabled"].as<bool>());
                  }
                  if (jsonObj["door_auto_open_offset_minutes"].is<int>()) {
                    settingsManager.setDoorAutoOpenOffsetMinutes(jsonObj["door_auto_open_offset_minutes"].as<int>());
                  }
                  if (jsonObj["door_auto_open_days"].is<JsonArray>()) {
                    JsonArrayConst arr = jsonObj["door_auto_open_days"].as<JsonArrayConst>();
                    for (int i = 0; i < 7 && i < (int)arr.size(); i++) {
                        if (arr[i].is<bool>()) settingsManager.setDoorAutoOpenDay(i, arr[i].as<bool>());
                    }
                  }
                  if (jsonObj["door_auto_close_enabled"].is<bool>()) {
                    settingsManager.setDoorAutoCloseEnabled(jsonObj["door_auto_close_enabled"].as<bool>());
                  }
                  if (jsonObj["door_auto_close_offset_minutes"].is<int>()) {
                    settingsManager.setDoorAutoCloseOffsetMinutes(jsonObj["door_auto_close_offset_minutes"].as<int>());
                  }
                  if (jsonObj["door_auto_close_days"].is<JsonArray>()) {
                    JsonArrayConst arr = jsonObj["door_auto_close_days"].as<JsonArrayConst>();
                    for (int i = 0; i < 7 && i < (int)arr.size(); i++) {
                        if (arr[i].is<bool>()) settingsManager.setDoorAutoCloseDay(i, arr[i].as<bool>());
                    }
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
    if (settingsManager.getHostname().length() > 0) {
        ArduinoOTA.setHostname(settingsManager.getHostname().c_str()); // Need to set hostname in all places for mDNS to work
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
        // Back up settings to NVS before ElegantOTA flashes anything. A filesystem
        // update overwrites /user_settings.json on LittleFS with the image's default
        // file; without this backup the user's settings are lost. restoreFromNVS()
        // (called in SettingsManager::begin on boot) reapplies the backup after the
        // update. Harmless for firmware-only updates (backup sits unused, cleared on
        // the next restore). The UpdateManager/GitHub path already does this; this
        // closes the gap for ElegantOTA (web UI) updates.
        if (!settingsManager.backupToNVS()) {
            logger.logWarning("Failed to back up settings to NVS before OTA - settings may be lost if this is a filesystem update");
        }
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
    // Historical Data Endpoints (chunked streaming to avoid memory exhaustion)
    hal->webServerOn("/data/history", HAL_WebRequestMethod::HTTP_GET,
              [&historyManager](IWebRequest *request, IWebResponse *response)
              {
                  size_t totalPoints = historyManager.getDataPointCount();

                  // Shared state for chunked callback
                  struct StreamState {
                      size_t total;
                      size_t current;
                      String overflow;
                      bool started;
                      bool closedBracket;
                      const HistoricalDataManager* mgr;
                  };
                  auto state = std::make_shared<StreamState>();
                  state->total = totalPoints;
                  state->current = 0;
                  state->started = false;
                  state->closedBracket = false;
                  state->mgr = &historyManager;

                  response->sendChunked(200, "application/json",
                      [state](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                          if (state->closedBracket && state->overflow.length() == 0) return 0;

                          size_t written = 0;

                          // Flush overflow from previous call
                          if (state->overflow.length() > 0) {
                              size_t toWrite = std::min(static_cast<size_t>(state->overflow.length()), maxLen);
                              memcpy(buffer, state->overflow.c_str(), toWrite);
                              written += toWrite;
                              if (toWrite < state->overflow.length()) {
                                  state->overflow = state->overflow.substring(toWrite);
                              } else {
                                  state->overflow = "";
                              }
                              if (written >= maxLen) return written;
                          }

                          // Opening bracket
                          if (!state->started) {
                              buffer[written++] = '[';
                              state->started = true;
                              if (written >= maxLen) return written;
                          }

                          // Write data points one at a time
                          while (state->current < state->total && written < maxLen) {
                              size_t bufIdx = state->mgr->getOrderedIndex(state->current);
                              const DataPoint& dp = state->mgr->getDataPointAt(bufIdx);

                              String point;
                              if (state->current > 0) point += ",";
                              point += "{\"timestamp\":";
                              point += String((unsigned long)dp.timestamp);
                              point += ",\"temperature_f\":";
                              if (isnan(dp.temperature_f)) {
                                  point += "null";
                              } else {
                                  point += String(dp.temperature_f, 2);
                              }
                              point += ",\"pump_active\":";
                              point += dp.pump_active ? "true" : "false";
                              point += ",\"flow_rate\":";
                              point += String(dp.flow_rate, 3);
                              point += ",\"light_brightness\":";
                              point += String(dp.light_brightness);
                              point += ",\"door_state\":\"";
                              point += dp.door_state;
                              point += "\",\"door_position\":\"";
                              point += dp.door_position;
                              point += "\",\"pump_trigger\":\"";
                              point += dp.pump_trigger;
                              point += "\",\"door_trigger\":\"";
                              point += dp.door_trigger;
                              point += "\",\"light_trigger\":\"";
                              point += dp.light_trigger;
                              point += "\",\"event_type\":\"";
                              point += dp.event_type;
                              point += "\"}";

                              state->current++;

                              size_t available = maxLen - written;
                              size_t toWrite = std::min(static_cast<size_t>(point.length()), available);
                              memcpy(buffer + written, point.c_str(), toWrite);
                              written += toWrite;

                              if (toWrite < point.length()) {
                                  state->overflow = point.substring(toWrite);
                                  return written;
                              }
                          }

                          // Closing bracket
                          if (state->current >= state->total && !state->closedBracket) {
                              if (written < maxLen) {
                                  buffer[written++] = ']';
                                  state->closedBracket = true;
                              } else {
                                  state->overflow = "]";
                              }
                          }

                          return written;
                      });
              });

    hal->webServerOn("/data/export_csv", HAL_WebRequestMethod::HTTP_GET,
              [&historyManager](IWebRequest *request, IWebResponse *response)
              {
                  size_t totalPoints = historyManager.getDataPointCount();

                  struct CsvStreamState {
                      size_t total;
                      size_t current;
                      String overflow;
                      bool headerSent;
                      const HistoricalDataManager* mgr;
                  };
                  auto state = std::make_shared<CsvStreamState>();
                  state->total = totalPoints;
                  state->current = 0;
                  state->headerSent = false;
                  state->mgr = &historyManager;

                  response->addHeader("Content-Disposition", "attachment; filename=coop_history.csv");
                  response->sendChunked(200, "text/csv",
                      [state](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                          // Runs on async_tcp: an uncaught bad_alloc here panics
                          // and reboots (decoded backtrace: std::__throw_bad_alloc
                          // in this callback). Catch any exception, end the stream
                          // gracefully, return 0 to terminate the chunked body.
                          try {
                          size_t written = 0;

                          // Flush overflow
                          if (state->overflow.length() > 0) {
                              size_t toWrite = std::min(static_cast<size_t>(state->overflow.length()), maxLen);
                              memcpy(buffer, state->overflow.c_str(), toWrite);
                              written += toWrite;
                              if (toWrite < state->overflow.length()) {
                                  state->overflow = state->overflow.substring(toWrite);
                              } else {
                                  state->overflow = "";
                              }
                              if (written >= maxLen) return written;
                          }

                          // CSV header
                          if (!state->headerSent) {
                              String hdr = "datetime,temperature_f,pump_active,flow_rate,light_brightness,door_state,door_position,pump_trigger,door_trigger,light_trigger,event_type\n";
                              state->headerSent = true;
                              size_t available = maxLen - written;
                              size_t toWrite = std::min(static_cast<size_t>(hdr.length()), available);
                              memcpy(buffer + written, hdr.c_str(), toWrite);
                              written += toWrite;
                              if (toWrite < hdr.length()) {
                                  state->overflow = hdr.substring(toWrite);
                                  return written;
                              }
                          }

                          // Write rows
                          while (state->current < state->total && written < maxLen) {
                              size_t bufIdx = state->mgr->getOrderedIndex(state->current);
                              const DataPoint& dp = state->mgr->getDataPointAt(bufIdx);

                              char timeBuf[20]; // "YYYY-MM-DD HH:MM:SS"
                              time_t ts = dp.timestamp;
                              struct tm* tm_info = localtime(&ts);
                              if (tm_info && ts > 0) {
                                  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm_info);
                              } else {
                                  snprintf(timeBuf, sizeof(timeBuf), "%lu", (unsigned long)ts);
                              }
                              String row = String(timeBuf) + ",";
                              if (isnan(dp.temperature_f)) {
                                  row += ",";
                              } else {
                                  row += String(dp.temperature_f, 2) + ",";
                              }
                              row += dp.pump_active ? "true," : "false,";
                              row += String(dp.flow_rate, 3) + ",";
                              row += String(dp.light_brightness) + ",";
                              row += String(dp.door_state) + ",";
                              row += String(dp.door_position) + ",";
                              row += String(dp.pump_trigger) + ",";
                              row += String(dp.door_trigger) + ",";
                              row += String(dp.light_trigger) + ",";
                              row += String(dp.event_type) + "\n";

                              state->current++;

                              size_t available = maxLen - written;
                              size_t toWrite = std::min(static_cast<size_t>(row.length()), available);
                              memcpy(buffer + written, row.c_str(), toWrite);
                              written += toWrite;

                              if (toWrite < row.length()) {
                                  state->overflow = row.substring(toWrite);
                                  return written;
                              }
                          }

                          return written;
                          } catch (...) {
                              state->overflow = "";
                              // Force the loop to finish: jump current to total so
                              // the next invocation returns 0 and ends the stream.
                              state->current = state->total;
                              return 0;
                          }
                      });
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
            uri.equals("/update") || uri.startsWith("/update/") || uri.startsWith("/buzzer/") || uri.startsWith("/door/") ||
            uri.startsWith("/light/") || uri.startsWith("/weather/") || uri.equals("/sun/times") ||
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
void CoopControllerWebServer::setUpdateManager(UpdateManager* updateManager) {
    updateManager_ = updateManager;
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

void CoopControllerWebServer::setWeatherManager(WeatherManager* weatherManager) {
    weatherManager_ = weatherManager;

    if (!weatherManager_ || !hal) return;

    // Weather status endpoint - current conditions + short forecast for status page
    hal->webServerOn("/weather/status", HAL_WebRequestMethod::HTTP_GET,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!weatherManager_) {
                response->send(200, "application/json", R"({"enabled":false,"configured":false})");
                return;
            }
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            weatherManager_->toJson(obj);
            String output;
            serializeJson(doc, output);
            response->send(200, "application/json", output.c_str());
        });

    // LLM provider test-connection endpoint (issue #6). Accepts an optional
    // JSON body with unsaved provider config so the UI can test before saving.
    // CRASH FIX (v0.7.1): the TLS probe is deferred to the main loop — this
    // handler only captures overrides + flags the request, then returns 202
    // immediately. Running blocking TLS here panicked async_tcp (see issue #4
    // / v0.7.0 syslog). Pair with GET /weather/llm/test_result.
    hal->webServerOn("/weather/llm/test_connection", HAL_WebRequestMethod::HTTP_POST,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!weatherManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"WeatherManager not initialized"})");
                return;
            }
            // Defaults to the saved settings; JSON body overrides individual fields.
            String baseUrl = settingsManager.getLlmBaseUrl();
            String apiKey = settingsManager.getLlmApiKey();
            String model = settingsManager.getLlmModel();
            String type = settingsManager.getLlmProviderType();
            unsigned int timeout = settingsManager.getLlmTimeoutSeconds();

            String body = request->body();
            if (body.length() > 0) {
                JsonDocument bodyDoc;
                if (deserializeJson(bodyDoc, body) == DeserializationError::Ok) {
                    JsonObject cfg = bodyDoc.as<JsonObject>();
                    if (cfg["llm_base_url"].is<const char*>()) baseUrl = cfg["llm_base_url"].as<String>();
                    if (cfg["llm_api_key"].is<const char*>()) apiKey = cfg["llm_api_key"].as<String>();
                    if (cfg["llm_model"].is<const char*>()) model = cfg["llm_model"].as<String>();
                    if (cfg["llm_provider_type"].is<const char*>()) type = cfg["llm_provider_type"].as<String>();
                    if (cfg["llm_timeout_seconds"].is<int>()) timeout = cfg["llm_timeout_seconds"].as<unsigned int>();
                }
            }

            weatherManager_->requestLlmTest(baseUrl, apiKey, model, type, timeout);
            response->send(202, "application/json", R"({"status":"pending"})");
        });

    // LLM test result polling endpoint. No network I/O — reads the snapshot
    // produced by the loop task. Status: "pending" | "success" | "error" | "idle".
    hal->webServerOn("/weather/llm/test_result", HAL_WebRequestMethod::HTTP_GET,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!weatherManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"WeatherManager not initialized"})");
                return;
            }
            JsonDocument doc = weatherManager_->getLlmTestResultJson();
            String output;
            serializeJson(doc, output);
            response->send(200, "application/json", output.c_str());
        });

    // Weather (OpenWeatherMap) test endpoint (issue #6). Triggers one fetch
    // cycle with optional API-key override so the user can verify the key
    // before saving. CRASH FIX (v0.7.1): like the LLM endpoint above, the
    // fetch is deferred to the main loop. Pair with GET /weather/test_result.
    hal->webServerOn("/weather/test", HAL_WebRequestMethod::HTTP_POST,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!weatherManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"WeatherManager not initialized"})");
                return;
            }
            // Optional unsaved API key override (applied one-shot by the loop).
            String override;
            String body = request->body();
            if (body.length() > 0) {
                JsonDocument bodyDoc;
                if (deserializeJson(bodyDoc, body) == DeserializationError::Ok) {
                    JsonObject cfg = bodyDoc.as<JsonObject>();
                    if (cfg["weather_api_key"].is<const char*>()) {
                        String k = cfg["weather_api_key"].as<String>();
                        if (k.length() > 0) override = k;
                    }
                }
            }

            weatherManager_->requestWeatherTest(override);
            response->send(202, "application/json", R"({"status":"pending"})");
        });

    // Weather test result polling endpoint. No network I/O.
    hal->webServerOn("/weather/test_result", HAL_WebRequestMethod::HTTP_GET,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!weatherManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"WeatherManager not initialized"})");
                return;
            }
            JsonDocument doc = weatherManager_->getWeatherTestResultJson();
            String output;
            serializeJson(doc, output);
            response->send(200, "application/json", output.c_str());
        });

    logger.logInfo("Weather endpoint registered");
}

void CoopControllerWebServer::setNotificationManager(NotificationManager* notificationManager) {
    notificationManager_ = notificationManager;

    if (!notificationManager_ || !hal) return;

    // Test Telegram notification endpoint - accepts optional JSON body with unsaved config
    hal->webServerOn("/notifications/test/telegram", HAL_WebRequestMethod::HTTP_POST,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!notificationManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"NotificationManager not initialized"})");
                return;
            }
            // Try to parse JSON body with override config values
            NotificationResult result;
            String body = request->body();
            if (body.length() > 0) {
                JsonDocument bodyDoc;
                if (deserializeJson(bodyDoc, body) == DeserializationError::Ok) {
                    JsonObject config = bodyDoc.as<JsonObject>();
                    result = notificationManager_->sendTestWithConfig(NotificationChannel::TELEGRAM, config);
                } else {
                    result = notificationManager_->sendTest(NotificationChannel::TELEGRAM);
                }
            } else {
                result = notificationManager_->sendTest(NotificationChannel::TELEGRAM);
            }
            JsonDocument doc;
            doc["success"] = result.success;
            if (!result.success) doc["error"] = result.error_message;
            String output;
            serializeJson(doc, output);
            response->send(result.success ? 200 : 400, "application/json", output.c_str());
        });

    // Test Email notification endpoint - accepts optional JSON body with unsaved config
    hal->webServerOn("/notifications/test/email", HAL_WebRequestMethod::HTTP_POST,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!isAuthenticated(request)) {
                sendAuthRequired(response);
                return;
            }
            if (!notificationManager_) {
                response->send(500, "application/json", R"({"success":false,"error":"NotificationManager not initialized"})");
                return;
            }
            // Try to parse JSON body with override config values
            NotificationResult result;
            String body = request->body();
            if (body.length() > 0) {
                JsonDocument bodyDoc;
                if (deserializeJson(bodyDoc, body) == DeserializationError::Ok) {
                    JsonObject config = bodyDoc.as<JsonObject>();
                    result = notificationManager_->sendTestWithConfig(NotificationChannel::EMAIL, config);
                } else {
                    result = notificationManager_->sendTest(NotificationChannel::EMAIL);
                }
            } else {
                result = notificationManager_->sendTest(NotificationChannel::EMAIL);
            }
            JsonDocument doc;
            doc["success"] = result.success;
            if (!result.success) doc["error"] = result.error_message;
            String output;
            serializeJson(doc, output);
            response->send(result.success ? 200 : 400, "application/json", output.c_str());
        });

    // Notification status endpoint
    hal->webServerOn("/notifications/status", HAL_WebRequestMethod::HTTP_GET,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!notificationManager_) {
                response->send(500, "application/json", R"({"error":"NotificationManager not initialized"})");
                return;
            }
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            notificationManager_->toJson(obj);
            String output;
            serializeJson(doc, output);
            response->send(200, "application/json", output.c_str());
        });

    // MQTT status endpoint
    hal->webServerOn("/mqtt/status", HAL_WebRequestMethod::HTTP_GET,
        [this](IWebRequest *request, IWebResponse *response) {
            if (!mqttManager_) {
                response->send(200, "application/json", R"({"enabled":false,"connected":false})");
                return;
            }
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            mqttManager_->toJson(obj);
            String output;
            serializeJson(doc, output);
            response->send(200, "application/json", output.c_str());
        });

    logger.logInfo("Notification endpoints registered");
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
