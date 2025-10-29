#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "LittleFS.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "WebServer.h"
#include "time.h"

#include <esp_task_wdt.h>  // Add this for WDT functions

#include "Buzzer.h"
#include "TempSensor.h"
#include "PumpController.h"



#define SPIFFS LittleFS

// Fix for CHIP_FAMILY_RAW that may contain "ESP32"
#ifdef ESP32
#undef ESP32
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

const char* firmwareVersion = (strcmp(TOSTRING(FIRMWARE_VERSION_RAW), "") == 0) ? "dev" : TOSTRING(FIRMWARE_VERSION_RAW);
const char* chipFamily      = (strcmp(TOSTRING(CHIP_FAMILY_RAW), "") == 0) ? "unknown" : TOSTRING(CHIP_FAMILY_RAW);

#define WIFI_CHECK_INTERVAL 30000     // Check WiFi every 30 seconds
#define WIFI_RECONNECT_TIMEOUT 10000  // Wait 10 seconds for reconnection
#define SENSOR_UPDATE_INTERVAL 5000    // Update sensors every 5 seconds
#define PUMP_UPDATE_INTERVAL 1000     // Update pump controller every 1 second


// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

WebServer webServer(80);

// Coop Controller components
TempSensor tempSensor;
PumpController pumpController;

// Variables to track WiFi connection monitoring
unsigned long lastWifiCheck      = 0;
unsigned long wifiReconnectStart = 0;
unsigned long wifiAPModeStart     = 0;
bool          isReconnecting     = false;
bool          isInAPMode         = false;
int           wifiRetryCount       = 0;

// Timing variables for coop controller
unsigned long lastSensorUpdate = 0;
unsigned long lastPumpUpdate = 0;

// If wifi fails, revert to AP mode and restart;
void failWifi()
{
    // Only revert to AP mode if WiFi has never successfully connected
    if (!settingsManager.getHasConnected())
    {
    settingsManager.setAPMode(true);
    if (settingsManager.save())
    {
        logger.log("Failed to connect to wifi, reverted to AP mode");
    }
    else
    {
        logger.log("Failed to update settings");
    }

    delay(1000);  // Give time for serial output
    ESP.restart();
}
    else
    {
        logger.log("WiFi connection failed, retrying in 30 seconds");
        // Don't restart, just continue trying to reconnect in checkWifiConnection()
    }
}

void wifiSetup()
{
    if (settingsManager.isAPMode())
    {
        logger.log("Starting AP mode");
        WiFi.softAP("CoopController", "coopycontroller");
        logger.log("AP mode started, IP address: " + WiFi.softAPIP().toString());
        isInAPMode = true;
        wifiAPModeStart = millis();
        
        // Add logging for WiFi task status
        logger.logf("WiFi AP mode started on core %d", xPortGetCoreID());
        logger.logf("Current WiFi task handle: %p", xTaskGetCurrentTaskHandle());
    }
    else
    {
        String ssid = settingsManager.getSSID();
        String password = settingsManager.getPassword();
        
        if (ssid.length() == 0) {
            logger.log("No SSID configured, falling back to AP mode");
            settingsManager.setAPMode(true);
            settingsManager.save();
            delay(1000);
            ESP.restart();
            return;
        }
        
        logger.logf("Connecting to WiFi: %s", ssid.c_str());
        WiFi.setHostname("coopcontroller");
        WiFi.persistent(false);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int maxRetries = settingsManager.getWifiMaxRetries();
        int retryDelay = settingsManager.getWifiRetryDelaySeconds();
        
        wifiRetryCount = 0;
        while (WiFi.status() != WL_CONNECTED && wifiRetryCount < maxRetries)
        {
            Serial.print('.');
            delay(retryDelay * 1000);
            wifiRetryCount++;
            
            // Add some debugging
            if (settingsManager.getDebugEnabled()) {
                Serial.printf("DEBUG: WiFi status: %d, attempt %d/%d\n", WiFi.status(), wifiRetryCount, maxRetries);
            }
        }

        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            logger.log("WiFi Connected, IP address: " + WiFi.localIP().toString());
            isInAPMode = false;

            // Mark that WiFi has successfully connected at least once
            if (!settingsManager.getHasConnected()) {
                settingsManager.setHasConnected(true);
                settingsManager.save();
                logger.log("First successful WiFi connection recorded");
            }
            if (!MDNS.begin("coopcontroller"))
            {
                logger.log("Error setting up MDNS responder!");
            }
            
            // Add logging for WiFi task status
            logger.logf("WiFi connected on core %d", xPortGetCoreID());
            logger.logf("Current WiFi task handle: %p", xTaskGetCurrentTaskHandle());
        } else {
            logger.logf("Failed to connect to WiFi after %d attempts", wifiRetryCount);
            failWifi();
        }
    }
}

