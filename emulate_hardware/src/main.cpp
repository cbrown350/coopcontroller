/**
 * @file main.cpp
 * @brief Main application entry point for ESP32 Hardware Emulator
 *
 * This firmware runs on a second ESP32 that physically connects to the main
 * Coop Controller ESP32. It emulates all external hardware components:
 * - Water meter pulse generation
 * - Door position via hall effect sensors
 * - Manual door switch
 * - Door motor fault signal
 *
 * It also monitors signals from the main controller:
 * - Pump relay state
 * - Light PWM output
 * - Door motor direction
 * - Buzzer activity
 * - WiFi LED status
 *
 * A web UI allows developers to:
 * - View real-time status of all signals
 * - Manually override emulated outputs
 * - Inject faults for testing
 * - Configure emulation parameters
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "EmulatorStateManager.h"
#include "EmulatorSettings.h"
#include "EmulatorWebServer.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

EmulatorStateManager stateManager;
EmulatorWebServer webServer(80);

// ============================================================================
// WIFI MANAGEMENT
// ============================================================================

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_MODE
};

WifiState wifiState = WifiState::DISCONNECTED;
uint32_t lastWifiAttempt = 0;
uint32_t wifiConnectStart = 0;

void setupWifi() {
    const String& ssid = emulatorSettings.getWifiSsid();
    const String& password = emulatorSettings.getWifiPassword();

    if (ssid.length() == 0 || emulatorSettings.isApMode()) {
        // Start in AP mode
        Serial.println("[WiFi] Starting AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(hostName, apPasswd);
        Serial.printf("[WiFi] AP started: %s\n", hostName);
        Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        wifiState = WifiState::AP_MODE;
    } else {
        // Connect to WiFi
        Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(hostName);
        WiFi.begin(ssid.c_str(), password.c_str());
        wifiState = WifiState::CONNECTING;
        wifiConnectStart = millis();
    }
}

void updateWifi() {
    switch (wifiState) {
        case WifiState::CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Connected!");
                Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

                // Start mDNS
                if (MDNS.begin(hostName)) {
                    Serial.printf("[WiFi] mDNS started: %s.local\n", hostName);
                }

                wifiState = WifiState::CONNECTED;

                // Update settings to remember we're not in AP mode
                emulatorSettings.setApMode(false);
                emulatorSettings.save();
            }
            else if (millis() - wifiConnectStart > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("[WiFi] Connection timeout, starting AP mode...");
                WiFi.disconnect();
                WiFi.mode(WIFI_AP);
                WiFi.softAP(hostName, apPasswd);
                wifiState = WifiState::AP_MODE;
                emulatorSettings.setApMode(true);
            }
            break;

        case WifiState::CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Disconnected, reconnecting...");
                wifiState = WifiState::DISCONNECTED;
                lastWifiAttempt = millis();
            }
            break;

        case WifiState::DISCONNECTED:
            if (millis() - lastWifiAttempt > WIFI_RECONNECT_INTERVAL_MS) {
                setupWifi();
            }
            break;

        case WifiState::AP_MODE:
            // Stay in AP mode until settings change
            break;
    }
}

// ============================================================================
// STATUS LED
// ============================================================================

uint32_t lastLedToggle = 0;
bool ledState = false;

void updateStatusLed() {
    uint32_t interval;

    switch (wifiState) {
        case WifiState::CONNECTED:
            interval = 2000;  // Slow heartbeat when connected
            break;
        case WifiState::AP_MODE:
            interval = 500;   // Medium blink in AP mode
            break;
        default:
            interval = 100;   // Fast blink when disconnected/connecting
            break;
    }

    if (millis() - lastLedToggle >= interval) {
        ledState = !ledState;
        digitalWrite(EMU_STATUS_LED_PIN, ledState);
        lastLedToggle = millis();
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32 Hardware Emulator");
    Serial.printf("   Firmware: %s\n", firmwareVersion);
    Serial.printf("   Hostname: %s\n", hostName);
    Serial.println("========================================");

    // Initialize settings (mounts LittleFS)
    if (!emulatorSettings.begin()) {
        Serial.println("[Setup] Settings initialization failed!");
    }

    // Initialize state manager (configures GPIO)
    stateManager.begin();

    // Apply settings to state manager
    emulatorSettings.applyToStateManager(stateManager);

    // Setup WiFi
    setupWifi();

    // Start web server
    webServer.begin(stateManager);

    // Initialize watchdog (30 second timeout)
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(nullptr);

    Serial.println("[Setup] Initialization complete");
    Serial.println();
}

// ============================================================================
// LOOP
// ============================================================================

uint32_t lastStatusLog = 0;
constexpr uint32_t STATUS_LOG_INTERVAL_MS = 10000;

void loop() {
    // Feed watchdog
    esp_task_wdt_reset();

    // Update WiFi connection
    updateWifi();

    // Update status LED
    updateStatusLed();

    // Update emulator state machine
    stateManager.update();

    // Process web server
    webServer.loop();

    // Periodic status log
    if (millis() - lastStatusLog >= STATUS_LOG_INTERVAL_MS) {
        const auto& monitored = stateManager.getMonitoredSignals();
        const auto& emulated = stateManager.getEmulatedOutputs();

        Serial.printf("[Status] Pump:%s Light:%d%% Motor:%s Door:%d%% Buzzer:%s\n",
                      monitored.pumpActive ? "ON" : "OFF",
                      monitored.lightBrightness,
                      monitored.motorDirection == MotorDirection::OPENING ? "OPEN" :
                      monitored.motorDirection == MotorDirection::CLOSING ? "CLOSE" : "STOP",
                      emulated.doorPosition,
                      monitored.buzzerActive ? "ON" : "OFF");

        Serial.printf("[Status] Hall: Open=%s Close=%s | Pulses: Ch1=%lu Ch2=%lu\n",
                      emulated.hallOpenActive ? "Y" : "N",
                      emulated.hallCloseActive ? "Y" : "N",
                      emulated.channel1PulseCount,
                      emulated.channel2PulseCount);

        Serial.printf("[Status] Heap: %lu/%lu bytes (%.1f%% used)\n",
                      ESP.getFreeHeap(), ESP.getHeapSize(),
                      100.0f * (1.0f - (float)ESP.getFreeHeap() / ESP.getHeapSize()));

        lastStatusLog = millis();
    }

    // Small delay to prevent tight loop
    delay(1);
}
