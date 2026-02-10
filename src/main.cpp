/**
 * @file main.cpp
 * @brief Main application entry point for ESP32 Chicken Coop Controller
 *
 * This file contains the setup() and loop() functions for the chicken coop
 * controller system. All initialization and main loop logic is contained
 * within setup() to use a single FreeRTOS task.
 *
 * System Components:
 * - WiFi Controller: Manages network connectivity
 * - Sensor Manager: Handles temperature and water flow sensors
 * - Pump Controller: Controls water pump with temperature-based cycling
 * - Light Controller: PWM light control with smooth fading
 * - Door Controller: Automatic coop door based on sunrise/sunset
 * - Buzzer Controller: Audible alerts for system conditions
 * - Web Server: REST API and web UI
 * - Settings Manager: Persistent configuration storage
 * - Logger: System logging with multiple levels
 * - Sunrise/Sunset: Astronomical calculations for scheduling
 *
 * Main Loop Responsibilities:
 * - Feed task watchdog timer
 * - Update all controllers at appropriate intervals
 * - Monitor sensor readings and trigger alerts
 * - Log system status periodically
 * - Handle restart requests
 * - Monitor memory and trigger low-memory alerts
 *
 * Timing Intervals:
 * - Sensor update: Every 10 seconds (configurable)
 * - Pump update: Every 1 second
 * - Door update: Every 100ms
 * - Light update: Every 100ms
 * - Sunrise/Sunset: Every 1 minute (recalculates daily)
 * - Web server: Every loop iteration
 * - Status logging: Every 10 seconds
 * - Temperature logging: Every 1 minute
 *
 * Watchdog:
 * - Task watchdog timer initialized with configurable timeout
 * - Watchdog fed at start of each loop iteration
 * - Logs watchdog status every 1000 iterations (verbose mode)
 * - System reset on watchdog timeout (indicates hang)
 */

#include <Arduino.h>
#include "time.h"
#include <stdint.h>
#include <esp_task_wdt.h>
 
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
#include "HistoricalDataManager.h"


/**
 * @brief Main setup function - initializes all system components
 *
 * This function runs once on startup and contains the main application loop.
 * All initialization and main loop logic is contained here to use a single
 * FreeRTOS task on the ESP32.
 *
 * Initialization Sequence:
 * 1. Initialize HAL (hardware abstraction layer)
 * 2. Initialize Logger and load settings
 * 3. Log system startup and firmware version
 * 4. Check reset reason and log any watchdog resets
 * 5. Initialize all controllers
 * 6. Configure sunrise/sunset calculator
 * 7. Initialize task watchdog timer
 * 8. Start NTP time synchronization
 * 9. Start web server
 * 10. Enter main loop
 *
 * Main Loop:
 * - Feed watchdog timer
 * - Update WiFi controller (every iteration)
 * - Update sensors (every 10 seconds)
 * - Update pump controller (every 1 second)
 * - Update door controller (every 100ms)
 * - Update light controller (every 100ms)
 * - Update sunrise/sunset (every minute)
 * - Monitor for sensor errors and trigger alerts
 * - Monitor pump flow errors and trigger alerts
 * - Monitor memory usage and trigger alerts
 * - Log system status (every 10 seconds)
 * - Handle restart requests
 *
 * @note This function never returns - contains infinite while loop
 */
/**
 * @brief Check for factory reset request during bootup
 *
 * Checks if DOOR_MANUAL_SWITCH_B_PIN is held LOW for 20 seconds during bootup.
 * If held, triggers factory reset (clears all settings and WiFi credentials).
 * Uses WIFI_LED_B_PIN for visual feedback (rapid blink pattern).
 *
 * @param hal Hardware abstraction layer for pin access and LED control
 * @return true if factory reset was triggered, false otherwise
 */
bool checkFactoryResetRequest() {
    // Configure manual switch pin as input with pullup
    pinMode(DOOR_MANUAL_SWITCH_B_PIN, INPUT_PULLUP);

    // Check if button is pressed (active LOW)
    if (digitalRead(DOOR_MANUAL_SWITCH_B_PIN) == HIGH) {
        // Button not pressed, no factory reset
        return false;
    }

    // Button is pressed - configure LED for feedback
    pinMode(WIFI_LED_B_PIN, OUTPUT);

    Serial.println("Factory reset button detected - hold for 20 seconds to confirm");

    // Wait 20 seconds, checking button state and blinking LED
    const unsigned long factoryResetHoldTime = 20000; // 20 seconds
    const unsigned long blinkInterval = 100; // Fast blink (100ms)
    unsigned long startTime = millis();
    unsigned long lastBlinkTime = 0;
    bool ledState = false;

    while (millis() - startTime < factoryResetHoldTime) {
        // Check if button was released
        if (digitalRead(DOOR_MANUAL_SWITCH_B_PIN) == HIGH) {
            digitalWrite(WIFI_LED_B_PIN, LOW); // Turn off LED
            Serial.println("Factory reset cancelled - button released");
            return false;
        }

        // Blink LED rapidly to indicate factory reset in progress
        if (millis() - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = millis();
            ledState = !ledState;
            digitalWrite(WIFI_LED_B_PIN, ledState ? HIGH : LOW);
        }

        // Print countdown every second        
        if (static unsigned long lastCountdown = 0; millis() - lastCountdown >= 1000) {
            lastCountdown = millis();
            unsigned long remaining = (factoryResetHoldTime - (millis() - startTime)) / 1000;
            Serial.print("Factory reset in ");
            Serial.print(remaining);
            Serial.println(" seconds...");
        }

        delay(10); // Small delay to prevent tight loop
    }

    // Button held for full 20 seconds - trigger factory reset
    digitalWrite(WIFI_LED_B_PIN, HIGH); // Turn LED on solid
    Serial.println("===========================================");
    Serial.println("FACTORY RESET TRIGGERED");
    Serial.println("===========================================");

    return true;
}

