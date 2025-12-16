#ifndef __WIFI_CONTROLLER_H__
#define __WIFI_CONTROLLER_H__

#include <Arduino.h>
#include "SettingsManager.h"
#include "BuzzerController.h"

// WiFi connection states
enum class WifiState {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_AP_MODE
};

// WiFi status structure
struct WifiStatus {
    WifiState state = WifiState::WIFI_DISCONNECTED;
    String ssid = "";
    String ip = "";
    bool hasConnected = false;
    bool isReconnecting = false;
    int retryCount = 0;
    unsigned long apModeStartTime = 0;
};

class WifiController {
private:
    SettingsManager* settingsManager_;
    BuzzerController* buzzerController_;
    
    // WiFi connection monitoring variables
    unsigned long lastWifiCheck = 0;
    unsigned long wifiReconnectStart = 0;
    unsigned long wifiAPModeStart = 0;
    bool isReconnecting = false;
    bool isInAPMode_ = false;
    int wifiRetryCount = 0;
    
    // WiFi LED control variables
    unsigned long lastLedToggle = 0;
    bool ledState = false;
    
    // Hostname and passwords (from build flags)
    const char* hostName_;
    const char* apPasswd_;
    
    // Private methods
    void failWifi();
    void wifiSetup();
    void checkWifiConnection();
    void updateWifiLed();
    
public:
    // Constructor
    WifiController() = default;
    
    // Initialization
    void begin(SettingsManager* settings, BuzzerController* buzzer, const char* hostName, const char* apPasswd);
    
    // Main update function - call this in loop()
    void update();
    
    // Status methods
    WifiStatus getStatus() const;
    bool isConnected() const;
    bool isInAPMode() const;
    String getIPAddress() const;
    String getSSID() const;
    int getRSSI() const;
    
    // Control methods
    void connectToWiFi();
    void startAPMode();
    void disconnect();
    
    // LED control
    void enableLed(bool enabled);
    void updateLed();
};

#endif // __WIFI_CONTROLLER_H__