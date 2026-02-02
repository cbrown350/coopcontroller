#include "config.h"
#include "WifiController.h"
#include "Logger.h"
#include "BuzzerController.h"
#include <Arduino.h>
#include <stdint.h>
#include <cstdint>

// Define constants (from main.cpp)
#define WIFI_CHECK_INTERVAL 30000     // Check WiFi every 30 seconds
#define WIFI_RECONNECT_TIMEOUT 10000  // Wait 10 seconds for reconnection


void WifiController::begin(IHAL* hal, SettingsManager* settings, BuzzerController* buzzer, const char* _hostName, const char* _apPasswd) {
    _hal = hal;
    settingsManager_ = settings;
    buzzerController_ = buzzer;
    hostName_ = _hostName;
    apPasswd_ = _apPasswd;

    // Initialize WiFi LED if enabled
    if (settingsManager_->getWifiLedEnabled()) {
        _hal->pinMode(WIFI_LED_B_PIN, OUTPUT);
        _hal->digitalWrite(WIFI_LED_B_PIN, HIGH); // Turn off LED initially (assuming active LOW)
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
        (_hal->wifiIsConnected() ? WifiState::WIFI_CONNECTED : WifiState::WIFI_DISCONNECTED); // NOSONAR - nested ok
    status.ssid = _hal->wifiGetSSID();
    status.ip = _hal->wifiGetLocalIP();
    status.hasConnected = settingsManager_->getHasConnected();
    status.isReconnecting = isReconnecting;
    status.retryCount = wifiRetryCount;
    status.apModeStartTime = wifiAPModeStart;
    return status;
}

bool WifiController::isConnected() const {
    return _hal->wifiIsConnected();
}

bool WifiController::isInAPMode() const {
    return isInAPMode_;
}

String WifiController::getIPAddress() const {
    return _hal->wifiGetLocalIP();
}

String WifiController::getSSID() const {
    return _hal->wifiGetSSID();
}

int WifiController::getRSSI() const {
    return _hal->wifiGetRSSI();
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
    _hal->restart();
}

void WifiController::disconnect() { // NOSONAR - modifies state
    _hal->wifiDisconnect();
}

void WifiController::enableLed(bool enabled) {
    settingsManager_->setWifiLedEnabled(enabled);
    if (enabled) {
        _hal->pinMode(WIFI_LED_B_PIN, OUTPUT);
        _hal->digitalWrite(WIFI_LED_B_PIN, HIGH);
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
        _hal->restart();
    } else {
        logger.logInfo("WiFi connection failed, retrying in 30 seconds");
        // Don't restart, just continue trying to reconnect in checkWifiConnection()
    }
}

void WifiController::wifiSetup() { // NOSONAR - complexity ok
    if (settingsManager_->isAPMode()) {
        logger.logInfo("Starting AP mode for " + String(settingsManager_->getWifiAPDurationMinutes()) + " minutes");
        if (apPasswd_ && strlen(apPasswd_) > 0) {
            _hal->wifiBeginAP(hostName_, apPasswd_);
            logger.logDebug("AP password configured");
        } else {
            _hal->wifiBeginAP(hostName_, (const char*)nullptr);
        }
        // Note: WiFi.softAPsetHostname() not available in HAL - using hostname from wifiBeginAP
        logger.logInfo("AP mode started, IP address: " + _hal->wifiGetAPIP());
        isInAPMode_ = true;
        wifiAPModeStart = millis();
        
        // Add logging for WiFi task status
        logger.logDebug("WiFi AP mode started on core " + String(_hal->getCoreID()));
        logger.logDebug("Current WiFi task handle: " + String(static_cast<unsigned long>(reinterpret_cast<uintptr_t>(_hal->getCurrentTaskHandle())), 16));
    } else {
        String ssid = settingsManager_->getSSID();
        String password = settingsManager_->getPassword();

        if (ssid.length() == 0) {
            logger.logWarning("No SSID configured, falling back to AP mode");
            settingsManager_->setAPMode(true);
            settingsManager_->save();
            settingsManager_->printSettingsDebug();
            delay(1000);
            _hal->restart();
            return;
        }
        
        // Note: WiFiClass::setHostname() not available in HAL - hostname handled in wifiBegin
        if (hostName_ && strlen(hostName_) > 0) {
            _hal->wifiSetHostname(hostName_); // Need to set hostname in all places for mDNS to work
            logger.logDebug("Hostname set to: " + String(hostName_));
        }
        
        // Disable auto-reconnect and persistent storage - credentials managed by settingsManager
        _hal->wifiSetAutoReconnect(false); // Disable auto-reconnect, we handle it manually
        
        logger.logInfo("Connecting to WiFi: " + ssid);
        _hal->wifiBegin(ssid.c_str(), password.c_str());
        
        int maxRetries = settingsManager_->getWifiMaxRetries();
        int retryDelay = settingsManager_->getWifiRetryDelaySeconds();

        wifiRetryCount = 0;
        while (!_hal->wifiIsConnected() && wifiRetryCount < maxRetries) {            
            _hal->taskWdtReset();  

            logger.logDebug(".");
            delay(retryDelay * 1000);
            wifiRetryCount++;  

            // Add some debugging
            logger.logDebug("WiFi status: " + String(_hal->wifiGetStatus()) + ", attempt " + 
                    String(wifiRetryCount) + "/" + String(maxRetries));
        }

        logger.logDebug(""); // New line after dots
        
        if (_hal->wifiIsConnected()) {
            logger.logInfo("WiFi Connected, IP address: " + _hal->wifiGetLocalIP());
            logger.logInfo("SSID: " + ssid);
            logger.logInfo("BSSID: " + _hal->wifiGetBSSID());
            logger.logInfo("MAC Address: " + _hal->wifiGetMacAddress());
            isInAPMode_ = false;
            
            if (hostName_ && strlen(hostName_) > 0) {
                int mDNSRetries = 5;
                while(mDNSRetries > 0 && !_hal->mdnsBegin(hostName_)) { // NOSONAR - nesting ok
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
            logger.logDebug("WiFi connected on core " + String(_hal->getCoreID()));
            logger.logDebug("Current WiFi task handle: " + String((unsigned long)reinterpret_cast<uintptr_t>(_hal->getCurrentTaskHandle()), 16));
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
        if (millis() - wifiAPModeStart >= apDuration && settingsManager_->getSSID().length() != 0){ // NOSONAR - clearer declared above
          logger.logInfo("AP mode duration expired, attempting WiFi connection");
          settingsManager_->setAPMode(false);
          settingsManager_->save();
          delay(1000);
          _hal->restart();
        }
        return;
    }

    // Skip check if already in AP mode
    if (settingsManager_->isAPMode()) {
        return;
    }

    // Check if WiFi is connected
    if (!_hal->wifiIsConnected()) {
        if (!isReconnecting) {
            logger.logWarning("WiFi disconnected, attempting to reconnect...");
            if (buzzerController_) {
                buzzerController_->triggerAlert(AlertType::WIFI_DISCONNECTED);
            }
            String ssid = settingsManager_->getSSID();
            String password = settingsManager_->getPassword();
            
            if (ssid.length() != 0) {
                _hal->wifiBegin(ssid.c_str(), password.c_str());
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
            unsigned int maxRetries = settingsManager_->getWifiMaxRetries();
            unsigned int retryDelay = settingsManager_->getWifiRetryDelaySeconds();

            if (millis() - wifiReconnectStart >= (static_cast<unsigned long>(retryDelay) * 1000UL * maxRetries)) {
                logger.logWarning("WiFi reconnection timeout, switching to AP mode");
                settingsManager_->setAPMode(true);
                settingsManager_->save();
                delay(1000);
                _hal->restart();
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
                while(mDNSRetries > 0 && !_hal->mdnsBegin(hostName_)) { // NOSONAR - nesting ok
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

    if (_hal->wifiIsConnected()) {
        // Heartbeat pattern: 50ms ON, 1950ms OFF
        unsigned long interval = (!ledState) ? 50 : 1950;
        if (currentMillis - lastLedToggle >= interval) {
            lastLedToggle = currentMillis;
            ledState = !ledState;
            _hal->digitalWrite(WIFI_LED_B_PIN, ledState ? LOW : HIGH); // LOW to turn on (active low)
        }
    } else if (isInAPMode_) {
        // AP mode: 250ms ON, 250ms OFF
        if (currentMillis - lastLedToggle >= 250) {
            lastLedToggle = currentMillis;
            ledState = !ledState;
            _hal->digitalWrite(WIFI_LED_B_PIN, ledState ? LOW : HIGH); // LOW to turn on
        }
    } else {
        // Disconnected: Turn OFF (no toggle)
        _hal->digitalWrite(WIFI_LED_B_PIN, HIGH); // HIGH to turn off (active low)
        ledState = false;
        lastLedToggle = currentMillis; // Prevent unnecessary checks
    }
}
