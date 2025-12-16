#include <Arduino.h>
#include <ESPmDNS.h>
#include "time.h"
#include "esp_task_wdt.h"
#include <stdint.h>

#include "config.h"
#include "HAL_ESP32.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "CoopControllerWebServer.h"
#include "BuzzerController.h"
#include "SensorManager.h"
#include "PumpController.h"
#include "LightController.h"
#include "DoorController.h"
#include "SunriseSunset.h"
#include "WifiController.h"


void setup() // NOSONAR - complexity ok
{
    Serial.begin(SERIAL_BAUD);

    HAL_ESP32 hal{}; // HAL implementation for ESP32
    
    logger.begin(&hal); // Initialize logger singleton   
    hal.begin();

    // Coop Controller components
    CoopControllerWebServer webServer(80);
    SensorManager sensorManager;
    PumpController pumpController;
    BuzzerController buzzerController;
    DoorController doorController;
    LightController lightController;
    SunriseSunsetCalculator sunriseSunset;
    WifiController wifiController;

    settingsManager.load();

    // Set log level from settings
    logger.setLogLevel(logger.stringToLogLevel(settingsManager.getLogLevel()));

    // Initialize logging system
    logger.logInfo("ESP Coop Controller System starting up...");
    logger.logInfo(String("Firmware version: ") + firmwareVersion);
    logger.logInfo(String("Chip family: ") + chipFamily);
    
    // Check reset reason    
    switch (esp_reset_reason_t resetReason = esp_reset_reason(); resetReason) {
        case ESP_RST_TASK_WDT:
            logger.logError("System was reset by Task Watchdog Timer!");
            break;
        case ESP_RST_INT_WDT:
            logger.logError("System was reset by Interrupt Watchdog Timer!");
            break;
        case ESP_RST_WDT:
            logger.logError("System was reset by other Watchdog!");
            break;
        case ESP_RST_POWERON:
            logger.logInfo("System powered on normally");
            break;
        case ESP_RST_SW:
            logger.logInfo("System reset by software");
            break;
        default:
            logger.logWarning(String("System reset reason: ") + String(resetReason));
            break;
    }

    // Initialize coop controller components
    sensorManager.begin(TEMP_METER_PIN, TEMP_METER_2_PIN);
    pumpController.begin(&sensorManager, &sensorManager, OUT_PUMP_PIN);
    buzzerController.begin(BUZZER_B_PIN);
    wifiController.begin(&settingsManager, &buzzerController, hostName, apPasswd);
    doorController.begin(&buzzerController, &sunriseSunset);
    lightController.begin(&sunriseSunset);
    
    // Initialize sunrise/sunset calculator with location settings
    sunriseSunset.begin(settingsManager.getLatitude(),
                      settingsManager.getLongitude(),
                      settingsManager.getTimezoneOffsetHours());
    
    // Set water meter calibration from settings
    sensorManager.setPulsesPerGallon(settingsManager.getPulsesPerGallon());
    
    logger.logInfo("Coop controller components initialized");

    // Initialize Task Watchdog Timer
    int watchdogTimeout = settingsManager.getWatchdogTimeoutSeconds();
    
    if (esp_err_t wdtResult = esp_task_wdt_init(watchdogTimeout, true); wdtResult == ESP_OK) {// timeout in seconds, panic on timeout
        esp_task_wdt_add(nullptr); // Add current task (loop) to WDT watch
        logger.logInfo(String("Task Watchdog Timer initialized with ") + String(watchdogTimeout) + " second timeout");
    } else {
        logger.logError(String("Failed to initialize Task Watchdog Timer: ") + String(esp_err_to_name(wdtResult)));
    }

    logger.logInfo("NTP time synchronization started");
    configTime(0, 0, ntpServer);
    
    // Wait a moment for NTP to sync, then calculate sunrise/sunset
    delay(2000);
    sunriseSunset.forceUpdate();
    
    webServer.begin(sensorManager,
                    pumpController,
                    buzzerController,
                    doorController,
                    lightController,
                    wifiController,
                    sunriseSunset);
    logger.logInfo("Web server started");

    logger.logInfo("System initialization complete");


    // Timing variables for coop controller
    unsigned long lastSensorUpdate = 0;
    unsigned long lastPumpUpdate = 0;
    unsigned long lastDoorUpdate = 0;
    unsigned long lastLightUpdate = 0;
    
    // main loop
    while(true) {
        
    #if CONFIG_FREERTOS_UNICORE
        yieldIfNecessary();
    #endif
        if (serialEventRun != nullptr) // NOSONAR - correct check for function
            serialEventRun();

        // Feed the watchdog timer at the start of each loop iteration
        esp_task_wdt_reset();    
        
        // Update WiFi controller every loop iteration (handles internal timing)
        wifiController.update();
        
        // Log watchdog status every 1000 loops for verbose logging
        static unsigned long loopCount = 0;
        loopCount++;
        if (loopCount % 1000 == 0) {
            logger.logVerbose(String("Watchdog fed at loop iteration ") + String(loopCount));
        }

        // Check if restart is requested
        unsigned long currentTime = millis();
        if (settingsManager.requestRestartAt > 0 && currentTime >= settingsManager.requestRestartAt)
        {
            logger.logWarning("Restarting device due to WiFi settings change...");
            delay(1000); // Allow time for log to be sent
            ESP.restart();
        }

        // Update temperature sensors
        if (currentTime - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
        {
            lastSensorUpdate = currentTime;
            sensorManager.update();
            
            // Log temperature readings periodically
            static unsigned long lastTempLog = 0;
            if (currentTime - lastTempLog >= 60000) // Log every minute
            {
                lastTempLog = currentTime;
                // Log temperature readings only if valid                
                if (float temp1 = sensorManager.getTemperature1F(); !isnan(temp1)) { // NOSONAR - complexity ok
                    logger.logDebug(String("Sensor 1: ") + String(temp1, 1) + "°F");
                }
                
                if (float temp2 = sensorManager.getTemperature2F(); !isnan(temp2)) { // NOSONAR - complexity ok
                    logger.logDebug(String("Sensor 2: ") + String(temp2, 1) + "°F");
                }
                
                // Check for sensor errors and trigger buzzer alerts
                static unsigned long lastSensorErrorAlert = 0;
                
                // Detailed logging for sensor status debugging
                bool hasWorkingTemperature = false;
                bool hasWorkingWaterMeter = false;
                bool sensor1Error = false;
                bool sensor2Error = false;
                
                // Check Sensor 1 status
                logger.logDebug(String("Sensor 1 status - Type: ") + 
                            (sensorManager.getSensor1Type() == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" : 
                            sensorManager.getSensor1Type() == SensorType::WATER_METER ? "WATER_METER" : "NONE") + // NOSONAR - complexity ok
                            ", Was detected: " + String(sensorManager.isSensor1Detected() ? "Yes" : "No") +
                            ", Connected: " + String(sensorManager.isSensor1Connected() ? "Yes" : "No"));
                
                if (sensorManager.getSensor1Type() == SensorType::DALLAS_TEMP) { // NOSONAR - complexity ok
                    float temp1 = sensorManager.getTemperature1F();
                    if (!isnan(temp1)) {
                        hasWorkingTemperature = true;
                        logger.logDebug(String("Sensor 1 - Temperature: ") + String(temp1, 1) + "°F (Working)");
                    } else {
                        sensor1Error = true;
                        logger.logDebug("Sensor 1 - Temperature: NaN (ERROR)");
                    }
                } else if (sensorManager.getSensor1Type() == SensorType::WATER_METER) {
                    bool activelyConnected = sensorManager.isActivelyConnected(sensorManager.getSensor1Data());
                    if (activelyConnected) {
                        hasWorkingWaterMeter = true;
                        logger.logDebug(String("Sensor 1 - Water meter active (") + 
                                    String(sensorManager.getFlowRate1(), 2) + " GPM) (Working)");
                    } else {
                        sensor1Error = true;
                        logger.logDebug("Sensor 1 - Water meter inactive (ERROR)");
                    }
                }
                
                // Check Sensor 2 status
                logger.logDebug(String("Sensor 2 status - Type: ") + 
                            (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" : 
                            sensorManager.getSensor2Type() == SensorType::WATER_METER ? "WATER_METER" : "NONE") + // NOSONAR - complexity ok
                            ", Was detected: " + String(sensorManager.isSensor2Detected() ? "Yes" : "No") +
                            ", Connected: " + String(sensorManager.isSensor2Connected() ? "Yes" : "No"));
                
                if (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP) { // NOSONAR - complexity ok
                    float temp2 = sensorManager.getTemperature2F();
                    if (!isnan(temp2)) {
                        hasWorkingTemperature = true;
                        logger.logDebug(String("Sensor 2 - Temperature: ") + String(temp2, 1) + "°F (Working)");
                    } else {
                        sensor2Error = true;
                        logger.logDebug("Sensor 2 - Temperature: NaN (ERROR)");
                    }
                } else if (sensorManager.getSensor2Type() == SensorType::WATER_METER) {
                    bool activelyConnected = sensorManager.isActivelyConnected(sensorManager.getSensor2Data());
                    if (activelyConnected) {
                        hasWorkingWaterMeter = true;
                        logger.logDebug(String("Sensor 2 - Water meter active (") + 
                                    String(sensorManager.getFlowRate2(), 2) + " GPM) (Working)");
                    } else {
                        sensor2Error = true;
                        logger.logDebug("Sensor 2 - Water meter inactive (ERROR)");
                    }
                }
                
                // Only trigger sensor error if we have no working temperature sensors AND no working water meters
                bool sensorError = (!hasWorkingTemperature && !hasWorkingWaterMeter);
                
                logger.logDebug(String("Sensor error analysis - Sensor 1 Error: ") + String(sensor1Error ? "Yes" : "No") +
                            ", Sensor 2 Error: " + String(sensor2Error ? "Yes" : "No") +
                            ", Has Working Temperature: " + String(hasWorkingTemperature ? "Yes" : "No") +
                            ", Has Working Water Meter: " + String(hasWorkingWaterMeter ? "Yes" : "No") +
                            ", Overall Sensor Error: " + String(sensorError ? "Yes" : "No"));
                
                if (sensorError && (currentTime - lastSensorErrorAlert > 60000)) { // NOSONAR - complexity ok
                    logger.logWarning("Triggering SENSOR_ERROR alert - No working sensors detected");
                    buzzerController.triggerAlert(AlertType::SENSOR_ERROR);
                    lastSensorErrorAlert = currentTime;
                } else if (!sensorError) {
                    // Clear sensor error alert when we have at least one working sensor
                    buzzerController.clearAlert(AlertType::SENSOR_ERROR);
                }
            }
        }

        // Update pump controller
        if (currentTime - lastPumpUpdate >= PUMP_UPDATE_INTERVAL)
        {
            lastPumpUpdate = currentTime;
            
            // Flow error detection is now handled inside PumpController
            
            // Update pump controller with current status
            pumpController.update();
            
            // Check for pump flow error and trigger buzzer alert
            static unsigned long lastPumpErrorAlert = 0;
            if (pumpController.hasFlowError()) {
                if (currentTime - lastPumpErrorAlert > 60000) { // NOSONAR - nested ok, Only alert once per minute
                    logger.logWarning("Pump flow error detected - triggering buzzer alert");
                    buzzerController.triggerAlert(AlertType::PUMP_ERROR);
                    lastPumpErrorAlert = currentTime;
                }
            } else {
                // Clear pump error alert when flow error is resolved
                buzzerController.clearAlert(AlertType::PUMP_ERROR);
            }
        }
        
        // Log sensor readings periodically
        static unsigned long lastSensorLog = 0;
        if (currentTime - lastSensorLog >= 30000) { // NOSONAR - declaration clearer if put above, Log every 30 seconds
            lastSensorLog = currentTime;

            if (sensorManager.isSensor1Connected()) {
                logger.logDebug(String("Sensor 1 (Pin ") + String(TEMP_METER_PIN) + String("): ") + String(sensorManager.getTemperature1F(), 1) + "°F " +
                (sensorManager.getSensor1Type() == SensorType::DALLAS_TEMP ? "(Temperature)" : "(Water Meter)"));
            }
            if (sensorManager.isSensor2Connected()) {
                if (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP) { // NOSONAR - nested  ok
                    float temp2 = sensorManager.getTemperature2F();
                    if (!isnan(temp2)) {
                        logger.logDebug(String("Sensor 2 (Pin ") + String(TEMP_METER_2_PIN) + String("): ") + String(temp2, 1) + String("°F (Temperature)"));
                    }
                } else {
                    logger.logDebug(String("Sensor 2 (Pin ") + String(TEMP_METER_2_PIN) + String("): ") + String(sensorManager.getFlowRate2(), 2) + String(" GPM, ") + String(sensorManager.getPulseCount2()) + String(" pulses (Water Meter)"));
                }
            }
            
            float currentTemp = sensorManager.getTemperature1F();
            // Try to get temperature from sensor 1 first, then sensor 2
            if (isnan(currentTemp)) {
                currentTemp = sensorManager.getTemperature2F();
            }

            float threshold = settingsManager.getTempThresholdOnF();
            bool tempBelowThreshold = sensorManager.isTemperatureBelowThreshold();
            if (!isnan(currentTemp) && (!tempBelowThreshold || tempBelowThreshold)) {
                if (tempBelowThreshold) { // NOSONAR - nesting ok
                    logger.logInfo(String("Temperature below threshold (") + String(currentTemp, 1) + String("°F < ") + String(threshold) + String("°F)"));
                } else {
                    logger.logInfo(String("Temperature above threshold (") + String(currentTemp, 1) + String("°F >= ") + String(settingsManager.getTempThresholdOffF()) + String("°F)"));
                }
            } else {
                logger.logWarning("No temperature sensor available for threshold comparison");
            }
        }

        // Update buzzer controller
        buzzerController.update();
        
        // Update door controller
        if (currentTime - lastDoorUpdate >= DOOR_UPDATE_INTERVAL)
        {
            lastDoorUpdate = currentTime;
            doorController.update();
        }
        
        // Update light controller
        if (currentTime - lastLightUpdate >= LIGHT_UPDATE_INTERVAL)
        {
            lastLightUpdate = currentTime;
            lightController.update();
        }
        
        // Update sunrise/sunset calculations (check every minute, but only recalculates every 24 hours)
        static unsigned long lastSunUpdate = 0;
        if (currentTime - lastSunUpdate >= 60000) { // NOSONAR - declaration clearer above, Check every minute
            lastSunUpdate = currentTime;
            sunriseSunset.update();
        }
        
        webServer.loop();

        delay(10);

        // log uptime and heap size every 10 seconds
        static unsigned long lastCPUStatusLog = 0;
        if (currentTime - lastCPUStatusLog >= 10000) {
            lastCPUStatusLog = currentTime;
            // Calculate memory usage percentage
            uint32_t heapSize = ESP.getHeapSize();
            uint32_t heapFree = ESP.getFreeHeap();
            double heapUsedPercent = 100.0 - (100.0 * static_cast<double>(heapFree) / static_cast<double>(heapSize));
            
            // Trigger low memory alert if usage is high
            if (heapUsedPercent > 80.0) {
                static unsigned long lastLowMemoryAlert = 0;
                if (currentTime - lastLowMemoryAlert > 60000) { // NOSONAR - declaration clearer above, Only alert once per minute
                    buzzerController.triggerAlert(AlertType::LOW_MEMORY);
                    lastLowMemoryAlert = currentTime;
                }
            }
            
            // Format uptime
            unsigned long uptimeSeconds = millis() / 1000;
            unsigned long days = uptimeSeconds / 86400;
            uptimeSeconds %= 86400;
            unsigned long hours = uptimeSeconds / 3600;
            uptimeSeconds %= 3600;
            unsigned long minutes = uptimeSeconds / 60;
            uptimeSeconds %= 60;
            
            String uptimeFormatted = "";
            if (days > 0) uptimeFormatted += String(days) + "d ";
            if (hours > 0 || days > 0) uptimeFormatted += String(hours) + "h ";
            if (minutes > 0 || hours > 0 || days > 0) uptimeFormatted += String(minutes) + "m ";
            uptimeFormatted += String(uptimeSeconds) + "s";
            
            logger.logVerbose(String("System Status - Uptime: ") + uptimeFormatted + 
                        ", Free heap: " + String(heapFree) + " bytes (" + 
                        String(heapUsedPercent, 1) + "% used), " +
                        "Chip: " + ESP.getChipModel() + 
                        ", CPU: " + String(ESP.getCpuFreqMHz()) + " MHz");
        }
    }
}

void loop() { 
    // Loop is handled inside setup() for this application
    // Empty - all logic is in setup() loop
}