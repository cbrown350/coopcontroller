#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "LittleFS.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "WebServer.h"
#include "time.h"

#include <esp_task_wdt.h>

#include "Buzzer.h"
#include "SensorManager.h"
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

const char* syslogServer = (strcmp(TOSTRING(SYSLOG_SERVER), "") == 0 || strcmp(TOSTRING(SYSLOG_SERVER), "1") == 0) ? "" : TOSTRING(SYSLOG_SERVER);
const char* syslogPort = (strcmp(TOSTRING(SYSLOG_PORT), "") == 0 || strcmp(TOSTRING(SYSLOG_PORT), "1") == 0) ? "" : TOSTRING(SYSLOG_PORT);

const char* hostName      = (strcmp(TOSTRING(HOST_NAME), "") == 0) ? "coopcontroller" : TOSTRING(HOST_NAME);
const char* otaPasswd      = (strcmp(TOSTRING(OTA_PASSWD), "") == 0 || strcmp(TOSTRING(OTA_PASSWD), "1") == 0) ? "" : TOSTRING(OTA_PASSWD);
const char* apPasswd      = (strcmp(TOSTRING(AP_PASSWD), "") == 0 || strcmp(TOSTRING(AP_PASSWD), "1") == 0) ? "" : TOSTRING(AP_PASSWD);

#define WIFI_CHECK_INTERVAL 30000     // Check WiFi every 30 seconds
#define WIFI_RECONNECT_TIMEOUT 10000  // Wait 10 seconds for reconnection
#define SENSOR_UPDATE_INTERVAL 5000    // Update sensors every 5 seconds
#define PUMP_UPDATE_INTERVAL 1000     // Update pump controller every 1 second


// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

WebServer webServer(80);

// Coop Controller components
SensorManager tempSensor;
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

// Watchdog reset handler - called when watchdog triggers
void IRAM_ATTR watchdogResetHandler() {
    // Log the watchdog reset event before system resets
    // Note: This runs in interrupt context, so keep it minimal
    // The actual logging will be done by the watchdog reset itself
    // This is just for any last-minute cleanup if needed
}

