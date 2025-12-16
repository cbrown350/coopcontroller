#include "config.h"
#include "WifiController.h"
#include "Logger.h"
#include "BuzzerController.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "esp_task_wdt.h"
#include "mDNS.h"
#include <stdint.h>

// Define constants (from main.cpp)
#define WIFI_CHECK_INTERVAL 30000     // Check WiFi every 30 seconds
#define WIFI_RECONNECT_TIMEOUT 10000  // Wait 10 seconds for reconnection


void WifiController::begin(SettingsManager* settings, BuzzerController* buzzer, const char* _hostName, const char* _apPasswd) {
    settingsManager_ = settings;
    buzzerController_ = buzzer;
    hostName_ = _hostName;
    apPasswd_ = _apPasswd;

    // Initialize WiFi LED if enabled
    if (settingsManager_->getWifiLedEnabled()) {
        pinMode(WIFI_LED_B_PIN, OUTPUT);
        digitalWrite(WIFI_LED_B_PIN, HIGH); // Turn off LED initially (assuming active LOW)
        logger.logInfo("WiFi status LED initialized on pin " + String(WIFI_LED_B_PIN));
    }
    
    wifiSetup();
}

void WifiController::update() {
    unsigned long currentTime = millis();    
    // Check WiFi connection periodically
    if (currentTime - lastWifiCheck >= WIFI_CHECK_INTERVAL) { // NOSONAR - clearer declared above
        lastWifiCheck = currentTime;
        checkWifiConnection();
    }
    
    // Update WiFi LED status
    if (settingsManager_->getWifiLedEnabled()) {
        updateWifiLed();
    }


    if (settingsManager_->getWifiChanged())
    {
        logger.logInfo("Wifi changed, requesting restart");
        settingsManager_->requestRestartAt = millis() + 3000;
        settingsManager_->setWifiChanged(false);
    }
}

WifiStatus WifiController::getStatus() const {
    WifiStatus status;
    status.state = isInAPMode_ ? WifiState::WIFI_AP_MODE : 
        (WiFi.status() == WL_CONNECTED ? WifiState::WIFI_CONNECTED : WifiState::WIFI_DISCONNECTED); // NOSONAR - nested ok
    status.ssid = WiFi.SSID();
    status.ip = WiFi.localIP().toString();
    status.hasConnected = settingsManager_->getHasConnected();
    status.isReconnecting = isReconnecting;
    status.retryCount = wifiRetryCount;
    status.apModeStartTime = wifiAPModeStart;
    return status;
}

bool WifiController::isConnected() const {
    return WiFi.isConnected();
}

bool WifiController::isInAPMode() const {
    return isInAPMode_;
}

String WifiController::getIPAddress() const {
    return WiFi.localIP().toString();
}

String WifiController::getSSID() const {
    return WiFi.SSID();
}

int WifiController::getRSSI() const {
    return WiFi.RSSI();
}

void WifiController::connectToWiFi() {
    // Implementation for manual connection if needed
    // For now, just call wifiSetup
    wifiSetup();
}

void WifiController::startAPMode() {
    settingsManager_->setAPMode(true);
    settingsManager_->save();
    delay(1000);
    ESP.restart();
}

void WifiController::disconnect() { // NOSONAR - modifies state
    WiFi.disconnect();
}

void WifiController::enableLed(bool enabled) {
    settingsManager_->setWifiLedEnabled(enabled);
    if (enabled) {
        pinMode(WIFI_LED_B_PIN, OUTPUT);
        digitalWrite(WIFI_LED_B_PIN, HIGH);
    }
}

void WifiController::updateLed() {
    updateWifiLed();
}

// Private methods (adapted from main.cpp)

void WifiController::failWifi() {
    // Only revert to AP mode if WiFi has never successfully connected
    if (!settingsManager_->getHasConnected()) {
        settingsManager_->setAPMode(true);
        if (settingsManager_->save()) {
            logger.logWarning("Failed to connect to WiFi, reverted to AP mode");
        } else {
            logger.logError("Failed to update settings for AP mode");
        }

        delay(1000);  // Give time for serial output
        ESP.restart();
    } else {
        logger.logInfo("WiFi connection failed, retrying in 30 seconds");
        // Don't restart, just continue trying to reconnect in checkWifiConnection()
    }
}

