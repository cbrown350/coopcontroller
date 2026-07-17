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
#include "UpdateManager.h"
#include "NotificationManager.h"
#include "TelegramBot.h"
#include "MQTTManager.h"
#include "CrashDiagnostics.h"


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
    UpdateManager updateManager;
    NotificationManager notificationManager;
    TelegramBot telegramBot;
    MQTTManager mqttManager;

    settingsManager.begin(&hal);
    settingsManager.load();
    
    // Reconfigure syslog from runtime settings (overrides compile-time defaults if configured)
    if (settingsManager.getSyslogServer().length() > 0) {
        logger.reconfigureSyslog(settingsManager.getSyslogServer(),
                                settingsManager.getSyslogPort(),
                                settingsManager.getHostname().c_str());
    }

    // Set log level from settings
    logger.setLogLevel(logger.stringToLogLevel(settingsManager.getLogLevel()));

    // Initialize logging system
    logger.logInfo("ESP Coop Controller System starting up...");
    logger.logfInfo("Firmware version: %s", firmwareVersion);
    logger.logfInfo("Chip family: %s", chipFamily);

    // Initialize coop controller components
    sensorManager.begin(TEMP_METER_PIN, TEMP_METER_2_PIN);
    pumpController.begin(&sensorManager, &sensorManager, OUT_PUMP_PIN);
    buzzerController.begin(BUZZER_B_PIN);
    wifiController.begin(&hal, &settingsManager, &buzzerController, apPasswd);
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
    
    logger.logInfo("Coop controller components initialized");    
    
    // Check reset reason - deferred until after logger/Wifi is initialized to capture any watchdog resets during bootup
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
        case ESP_RST_PANIC:
            logger.logError("System was reset by software PANIC/exception!");
            break;
        default:
            logger.logfWarning("System reset reason: %d", resetReason);
            break;
    }

    // Check for coredump from previous crash and log details
    crashDiagnosticsCheck([](const char* msg) {
        logger.logError(msg);
    });

    // Initialize Task Watchdog Timer
    int watchdogTimeout = settingsManager.getWatchdogTimeoutSeconds();
    
    if (esp_err_t wdtResult = esp_task_wdt_init(watchdogTimeout, true); wdtResult == ESP_OK) { // timeout in seconds, panic on timeout
        esp_task_wdt_add(nullptr); // Add current task (loop) to WDT watch
        logger.logfInfo("Task Watchdog Timer initialized with %d second timeout", watchdogTimeout);
    } else {
        logger.logfError("Failed to initialize Task Watchdog Timer: %s", esp_err_to_name(wdtResult));
    }

    // Only sync NTP if connected to WiFi (not in AP mode)
    if (wifiController.isConnected()) {
        logger.logInfo("NTP time synchronization started");
        String tzPosix = settingsManager.getTimezonePosix();
        if (tzPosix.length() == 0) {
            // Auto-detect timezone from coordinates (US-focused, covers CONUS + AK + HI)
            float lat = settingsManager.getLatitude();
            float lon = settingsManager.getLongitude();
            if (lat >= 24.0f && lat <= 50.0f && lon >= -125.0f && lon <= -66.0f) {
                // Continental US — pick timezone by longitude band
                if (lon >= -87.5f)       tzPosix = "EST5EDT,M3.2.0,M11.1.0";  // Eastern (NYC -74, Detroit -83, Atlanta -84)
                else if (lon >= -104.0f) tzPosix = "CST6CDT,M3.2.0,M11.1.0";  // Central (Chicago -87.6, Dallas -96.8)
                else if (lon >= -115.0f) tzPosix = "MST7MDT,M3.2.0,M11.1.0";  // Mountain (Denver -104.9, SLC -111.9, Boise -116.2 is close)
                else                     tzPosix = "PST8PDT,M3.2.0,M11.1.0";  // Pacific (LA -118.2, Seattle -122.3)
            } else if (lat >= 51.0f && lon <= -130.0f) {
                tzPosix = "AKST9AKDT,M3.2.0,M11.1.0"; // Alaska
            } else if (lat >= 18.0f && lat <= 23.0f && lon >= -161.0f && lon <= -154.0f) {
                tzPosix = "HST10"; // Hawaii
            }
            if (tzPosix.length() > 0) {
                settingsManager.setTimezonePosix(tzPosix);
                settingsManager.save();
                logger.logfInfo("Timezone auto-detected from coordinates (%.2f, %.2f): %s", lat, lon, tzPosix.c_str());
            }
        }
        if (tzPosix.length() > 0) {
            configTzTime(tzPosix.c_str(), ntpServer);
            logger.logfInfo("Timezone set via POSIX string: %s", tzPosix.c_str());
        } else {
            // Final fallback: use legacy offset (no DST)
            int offset = settingsManager.getTimezoneOffsetHours();
            String tz = "UTC" + String(-offset);
            configTzTime(tz.c_str(), ntpServer);
            logger.logfInfo("Timezone set from offset: %s (UTC%+d)", tz.c_str(), offset);
        }
    } else {
        logger.logWarning("WiFi not connected - NTP time sync deferred");
    }
    
    // Wait a moment for NTP to sync, then calculate sunrise/sunset
    delay(2000);
    sunriseSunset.forceUpdate();

    // Initialize historical data manager (event-based capture)
    historyManager.begin(settingsManager.getHistoryEnabled(),
                         settingsManager.getHistoryBufferSize(),
                         settingsManager.getHistoryTempMinIntervalSeconds(),
                         settingsManager.getHistoryFlowMinIntervalSeconds());
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

    // Initialize OTA update manager
    updateManager.begin(&hal, settingsManager.getManifestUrl());
    webServer.setUpdateManager(&updateManager);
    logger.logInfo("Update manager initialized");

    // Initialize Telegram bot
    telegramBot.begin(&hal);
    telegramBot.setEnabled(settingsManager.getTelegramEnabled());
    telegramBot.setBotToken(settingsManager.getTelegramBotToken());
    telegramBot.setChatId(settingsManager.getTelegramChatId());
    telegramBot.setPollingEnabled(settingsManager.getTelegramEnabled());
    telegramBot.setPollingIntervalMs(settingsManager.getTelegramPollingIntervalSeconds() * 1000UL);

    // Register Telegram bot commands
    telegramBot.onCommand("/status", "Show system status", [&](const String&) -> String {
        String msg = "🐔 *Coop Status*\n";
        float temp1 = sensorManager.getTemperature1F();
        float temp2 = sensorManager.getTemperature2F();
        if (!isnan(temp1)) msg += "🌡️ Sensor 1: " + String(temp1, 1) + "°F\n";
        if (!isnan(temp2)) msg += "🌡️ Sensor 2: " + String(temp2, 1) + "°F\n";
        if (isnan(temp1) && isnan(temp2)) msg += "🌡️ Temp: N/A\n";
        float waterFlow = sensorManager.getFlowRate1();
        if (waterFlow > 0) msg += "💧 Flow: " + String(waterFlow, 2) + " GPM\n";
        msg += "🚪 Door: " + String(doorController.getStateCStr()) + " (" + doorController.getPositionCStr() + ")\n";
        msg += "💧 Pump: " + String(pumpController.isPumpOn() ? "ON" : "OFF") + "\n";
        msg += "💡 Light: " + String(lightController.getCurrentBrightness()) + "%\n";
        msg += "🧠 Heap: " + String(hal.getFreeHeap()) + " bytes free";
        return msg;
    });

    telegramBot.onCommand("/door", "Control door (open/close/stop/auto)", [&](const String& args) -> String {
        if (args == "open") { doorController.open(TriggerSource::API); return "🚪 Door opening..."; }
        if (args == "close") { doorController.close(TriggerSource::API); return "🚪 Door closing..."; }
        if (args == "stop") { doorController.stop(TriggerSource::API); return "🚪 Door stopped."; }
        if (args == "auto") { doorController.setAutoMode(true, TriggerSource::API); return "🚪 Door set to AUTO."; }
        return "Usage: /door open|close|stop|auto";
    });

    telegramBot.onCommand("/pump", "Control pump (on/off/auto)", [&](const String& args) -> String {
        if (args == "on") { pumpController.turnOn(TriggerSource::API); return "💧 Pump turned ON."; }
        if (args == "off") { pumpController.turnOff(TriggerSource::API); return "💧 Pump turned OFF."; }
        if (args == "auto") { pumpController.setAutoMode(true, TriggerSource::API); return "💧 Pump set to AUTO."; }
        return "Usage: /pump on|off|auto";
    });

    telegramBot.onCommand("/light", "Control light (on/off/auto)", [&](const String& args) -> String {
        if (args == "on") { lightController.turnOn(TriggerSource::API); return "💡 Light turned ON."; }
        if (args == "off") { lightController.turnOff(TriggerSource::API); return "💡 Light turned OFF."; }
        if (args == "auto") { lightController.setAutoMode(true, TriggerSource::API); return "💡 Light set to AUTO."; }
        return "Usage: /light on|off|auto";
    });

    telegramBot.onCommand("/buzzer", "Silence buzzer", [&](const String&) -> String {
        buzzerController.silenceAlerts();
        return "🔇 Buzzer silenced.";
    });

    // Initialize notification manager
    notificationManager.begin(&hal);
    notificationManager.setTelegramBot(&telegramBot);
    notificationManager.setEmailEnabled(settingsManager.getEmailEnabled());
    notificationManager.setSmtpServer(settingsManager.getEmailSmtpServer());
    notificationManager.setSmtpPort(settingsManager.getEmailSmtpPort());
    notificationManager.setSmtpUsername(settingsManager.getEmailSmtpUsername());
    notificationManager.setSmtpPassword(settingsManager.getEmailSmtpPassword());
    notificationManager.setEmailFrom(settingsManager.getEmailFrom());
    notificationManager.setEmailTo(settingsManager.getEmailTo());
    notificationManager.setNotifyOnPumpError(settingsManager.getNotifyPumpError());
    notificationManager.setNotifyOnSensorError(settingsManager.getNotifySensorError());
    notificationManager.setNotifyOnDoorFault(settingsManager.getNotifyDoorFault());
    notificationManager.setNotifyOnWifiDisconnect(settingsManager.getNotifyWifiDisconnect());
    notificationManager.setNotifyOnSystemError(settingsManager.getNotifySystemError());
    webServer.setNotificationManager(&notificationManager);
    webServer.setTelegramBot(&telegramBot);
    logger.logInfo("Notification manager and Telegram bot initialized");

    // Initialize MQTT manager for Home Assistant integration
    {
        MQTTConfig mqttConfig;
        mqttConfig.server = settingsManager.getMqttServer();
        mqttConfig.port = settingsManager.getMqttPort();
        mqttConfig.username = settingsManager.getMqttUsername();
        mqttConfig.password = settingsManager.getMqttPassword();
        // Use MAC address (without colons) as device_id for uniqueness
        String mac = hal.wifiGetMacAddress();
        mac.replace(":", "");
        mac.toLowerCase();
        mqttConfig.device_id = mac;
        mqttConfig.device_name = settingsManager.getHostname();
        mqttConfig.hostname = settingsManager.getHostname();
        mqttConfig.fw_version = firmwareVersion;
        mqttManager.begin(mqttConfig);
        mqttManager.setEnabled(settingsManager.getMqttEnabled());

        // Register MQTT command handler
        mqttManager.onCommand([&](const String& entityId, const String& payload) {
            logger.logfDebug("MQTT command: %s = %s", entityId.c_str(), payload.c_str());

            // Switch commands (ON/OFF)
            if (entityId == "pump_auto_mode") {
                pumpController.setAutoMode(payload == "ON", TriggerSource::API);
            } else if (entityId == "light_auto_mode") {
                lightController.setAutoMode(payload == "ON", TriggerSource::API);
            } else if (entityId == "door_auto_mode") {
                doorController.setAutoMode(payload == "ON", TriggerSource::API);
            }
            // Button commands (PRESS)
            else if (entityId == "pump_on") {
                pumpController.turnOn(TriggerSource::API);
            } else if (entityId == "pump_off") {
                pumpController.turnOff(TriggerSource::API);
            } else if (entityId == "door_open_cmd") {
                doorController.open(TriggerSource::API);
            } else if (entityId == "door_close_cmd") {
                doorController.close(TriggerSource::API);
            } else if (entityId == "door_stop") {
                doorController.stop(TriggerSource::API);
            }
            // Light JSON command
            else if (entityId == "light") {
                JsonDocument cmdDoc;
                if (deserializeJson(cmdDoc, payload) == DeserializationError::Ok) {
                    if (cmdDoc["state"] == "ON") {
                        if (cmdDoc["brightness"].is<int>()) {
                            int brightness = cmdDoc["brightness"].as<int>();
                            lightController.setBrightness(brightness, TriggerSource::API);
                        }
                        lightController.turnOn(TriggerSource::API);
                    } else if (cmdDoc["state"] == "OFF") {
                        lightController.turnOff(TriggerSource::API);
                    }
                }
            }
            // Number commands (numeric values)
            else if (entityId == "temp_threshold_on") {
                float val = payload.toFloat();
                // Ensure on threshold <= off threshold
                if (val > settingsManager.getTempThresholdOffF()) {
                    settingsManager.setTempThresholdOffF(val);
                }
                settingsManager.setTempThresholdOnF(val);
                settingsManager.save();
            } else if (entityId == "temp_threshold_off") {
                float val = payload.toFloat();
                // Ensure off threshold >= on threshold
                if (val < settingsManager.getTempThresholdOnF()) {
                    settingsManager.setTempThresholdOnF(val);
                }
                settingsManager.setTempThresholdOffF(val);
                settingsManager.save();
            } else if (entityId == "light_brightness") {
                int val = payload.toInt();
                settingsManager.setLightBrightnessPercent(val);
                lightController.setBrightness(val, TriggerSource::API);
                settingsManager.save();
            }
        });

        webServer.setMQTTManager(&mqttManager);
        logger.logInfo("MQTT manager initialized");
    }

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
            logger.logfVerbose("Watchdog fed at loop iteration %lu", loopCount);
        }

        // Acquire shared state mutex before accessing controller objects.
        // Web handlers on async_tcp (core 0) also acquire this mutex,
        // preventing concurrent access to non-thread-safe Arduino Strings.
        bool stateLocked = hal.lockSharedState(100);

        // Check if restart is requested
        unsigned long currentTime = millis();
        if (settingsManager.requestRestartAt > 0 && currentTime >= settingsManager.requestRestartAt)
        {
            logger.logWarning("Restarting device due to WiFi settings change...");
            delay(1000); // Allow time for log to be sent
            hal.restart();
        }

        // Helper: get best available temperature (sensor 1 preferred, fallback to sensor 2)
        auto getBestTemperature = [&sensorManager]() -> float {
            float temp = sensorManager.getTemperature1F();
            if (isnan(temp)) temp = sensorManager.getTemperature2F();
            return temp;
        };

        // Update temperature sensors
        if (currentTime - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
        {
            lastSensorUpdate = currentTime;
            sensorManager.update();

            // Check for state changes and record historical data (event-based)
            historyManager.checkAndRecord(
                getBestTemperature(),
                pumpController.isPumpOn(),
                sensorManager.getFlowRate1(),
                lightController.getCurrentBrightness(),
                doorController.getStateCStr(),
                doorController.getPositionCStr(),
                pumpController.getLastTriggerSourceCStr(),
                doorController.getLastTriggerSourceCStr(),
                lightController.getLastTriggerSourceCStr()
            );

            // Log temperature readings periodically
            static unsigned long lastTempLog = 0;
            if (currentTime - lastTempLog >= 60000) // Log every minute
            {
                lastTempLog = currentTime;
                // Log temperature readings only if valid
                if (float temp1 = sensorManager.getTemperature1F(); !isnan(temp1)) { // NOSONAR - complexity ok
                    logger.logfDebug("Sensor 1: %.1f°F", temp1);
                }

                if (float temp2 = sensorManager.getTemperature2F(); !isnan(temp2)) { // NOSONAR - complexity ok
                    logger.logfDebug("Sensor 2: %.1f°F", temp2);
                }

                // Check for sensor errors and trigger buzzer alerts
                static unsigned long lastSensorErrorAlert = 0;

                // Detailed logging for sensor status debugging
                bool hasWorkingTemperature = false;
                bool hasWorkingWaterMeter = false;
                bool sensor1Error = false;
                bool sensor2Error = false;

                // Helper for sensor type name
                auto sensorTypeName = [](SensorType t) -> const char* {
                    return t == SensorType::DALLAS_TEMP ? "DALLAS_TEMP" :
                           t == SensorType::WATER_METER ? "WATER_METER" : "NONE";
                };

                // Check Sensor 1 status
                logger.logfDebug("Sensor 1 status - Type: %s, Was detected: %s, Connected: %s",
                            sensorTypeName(sensorManager.getSensor1Type()),
                            sensorManager.isSensor1Detected() ? "Yes" : "No",
                            sensorManager.isSensor1Connected() ? "Yes" : "No");

                if (sensorManager.getSensor1Type() == SensorType::DALLAS_TEMP) { // NOSONAR - complexity ok
                    float temp1 = sensorManager.getTemperature1F();
                    if (!isnan(temp1)) {
                        hasWorkingTemperature = true;
                        logger.logfDebug("Sensor 1 - Temperature: %.1f°F (Working)", temp1);
                    } else {
                        sensor1Error = true;
                        logger.logDebug("Sensor 1 - Temperature: NaN (ERROR)");
                    }
                } else if (sensorManager.getSensor1Type() == SensorType::WATER_METER) {
                    bool activelyConnected = sensorManager.isActivelyConnected(sensorManager.getSensor1Data());
                    if (activelyConnected) {
                        hasWorkingWaterMeter = true;
                        logger.logfDebug("Sensor 1 - Water meter active (%.2f GPM) (Working)", sensorManager.getFlowRate1());
                    } else {
                        sensor1Error = true;
                        logger.logDebug("Sensor 1 - Water meter inactive (ERROR)");
                    }
                }

                // Check Sensor 2 status
                logger.logfDebug("Sensor 2 status - Type: %s, Was detected: %s, Connected: %s",
                            sensorTypeName(sensorManager.getSensor2Type()),
                            sensorManager.isSensor2Detected() ? "Yes" : "No",
                            sensorManager.isSensor2Connected() ? "Yes" : "No");

                if (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP) { // NOSONAR - complexity ok
                    float temp2 = sensorManager.getTemperature2F();
                    if (!isnan(temp2)) {
                        hasWorkingTemperature = true;
                        logger.logfDebug("Sensor 2 - Temperature: %.1f°F (Working)", temp2);
                    } else {
                        sensor2Error = true;
                        logger.logDebug("Sensor 2 - Temperature: NaN (ERROR)");
                    }
                } else if (sensorManager.getSensor2Type() == SensorType::WATER_METER) {
                    bool activelyConnected = sensorManager.isActivelyConnected(sensorManager.getSensor2Data());
                    if (activelyConnected) {
                        hasWorkingWaterMeter = true;
                        logger.logfDebug("Sensor 2 - Water meter active (%.2f GPM) (Working)", sensorManager.getFlowRate2());
                    } else {
                        sensor2Error = true;
                        logger.logDebug("Sensor 2 - Water meter inactive (ERROR)");
                    }
                }

                // Only trigger sensor error if we have no working temperature sensors AND no working water meters
                bool sensorError = (!hasWorkingTemperature && !hasWorkingWaterMeter);

                logger.logfDebug("Sensor error analysis - Sensor 1 Error: %s, Sensor 2 Error: %s, Has Working Temperature: %s, Has Working Water Meter: %s, Overall Sensor Error: %s",
                            sensor1Error ? "Yes" : "No", sensor2Error ? "Yes" : "No",
                            hasWorkingTemperature ? "Yes" : "No", hasWorkingWaterMeter ? "Yes" : "No",
                            sensorError ? "Yes" : "No");
                
                if (sensorError && (currentTime - lastSensorErrorAlert > 60000)) { // NOSONAR - complexity ok
                    logger.logWarning("Triggering SENSOR_ERROR alert - No working sensors detected");
                    buzzerController.triggerAlert(AlertType::SENSOR_ERROR);
                    notificationManager.notify(AlertType::SENSOR_ERROR, "No working sensors detected. Check sensor connections.");
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

            // Capture web-triggered pump changes BEFORE update() potentially changes trigger
            historyManager.checkAndRecord(
                getBestTemperature(),
                pumpController.isPumpOn(),
                sensorManager.getFlowRate1(),
                lightController.getCurrentBrightness(),
                doorController.getStateCStr(),
                doorController.getPositionCStr(),
                pumpController.getLastTriggerSourceCStr(),
                doorController.getLastTriggerSourceCStr(),
                lightController.getLastTriggerSourceCStr()
            );

            // Update pump controller with current status
            pumpController.update();

            // Check for pump flow error and trigger buzzer alert
            static unsigned long lastPumpErrorAlert = 0;
            if (pumpController.hasFlowError()) {
                if (currentTime - lastPumpErrorAlert > 60000) { // NOSONAR - nested ok, Only alert once per minute
                    logger.logWarning("Pump flow error detected - triggering buzzer alert");
                    buzzerController.triggerAlert(AlertType::PUMP_ERROR);
                    notificationManager.notify(AlertType::PUMP_ERROR, "Pump flow error detected. Check water supply and pump connections.");
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
                logger.logfDebug("Sensor 1 (Pin %d): %.1f°F (%s)", TEMP_METER_PIN, sensorManager.getTemperature1F(),
                    sensorManager.getSensor1Type() == SensorType::DALLAS_TEMP ? "Temperature" : "Water Meter");
            }
            if (sensorManager.isSensor2Connected()) {
                if (sensorManager.getSensor2Type() == SensorType::DALLAS_TEMP) { // NOSONAR - nested ok
                    float temp2 = sensorManager.getTemperature2F();
                    if (!isnan(temp2)) {
                        logger.logfDebug("Sensor 2 (Pin %d): %.1f°F (Temperature)", TEMP_METER_2_PIN, temp2);
                    }
                } else {
                    logger.logfDebug("Sensor 2 (Pin %d): %.2f GPM, %lu pulses (Water Meter)", TEMP_METER_2_PIN, sensorManager.getFlowRate2(), sensorManager.getPulseCount2());
                }
            }

            float currentTemp = sensorManager.getTemperature1F();
            // Try to get temperature from sensor 1 first, then sensor 2
            if (isnan(currentTemp)) {
                currentTemp = sensorManager.getTemperature2F();
            }

            float threshold = settingsManager.getTempThresholdOnF();
            bool tempBelowThreshold = sensorManager.isTemperatureBelowThreshold();
            if (!isnan(currentTemp)) {
                if (tempBelowThreshold) { // NOSONAR - nesting ok
                    logger.logfInfo("Temperature below threshold (%.1f°F < %.2f°F)", currentTemp, threshold);
                } else {
                    logger.logfInfo("Temperature above threshold (%.1f°F >= %.2f°F)", currentTemp, settingsManager.getTempThresholdOffF());
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

            // Capture web-triggered door changes BEFORE update() potentially changes trigger
            historyManager.checkAndRecord(
                getBestTemperature(),
                pumpController.isPumpOn(),
                sensorManager.getFlowRate1(),
                lightController.getCurrentBrightness(),
                doorController.getStateCStr(),
                doorController.getPositionCStr(),
                pumpController.getLastTriggerSourceCStr(),
                doorController.getLastTriggerSourceCStr(),
                lightController.getLastTriggerSourceCStr()
            );

            doorController.update();
        }

        // Update light controller
        if (currentTime - lastLightUpdate >= LIGHT_UPDATE_INTERVAL)
        {
            lastLightUpdate = currentTime;

            // Capture web-triggered light changes BEFORE update() potentially changes trigger
            historyManager.checkAndRecord(
                getBestTemperature(),
                pumpController.isPumpOn(),
                sensorManager.getFlowRate1(),
                lightController.getCurrentBrightness(),
                doorController.getStateCStr(),
                doorController.getPositionCStr(),
                pumpController.getLastTriggerSourceCStr(),
                doorController.getLastTriggerSourceCStr(),
                lightController.getLastTriggerSourceCStr()
            );

            lightController.update();
        }
        
        // Update sunrise/sunset calculations (check every minute, but only recalculates every 24 hours)
        static unsigned long lastSunUpdate = 0;
        if (currentTime - lastSunUpdate >= 60000) { // NOSONAR - declaration clearer above, Check every minute
            lastSunUpdate = currentTime;
            sunriseSunset.update();
        }

        // Release shared state mutex before web server loop and network I/O
        if (stateLocked) {
            hal.unlockSharedState();
            stateLocked = false;
        }

        webServer.loop();

        // OTA update: check for deferred install requests every loop,
        // and periodic auto-update checks based on settings interval
        updateManager.update();

        // Poll Telegram bot for incoming commands, but only when free heap is
        // healthy. Each poll allocates a WiFiClientSecure TLS context + JSON
        // docs; under heap pressure those allocations throw std::bad_alloc and,
        // with -fexceptions on and no catch handler on this task, reboot the
        // device (issue #4). Deferring non-essential polling lets heap recover.
        if (hal.getFreeHeap() >= NETWORK_LOW_HEAP_FLOOR) {
            telegramBot.update();
        }

        // Update MQTT manager (connection, message processing, state publishing)
        mqttManager.update();

        // Update MQTT state data (MQTTManager handles change detection and publish timing)
        if (mqttManager.isEnabled()) {
            MQTTStateData mqttState;
            mqttState.sensor1_temp_f = sensorManager.getTemperature1F();
            mqttState.sensor2_temp_f = sensorManager.getTemperature2F();
            mqttState.sensor1_connected = sensorManager.isSensor1Connected();
            mqttState.sensor2_connected = sensorManager.isSensor2Connected();
            mqttState.water_flow_rate = sensorManager.getFlowRate1();
            float pulsesPerGal = settingsManager.getPulsesPerGallon();
            mqttState.water_total_gallons = pulsesPerGal > 0 ? sensorManager.getPulseCount1() / pulsesPerGal : 0;
            mqttState.pump_running = pumpController.isPumpOn();
            mqttState.pump_auto_mode = pumpController.getState() == PumpState::PUMP_AUTO;
            mqttState.water_flow_error = pumpController.hasFlowError();
            mqttState.door_open = doorController.getPosition() == DoorPosition::OPEN;
            mqttState.door_closed = doorController.getPosition() == DoorPosition::CLOSED;
            mqttState.door_auto_mode = doorController.isAutoMode();
            mqttState.light_on = lightController.getCurrentBrightness() > 0;
            mqttState.light_brightness = lightController.getCurrentBrightness();
            mqttState.light_auto_mode = lightController.isAutoMode();
            mqttState.temp_threshold_on = settingsManager.getTempThresholdOnF();
            mqttState.temp_threshold_off = settingsManager.getTempThresholdOffF();
            mqttState.wifi_rssi = hal.wifiGetRSSI();
            mqttState.free_heap = hal.getFreeHeap();
            mqttState.uptime_seconds = currentTime / 1000;
            mqttManager.setState(mqttState);
        }

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
                    notificationManager.notify(AlertType::LOW_MEMORY, "System memory critically low (>80% used). Consider restarting.");
                    lastLowMemoryAlert = currentTime;
                }
            }
            
            uint32_t minFreeHeap = hal.getMinFreeHeap();
            logger.logfInfo("Heap: %u free, %u min, %.1f%% used", heapFree, minFreeHeap, heapUsedPercent);
        }
    }
}

void loop() {
    // Loop is handled inside setup() for this application
    // Empty - all logic is in setup() loop
}