void setup() // NOSONAR - complexity ok
{
    Serial.begin(SERIAL_BAUD);
    delay(100); // Allow serial to initialize

    HAL_ESP32 hal{}; // HAL implementation for ESP32

    // Check for factory reset request BEFORE initializing anything else
    if (checkFactoryResetRequest()) {
        // Initialize minimal components needed for factory reset
        hal.begin();
        settingsManager.begin(&hal);

        // Perform factory reset
        settingsManager.factoryReset();

        Serial.println("Factory reset complete - device will restart");
        Serial.println("===========================================");
        delay(2000); // Allow time for serial output

        // Restart device to apply factory reset
        hal.restart();
    }
    
    logger.begin(&hal); // Initialize logger singleton   
    hal.begin();

    // Coop Controller components
    CoopControllerWebServer webServer(&hal, 80);
    SensorManager sensorManager;
    PumpController pumpController;
    BuzzerController buzzerController;
    DoorController doorController;
    LightController lightController;
    SunriseSunsetCalculator sunriseSunset;
    WifiController wifiController;
    HistoricalDataManager historyManager;

    settingsManager.begin(&hal);
    settingsManager.load();

    // Set log level from settings
    logger.setLogLevel(logger.stringToLogLevel(settingsManager.getLogLevel()));

    // Initialize logging system
    logger.logInfo("ESP Coop Controller System starting up...");
    logger.logInfo(String("Firmware version: ") + firmwareVersion);
    logger.logInfo(String("Chip family: ") + chipFamily);
    
    // Check reset reason
    switch (int resetReason = hal.getResetReason(); resetReason) {
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
    wifiController.begin(&hal, &settingsManager, &buzzerController, hostName, apPasswd);
    doorController.begin(&buzzerController, &sunriseSunset);
    doorController.setLockoutEnabled(settingsManager.getDoorLockoutEnabled());
    doorController.setAutoCalcTimeoutEnabled(settingsManager.getDoorTimeoutAutoCalcEnabled());
    lightController.begin(&hal, &sunriseSunset);
    
    // Initialize sunrise/sunset calculator with location settings
    sunriseSunset.begin(&hal, settingsManager.getLatitude(),
                      settingsManager.getLongitude(),
                      settingsManager.getTimezoneOffsetHours());
    
    // Set water meter calibration from settings
    sensorManager.setPulsesPerGallon(settingsManager.getPulsesPerGallon());
    sensorManager.setFlowCalculationIntervalSeconds(settingsManager.getFlowCalculationIntervalSeconds());
    
    // Reconfigure syslog from runtime settings (overrides compile-time defaults if configured)
    if (settingsManager.getSyslogServer().length() > 0) {
        logger.reconfigureSyslog(settingsManager.getSyslogServer(),
                                settingsManager.getSyslogPort(),
                                hostName);
    }
    
    logger.logInfo("Coop controller components initialized");

    // Initialize Task Watchdog Timer
    int watchdogTimeout = settingsManager.getWatchdogTimeoutSeconds();
    
    if (esp_err_t wdtResult = esp_task_wdt_init(watchdogTimeout, true); wdtResult == ESP_OK) { // timeout in seconds, panic on timeout
        esp_task_wdt_add(nullptr); // Add current task (loop) to WDT watch
        logger.logInfo(String("Task Watchdog Timer initialized with ") + String(watchdogTimeout) + " second timeout");
    } else {
        logger.logError(String("Failed to initialize Task Watchdog Timer: ") + String(esp_err_to_name(wdtResult)));
    }

    // Only sync NTP if connected to WiFi (not in AP mode)
    if (wifiController.isConnected()) {
        logger.logInfo("NTP time synchronization started");
        configTime(0, 0, ntpServer);
    } else {
        logger.logWarning("WiFi not connected - NTP time sync deferred");
    }
    
    // Wait a moment for NTP to sync, then calculate sunrise/sunset
    delay(2000);
    sunriseSunset.forceUpdate();

    // Initialize historical data manager
    historyManager.begin(settingsManager.getHistoryEnabled(),
                         settingsManager.getHistoryBufferSize(),
                         settingsManager.getHistorySampleIntervalSeconds());
    logger.logInfo("Historical data manager initialized");

    webServer.begin(sensorManager,
                    pumpController,
                    buzzerController,
                    doorController,
                    lightController,
                    wifiController,
                    sunriseSunset,
                    historyManager);
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
            hal.restart();
        }

        // Update temperature sensors
        if (currentTime - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
        {
            lastSensorUpdate = currentTime;
            sensorManager.update();

            // Update historical data manager with current readings
            historyManager.update(
                sensorManager.getTemperature1F(),
                pumpController.isPumpOn(),
                sensorManager.getFlowRate1(),
                lightController.getCurrentBrightness(),
                doorController.getStateString(),
                pumpController.getLastTriggerSourceString(),
                doorController.getLastTriggerSourceString(),
                lightController.getLastTriggerSourceString()
            );

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
                if (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP) { // NOSONAR - nested ok
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
            uint32_t heapSize = hal.getHeapSize();
            uint32_t heapFree = hal.getFreeHeap();
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
                        ", Chip: " + hal.getChipModel() + 
                        ", CPU: " + ESP.getCpuFreqMHz() + " MHz");
        }
    }
}

void loop() {
    // Loop is handled inside setup() for this application
    // Empty - all logic is in setup() loop
}