void wifiSetup()
{
    if (settingsManager.isAPMode())
    {
        logger.log("Starting AP mode");
        if (apPasswd && strlen(apPasswd) >= 0) {
            WiFi.softAP("CoopController", apPasswd);
            Serial.println("AP password set: " + String(apPasswd));
        } else {
            WiFi.softAP("CoopController", NULL);
        }
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
        
        if (hostName && strlen(hostName) > 0) {
            WiFi.setHostname(hostName); // Need to set hostname in all places for mDNS to work
            logger.logf("Hostname set to: %s", hostName);
        } 
        WiFi.persistent(false); // Fix for issues with reconnection, credentials are stored in settingsManager
        logger.logf("Connecting to WiFi: %s", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int maxRetries = settingsManager.getWifiMaxRetries();
        int retryDelay = settingsManager.getWifiRetryDelaySeconds();
        
        wifiRetryCount = 0;
        while (!WiFi.isConnected() && wifiRetryCount < maxRetries)
        {
            Serial.print('.');
            delay(retryDelay * 1000);
            wifiRetryCount++;
            
            // Add some debugging
            logger.logDebug(String("WiFi status: ") + String(WiFi.status()) + ", attempt " + String(wifiRetryCount) + "/" + String(maxRetries));
        }

        Serial.println();
        
        if (WiFi.isConnected()) {
            logger.log("WiFi Connected, IP address: " + WiFi.localIP().toString());
            isInAPMode = false;
            
            if (hostName && strlen(hostName) > 0) {
                int mDNSRetries = 5;
                while(mDNSRetries > 0 && !MDNS.begin(hostName)) {
                    Serial.println("Starting mDNS...");
                    delay(1000);
                    mDNSRetries--;
                }
                
                Serial.println("MDNS started"); 
            }

            // Mark that WiFi has successfully connected at least once
            if (!settingsManager.getHasConnected()) {
                settingsManager.setHasConnected(true);
                settingsManager.save();
                logger.log("First successful WiFi connection recorded");
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
    if (!WiFi.isConnected())
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
                
                logger.logDebug(String("Starting WiFi reconnection to ") + String(ssid.c_str()));
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
            
            
            if (hostName && strlen(hostName) > 0) {
                int mDNSRetries = 5;
                while(mDNSRetries > 0 && !MDNS.begin(hostName)) {
                    Serial.println("Starting mDNS...");
                    delay(1000);
                    mDNSRetries--;
                }
                
                Serial.println("MDNS started"); 
            }

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
    
    // Initialize logger level from settings
    String logLevelStr = settingsManager.getLogLevel();
    LogLevel level = LogLevel::INFO; // default
    if (logLevelStr == "VERBOSE") level = LogLevel::VERBOSE;
    else if (logLevelStr == "DEBUG") level = LogLevel::DEBUG;
    else if (logLevelStr == "INFO") level = LogLevel::INFO;
    else if (logLevelStr == "WARNING") level = LogLevel::WARNING;
    else if (logLevelStr == "ERROR") level = LogLevel::ERROR;
    logger.setLogLevel(level);
    
    logger.log("Coop controller components initialized");

    // Initialize Task Watchdog Timer
    int watchdogTimeout = settingsManager.getWatchdogTimeoutSeconds();
    esp_err_t wdtResult = esp_task_wdt_init(watchdogTimeout, true); // timeout in seconds, panic on timeout
    if (wdtResult == ESP_OK) {
        esp_task_wdt_add(NULL); // Add current task (loop) to WDT watch
        logger.logInfo(String("Task Watchdog Timer initialized with ") + String(watchdogTimeout) + " second timeout");
    } else {
        logger.logError(String("Failed to initialize Task Watchdog Timer: ") + String(esp_err_to_name(wdtResult)));
    }

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
        Serial.println("Failed to obtain time"); // Can't call logger since it may call getTime()
        return (0);
    }
    time(&now);
    return now;
}

void loop()
{
    // put your main code here, to run repeatedly:
    // Feed the watchdog timer at the start of each loop iteration
    esp_task_wdt_reset();    

    // Log watchdog status every 1000 loops for verbose logging
    static unsigned long loopCount = 0;
    loopCount++;
    if (loopCount % 1000 == 0) {
        logger.logVerbose(String("Watchdog fed at loop iteration ") + String(loopCount));
    }

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
        
        // Check for water flow errors - only when pump is on and running long enough without flow
        bool hasWaterMeter = tempSensor.hasActiveWaterMeter();
        unsigned long lastPulse = tempSensor.getMostRecentPulseTime();
        unsigned long pumpRunStart = pumpController.getCurrentRunStartTime();
        unsigned long currentTimeMs = millis();
        unsigned long pumpRunTime = (pumpRunStart > 0) ? (currentTimeMs - pumpRunStart) : 0;
        int timeoutSeconds = settingsManager.getWaterFlowErrorTimeoutSeconds();
        unsigned long timeoutMs = (unsigned long)timeoutSeconds * 1000UL;
        bool flowError = false;
        if (hasWaterMeter && pumpController.isPumpOn() && pumpRunTime >= timeoutMs && (currentTimeMs - lastPulse) >= timeoutMs) {
            flowError = true;
            logger.log("Flow error detected: pump running without flow for timeout period");
            logger.logDebug(String("Pump run time: ") + String(pumpRunTime) + " ms, Current time: " + String(currentTime) + " ms, Last pulse time: " + String(lastPulse) + " ms, Timeout: " + String(timeoutMs) + " ms");
        }
        
        // Update pump controller with current status
        pumpController.update(
            currentTemp, // Use actual temperature reading
            flowError
        );
    }
    
    // Log sensor readings periodically
    static unsigned long lastSensorLog = 0;
    if (currentTime - lastSensorLog >= 30000) { // Log every 30 seconds
        lastSensorLog = currentTime;
        float threshold = settingsManager.getTempThresholdOnF();
        if (tempSensor.isSensor1Connected()) {
            logger.logf("Sensor 1 (Pin %d): %.1f°F %s", TEMP_METER_PIN, tempSensor.getTemperature1F(),
                       tempSensor.getSensor1Type() == SensorType::DALLAS_TEMP ? "(Temperature)" : "(Water Meter)");
        }
        if (tempSensor.isSensor2Connected()) {
            if (tempSensor.getSensor2Type() == SensorType::DALLAS_TEMP) {
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

    // log the uptime and heap size every 10 seconds
    static unsigned long lastCPUStatusLog = 0;
    if (currentTime - lastCPUStatusLog >= 10000) {
        lastCPUStatusLog = currentTime;
        logger.logVerbose(String("Uptime: ") + String(millis() / 1000) + " seconds, Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    }
}
