#include "WebServer.h"

#include <AsyncJson.h>

#include "Logger.h"

#define SPIFFS LittleFS

// External reference to firmware version from main.cpp
extern const char *firmwareVersion;
extern const char *chipFamily;

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
            settingsManager.setSSID(jsonObj["ssid"].as<String>());
            settingsManager.setSSID(jsonObj["ssid"].as<String>());
            if (jsonObj["passwd"].is<String>() && jsonObj["passwd"].as<String>().length() > 0)
            {
                settingsManager.setPassword(jsonObj["passwd"].as<String>());
            }
            settingsManager.setAPMode(jsonObj["ap_mode"].as<bool>());
            settingsManager.setEnabled(jsonObj["enabled"].as<bool>());
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
                  // Add status information using singleton

                  JsonDocument jsonDoc;
                  
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
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