void WifiController::wifiSetup() { // NOSONAR - complexity ok
    if (settingsManager_->isAPMode()) {
        logger.logInfo("Starting AP mode for " + String(settingsManager_->getWifiAPDurationMinutes()) + " minutes");
        if (apPasswd_ && strlen(apPasswd_) > 0) {
            WiFi.softAP(hostName_, apPasswd_);
            logger.logDebug("AP password set: " + String(apPasswd_));
        } else {
            WiFi.softAP(hostName_, nullptr);
        }
        WiFi.softAPsetHostname(hostName_); 
        logger.logInfo("AP mode started, IP address: " + WiFi.softAPIP().toString());
        isInAPMode_ = true;
        wifiAPModeStart = millis();
        
        // Add logging for WiFi task status
        logger.logDebug("WiFi AP mode started on core " + String(xPortGetCoreID()));
        logger.logDebug("Current WiFi task handle: " + String((uint32_t)xTaskGetCurrentTaskHandle(), HEX));
    } else {
        String ssid = settingsManager_->getSSID();
        String password = settingsManager_->getPassword();
        
        if (ssid.isEmpty()) {
            logger.logWarning("No SSID configured, falling back to AP mode");
            settingsManager_->setAPMode(true);
            settingsManager_->save();
            settingsManager_->printSettingsDebug();
            delay(1000);
            ESP.restart();
            return;
        }
        
        if (hostName_ && strlen(hostName_) > 0) {
            WiFiClass::setHostname(hostName_); // Need to set hostname in all places for mDNS to work
            logger.logDebug("Hostname set to: " + String(hostName_));
        } 
        WiFi.persistent(false); // Fix for issues with reconnection, credentials are stored in settingsManager
        logger.logInfo("Connecting to WiFi: " + ssid);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        int maxRetries = settingsManager_->getWifiMaxRetries();
        int retryDelay = settingsManager_->getWifiRetryDelaySeconds();

        wifiRetryCount = 0;
        while (!WiFi.isConnected() && wifiRetryCount < maxRetries) {            
            esp_task_wdt_reset();  

            logger.logDebug(".");
            delay(retryDelay * 1000);
            wifiRetryCount++;  

            // Add some debugging
            logger.logDebug("WiFi status: " + String(WiFiClass::status()) + ", attempt " + 
                    String(wifiRetryCount) + "/" + String(maxRetries));
        }

        logger.logDebug(""); // New line after dots
        
        if (WiFi.isConnected()) {
            logger.logInfo("WiFi Connected, IP address: " + WiFi.localIP().toString());
            isInAPMode_ = false;
            
            if (hostName_ && strlen(hostName_) > 0) {
                int mDNSRetries = 5;
                while(mDNSRetries > 0 && !MDNS.begin(hostName_)) { // NOSONAR - nesting ok
                    logger.logDebug("Starting mDNS...");
                    delay(1000);
                    mDNSRetries--;
                }
                
                logger.logDebug("mDNS started"); 
            }

            // Mark that WiFi has successfully connected at least once
            if (!settingsManager_->getHasConnected()) {
                settingsManager_->setHasConnected(true);
                settingsManager_->save();
                logger.logInfo("First successful WiFi connection recorded");
            }
            
            // Add logging for WiFi task status
            logger.logDebug("WiFi connected on core " + String(xPortGetCoreID()));
            logger.logDebug("Current WiFi task handle: " + String((uint32_t)xTaskGetCurrentTaskHandle(), HEX));
        } else {
            logger.logWarning("Failed to connect to WiFi after " + String(wifiRetryCount) + " attempts");
            failWifi();
        }
    }
}

