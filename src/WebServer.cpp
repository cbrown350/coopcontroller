#include "WebServer.h"

#include <AsyncJson.h>

#include "Logger.h"
#include "TempSensor.h"
#include "PumpController.h"

#define SPIFFS LittleFS

// External references to firmware version from main.cpp
extern const char *firmwareVersion;
extern const char *chipFamily;

// External references to coop controller components
extern TempSensor tempSensor;
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
        [this](AsyncWebServerRequest *request, JsonVariant &json)
        {
            JsonObject jsonObj = json.as<JsonObject>();
            
            // Only set WiFi settings if provided (i.e., when changing WiFi)
            if (jsonObj.containsKey("ssid")) {
                settingsManager.setSSID(jsonObj["ssid"].as<String>());
            }
            if (jsonObj.containsKey("passwd")) {
                settingsManager.setPassword(jsonObj["passwd"].as<String>());
            }
            if (jsonObj.containsKey("ap_mode")) {
                settingsManager.setAPMode(jsonObj["ap_mode"].as<bool>());
            }
            
            // Handle coop controller settings (these don't trigger WiFi changes)
            if (jsonObj.containsKey("temp_threshold_on_f") && jsonObj["temp_threshold_on_f"].is<float>()) {
                settingsManager.setTempThresholdOnF(jsonObj["temp_threshold_on_f"].as<float>());
            }
            if (jsonObj.containsKey("temp_threshold_off_f") && jsonObj["temp_threshold_off_f"].is<float>()) {
                settingsManager.setTempThresholdOffF(jsonObj["temp_threshold_off_f"].as<float>());
            }
            if (jsonObj.containsKey("water_flow_error_timeout_seconds") && jsonObj["water_flow_error_timeout_seconds"].is<int>()) {
                settingsManager.setWaterFlowErrorTimeoutSeconds(jsonObj["water_flow_error_timeout_seconds"].as<int>());
            }
            if (jsonObj.containsKey("pump_on_time_seconds") && jsonObj["pump_on_time_seconds"].is<int>()) {
                settingsManager.setPumpOnTimeSeconds(jsonObj["pump_on_time_seconds"].as<int>());
            }
            if (jsonObj.containsKey("pump_off_time_seconds") && jsonObj["pump_off_time_seconds"].is<int>()) {
                settingsManager.setPumpOffTimeSeconds(jsonObj["pump_off_time_seconds"].as<int>());
            }
            if (jsonObj.containsKey("pump_auto_mode") && jsonObj["pump_auto_mode"].is<bool>()) {
                settingsManager.setPumpAutoMode(jsonObj["pump_auto_mode"].as<bool>());
            }
            if (jsonObj.containsKey("light_auto_mode") && jsonObj["light_auto_mode"].is<bool>()) {
                settingsManager.setLightAutoMode(jsonObj["light_auto_mode"].as<bool>());
            }
            if (jsonObj.containsKey("light_on_hour") && jsonObj["light_on_hour"].is<int>()) {
                settingsManager.setLightOnHour(jsonObj["light_on_hour"].as<int>());
            }
            if (jsonObj.containsKey("light_off_hour") && jsonObj["light_off_hour"].is<int>()) {
                settingsManager.setLightOffHour(jsonObj["light_off_hour"].as<int>());
            }
            if (jsonObj.containsKey("debug_enabled") && jsonObj["debug_enabled"].is<bool>()) {
                settingsManager.setDebugEnabled(jsonObj["debug_enabled"].as<bool>());
            }
            
            // Note: 'enabled' is not sent from UI, so not handling it here to avoid defaults triggering changes
            
            settingsManager.save();
            jsonObj.clear();
            request->send(200, "text/plain", "ok");
        }));

    // Setup ElegantOTA
    ElegantOTA.begin(&server);

    // Sensor status endpoint
    server.on("/sensor_status", HTTP_GET,
              [this](AsyncWebServerRequest *request)
              {
                  JsonDocument jsonDoc;
                  
                  // Temperature sensor data
                  JsonObject sensor1 = jsonDoc["sensor1"].to<JsonObject>();
                  sensor1["type"] = tempSensor.getSensor1Type();
                  sensor1["connected"] = tempSensor.isSensor1Connected();
                  sensor1["temperature_f"] = tempSensor.getTemperature1F();
                  sensor1["flow_rate"] = tempSensor.getFlowRate1();
                  sensor1["pulse_count"] = tempSensor.getPulseCount1();
                  sensor1["status"] = tempSensor.getSensorStatusString(tempSensor.getSensor1Data());
                  
                  JsonObject sensor2 = jsonDoc["sensor2"].to<JsonObject>();
                  sensor2["type"] = tempSensor.getSensor2Type();
                  sensor2["connected"] = tempSensor.isSensor2Connected();
                  sensor2["temperature_f"] = tempSensor.getTemperature2F();
                  sensor2["flow_rate"] = tempSensor.getFlowRate2();
                  sensor2["pulse_count"] = tempSensor.getPulseCount2();
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
                  
                  // System status
                  JsonObject system = jsonDoc["system"].to<JsonObject>();
                  system["temp_threshold_on_f"] = settingsManager.getTempThresholdOnF();
                  system["temp_threshold_off_f"] = settingsManager.getTempThresholdOffF();
                  system["pump_on_time_seconds"] = settingsManager.getPumpOnTimeSeconds();
                  system["pump_off_time_seconds"] = settingsManager.getPumpOffTimeSeconds();
                  system["pump_auto_mode"] = settingsManager.getPumpAutoMode();
                  system["light_auto_mode"] = settingsManager.getLightAutoMode();
                  system["light_on_hour"] = settingsManager.getLightOnHour();
                  system["light_off_hour"] = settingsManager.getLightOffHour();
                  system["debug_enabled"] = settingsManager.getDebugEnabled();
                  system["water_flow_error_timeout_seconds"] = settingsManager.getWaterFlowErrorTimeoutSeconds();
                  
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

    // Serve static files from SPIFFS
    server.serveStatic("/assets/", SPIFFS, "/assets/");
    server.serveStatic("/", SPIFFS, "/");
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.htm", "text/html");
    });
}

void WebServer::loop()
{
    ElegantOTA.loop();
}