void checkWifiConnection()
{
    // Check if we're in AP mode and need to retry WiFi connection
    if (isInAPMode) {
        unsigned long apDuration = settingsManager.getWifiAPDurationMinutes() * 60000; // Convert to milliseconds
        if (millis() - wifiAPModeStart >= apDuration && !settingsManager.getSSID().isEmpty()){
            logger.log("AP mode duration expired, attempting WiFi connection");
            settingsManager.setAPMode(false);
            settingsManager.save();
            delay(1000);
            ESP.restart();
        }
        return;
    }

    // Skip check if already in AP mode
    if (settingsManager.isAPMode())
    {
        return;
    }

    // Check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED)
    {
        if (!isReconnecting)
        {
            logger.log("WiFi disconnected, attempting to reconnect...");
            String ssid = settingsManager.getSSID();
            String password = settingsManager.getPassword();
            
            if (ssid.length() > 0) {
                WiFi.begin(ssid.c_str(), password.c_str());
                wifiReconnectStart = millis();
                isReconnecting = true;
                wifiRetryCount = 0;
                
                if (settingsManager.getDebugEnabled()) {
                    Serial.printf("DEBUG: Starting WiFi reconnection to %s\n", ssid.c_str());
                }
            } else {
                logger.log("No SSID configured for reconnection");
                failWifi();
            }
        }
        else
        {
            // Check if reconnection timeout has elapsed
            int maxRetries = settingsManager.getWifiMaxRetries();
            int retryDelay = settingsManager.getWifiRetryDelaySeconds();
            
            if (millis() - wifiReconnectStart >= (retryDelay * 1000 * maxRetries))
            {
                logger.log("WiFi reconnection timeout, switching to AP mode");
                settingsManager.setAPMode(true);
                settingsManager.save();
                delay(1000);
                ESP.restart();
            }
        }
    }
    else
    {
        // WiFi is connected, reset reconnection state
        if (isReconnecting)
        {
            logger.log("WiFi reconnected successfully");
            isReconnecting = false;

            // Mark that WiFi has successfully connected at least once
            if (!settingsManager.getHasConnected())
            {
                settingsManager.setHasConnected(true);
                settingsManager.save();
            }
        }
    }
}


void setup()
{
    // put your setup code here, to run once:
    pinMode(TEMP_METER_PIN, INPUT_PULLUP);
    pinMode(TEMP_METER_2_PIN, INPUT_PULLUP);
    Serial.begin(SERIAL_BAUD);

    // Initialize logging system
    logger.log("ESP Coop Controller System starting up...");
    logger.logf("Firmware version: %s", firmwareVersion);
    logger.logf("Chip family: %s", chipFamily);

    // Initialize filesystem
    if (!LittleFS.begin(true)) {  // The 'true' parameter formats the filesystem if it fails to mount
        logger.log("Failed to mount LittleFS filesystem, formatting...");
        if (!LittleFS.begin(true)) {
            logger.log("Failed to initialize LittleFS even after formatting");
            // Continue without filesystem - settings will use defaults
        } else {
            logger.log("LittleFS filesystem initialized after formatting");
        }
    } else {
        logger.log("LittleFS filesystem initialized");
    }

    // Load settings early
    settingsManager.load();

    // Initialize coop controller components
    tempSensor.begin();
    pumpController.begin();
    logger.log("Coop controller components initialized");

    wifiSetup();
    logger.log("NTP time synchronization started");
    configTime(0, 0, ntpServer);
    webServer.begin();
    logger.log("Web server started");

    logger.log("System initialization complete");
}