void WifiController::checkWifiConnection() { // NOSONAR - complexity ok
    // Check if we're in AP mode and need to retry WiFi connection
    if (isInAPMode_) {
        unsigned long apDuration = settingsManager_->getWifiAPDurationMinutes() * 60000; // Convert to milliseconds
        if (millis() - wifiAPModeStart >= apDuration && !settingsManager_->getSSID().isEmpty()){ // NOSONAR - clearer declared above
          logger.logInfo("AP mode duration expired, attempting WiFi connection");
          settingsManager_->setAPMode(false);
          settingsManager_->save();
          delay(1000);
          ESP.restart();
        }
        return;
    }

    // Skip check if already in AP mode
    if (settingsManager_->isAPMode()) {
        return;
    }

    // Check if WiFi is connected
    if (!WiFi.isConnected()) {
        if (!isReconnecting) {
            logger.logWarning("WiFi disconnected, attempting to reconnect...");
            if (buzzerController_) {
                buzzerController_->triggerAlert(AlertType::WIFI_DISCONNECTED);
            }
            String ssid = settingsManager_->getSSID();
            String password = settingsManager_->getPassword();
            
            if (!ssid.isEmpty()) {
                WiFi.begin(ssid.c_str(), password.c_str());
                wifiReconnectStart = millis();
                isReconnecting = true;
                wifiRetryCount = 0;
                
                logger.logDebug("Starting WiFi reconnection to " + ssid);
            } else {
                logger.logError("No SSID configured for reconnection");
                failWifi();
            }
        } else {
            // Check if reconnection timeout has elapsed
            int maxRetries = settingsManager_->getWifiMaxRetries();
            int retryDelay = settingsManager_->getWifiRetryDelaySeconds();
            
            if (millis() - wifiReconnectStart >= (retryDelay * 1000 * maxRetries)) {
                logger.logWarning("WiFi reconnection timeout, switching to AP mode");
                settingsManager_->setAPMode(true);
                settingsManager_->save();
                delay(1000);
                ESP.restart();
            }
        }
    } else {
        // WiFi is connected, reset reconnection state
        if (isReconnecting) {
            logger.logInfo("WiFi reconnected successfully");
            isReconnecting = false;
            
            // Clear WiFi disconnected alert when reconnected
            if (buzzerController_) {
                buzzerController_->clearAlert(AlertType::WIFI_DISCONNECTED);
            }
            
            if (hostName_ && strlen(hostName_) > 0) {
                int mDNSRetries = 5;
                while(mDNSRetries > 0 && !MDNS.begin(hostName_)) { // NOSONAR - nesting ok
                    logger.logDebug("Starting mDNS...");
                    delay(1000);
                    mDNSRetries--;
                }
                
                logger.logDebug("mDNS started"); 
            }

            // Mark that WiFi has successfully connected at least once
            if (!settingsManager_->getHasConnected()) {
                settingsManager_->setHasConnected(true);
                settingsManager_->save();
            }
        }
    }
}

void WifiController::updateWifiLed() {
    if (!settingsManager_->getWifiLedEnabled()) {
        return;
    }

    unsigned long currentMillis = millis();

    if (WiFiClass::status() == WL_CONNECTED) {
        // Heartbeat pattern: 50ms ON, 1950ms OFF
        unsigned long interval = (!ledState) ? 50 : 1950;
        if (currentMillis - lastLedToggle >= interval) {
            lastLedToggle = currentMillis;
            ledState = !ledState;
            digitalWrite(WIFI_LED_B_PIN, ledState ? LOW : HIGH); // LOW to turn on (active low)
        }
    } else if (isInAPMode_) {
        // AP mode: 250ms ON, 250ms OFF
        if (currentMillis - lastLedToggle >= 250) {
            lastLedToggle = currentMillis;
            ledState = !ledState;
            digitalWrite(WIFI_LED_B_PIN, ledState ? LOW : HIGH); // LOW to turn on
        }
    } else {
        // Disconnected: Turn OFF (no toggle)
        digitalWrite(WIFI_LED_B_PIN, HIGH); // HIGH to turn off (active low)
        ledState = false;
        lastLedToggle = currentMillis; // Prevent unnecessary checks
    }
}