unsigned long getTime()
{
    time_t    now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 200)) { // Add timeout to prevent hanging
        // Serial.println("Failed to obtain time");
        return (0);
    }
    time(&now);
    return now;
}

void loop()
{
    // Check WiFi connection periodically
    unsigned long currentTime = millis();

    // Check if restart is requested
    if (settingsManager.requestRestartAt > 0 && currentTime >= settingsManager.requestRestartAt)
    {
        logger.log("Restarting device due to WiFi settings change...");
        ESP.restart();
    }
    if (currentTime - lastWifiCheck >= WIFI_CHECK_INTERVAL)
    {
        lastWifiCheck = currentTime;
        checkWifiConnection();
    }

    // Update temperature sensors
    if (currentTime - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
    {
        lastSensorUpdate = currentTime;
        tempSensor.update();
        
        // Log temperature readings periodically
        static unsigned long lastTempLog = 0;
        if (currentTime - lastTempLog >= 60000) // Log every minute
        {
            lastTempLog = currentTime;
            if (tempSensor.isSensor1Connected()) {
                logger.logf("Sensor 1: %.1f°F", tempSensor.getTemperature1F());
            }
            if (tempSensor.isSensor2Connected()) {
                logger.logf("Sensor 2: %.1f°F", tempSensor.getTemperature2F());
            }
        }
    }

    // Update pump controller
    if (currentTime - lastPumpUpdate >= PUMP_UPDATE_INTERVAL)
    {
        lastPumpUpdate = currentTime;
        
        // Get temperature status
        float currentTemp = tempSensor.getTemperature1F();
        bool tempBelowThreshold = tempSensor.isTemperatureBelowThreshold();
        
        // Check for water flow errors
        bool flowError = tempSensor.hasWaterFlowError(1) || tempSensor.hasWaterFlowError(2);
        
        // Update pump controller with current status
        pumpController.update(
            currentTemp, // Use actual temperature reading
            tempBelowThreshold,
            flowError
        );
    }

    // Set pump auto mode based on settings (only when it changes)
    static bool lastPumpAutoMode = false;
    bool currentPumpAutoMode = settingsManager.getPumpAutoMode();
    if (currentPumpAutoMode != lastPumpAutoMode) {
        if (currentPumpAutoMode) {
            pumpController.setAutoMode(true);
        } else {
            pumpController.turnOff();
        }
        lastPumpAutoMode = currentPumpAutoMode;
    }
    
    // Log sensor readings periodically
    static unsigned long lastSensorLog = 0;
    if (currentTime - lastSensorLog >= 30000) { // Log every 30 seconds
        lastSensorLog = currentTime;
        float threshold = settingsManager.getTempThresholdOnF();
        if (tempSensor.isSensor1Connected()) {
            logger.logf("Sensor 1 (Pin %d): %.1f°F %s", TEMP_METER_PIN, tempSensor.getTemperature1F(),
                       tempSensor.getSensor1Type() == SENSOR_TYPE_DALLAS_TEMP ? "(Temperature)" : "(Water Meter)");
        }
        if (tempSensor.isSensor2Connected()) {
            if (tempSensor.getSensor2Type() == SENSOR_TYPE_DALLAS_TEMP) {
                logger.logf("Sensor 2 (Pin %d): %.1f°F (Temperature)", TEMP_METER_2_PIN, tempSensor.getTemperature2F());
            } else {
                logger.logf("Sensor 2 (Pin %d): %.2f GPM, %lu pulses (Water Meter)", 
                           TEMP_METER_2_PIN, tempSensor.getFlowRate2(), tempSensor.getPulseCount2());
            }
        }
        
        float currentTemp = tempSensor.getTemperature1F();
        bool tempBelowThreshold = tempSensor.isTemperatureBelowThreshold();
        if (tempBelowThreshold) {
            logger.logf("Temperature below threshold (%.1f°F < %.1f°F)", currentTemp, threshold);
        } else {
            logger.logf("Temperature above threshold (%.1f°F >= %.1f°F)", currentTemp, settingsManager.getTempThresholdOffF());
        }
    }

    webServer.loop();

    delay(10);
}
