#ifndef __WIFI_CONTROLLER_H__
#define __WIFI_CONTROLLER_H__

#include "IHAL.h"
#include <Arduino.h>
#include "SettingsManager.h"
#include "BuzzerController.h"

/**
 * @brief WiFi connection states
 *
 * Enumeration of all possible WiFi connection states.
 */
enum class WifiState {
    WIFI_DISCONNECTED = 0,  ///< Not connected to WiFi
    WIFI_CONNECTING,        ///< Currently attempting to connect
    WIFI_CONNECTED,         ///< Successfully connected
    WIFI_AP_MODE           ///< Access Point mode active
};

/**
 * @brief Complete WiFi status structure
 *
 * Contains all current WiFi information including state,
 * connection details, and retry status.
 */
struct WifiStatus {
    WifiState state = WifiState::WIFI_DISCONNECTED; ///< Current WiFi state
    String ssid = "";                               ///< Connected SSID
    String ip = "";                                 ///< Local IP address
    bool hasConnected = false;                      ///< Has ever connected successfully
    bool isReconnecting = false;                    ///< Currently in reconnect loop
    int retryCount = 0;                             ///< Current retry attempt count
    unsigned long apModeStartTime = 0;              ///< When AP mode started
};

/**
 * @brief WiFi connection manager
 *
 * Manages WiFi connectivity with automatic reconnection, AP mode fallback,
 * and status LED feedback. Features include:
 *
 * - Automatic connection to configured network
 * - Retry logic with configurable attempts and delays
 * - AP mode fallback when connection fails
 * - Status LED indication (blinking during connect)
 * - Buzzer alerts on connection loss
 * - Automatic reconnection after disconnect
 * - mDNS hostname registration
 *
 * Connection Flow:
 * 1. Try to connect to saved SSID
 * 2. Retry up to configured maximum attempts
 * 3. Fall back to AP mode if all retries fail
 * 4. Stay in AP mode for configured duration
 * 5. Retry connection after AP mode expires
 *
 * LED Behavior:
 * - Off: Disconnected
 * - Blinking: Connecting
 * - On: Connected
 */
class WifiController {
private:
    IHAL* _hal;                        ///< Hardware abstraction layer
    SettingsManager* settingsManager_;  ///< Settings manager for WiFi credentials
    BuzzerController* buzzerController_; ///< Buzzer for connection alerts

    // WiFi connection monitoring variables
    unsigned long lastWifiCheck;        ///< Last time connection was checked
    unsigned long wifiReconnectStart;   ///< When reconnect attempts started
    unsigned long wifiAPModeStart;      ///< When AP mode was activated
    bool isReconnecting;                ///< Currently in reconnect loop
    bool isInAPMode_;                   ///< Currently in AP mode
    int wifiRetryCount;                 ///< Current retry attempt number

    // WiFi LED control variables
    unsigned long lastLedToggle;        ///< Last time LED was toggled
    bool ledState;                      ///< Current LED state

    // Hostname and passwords (from build flags)
    const char* hostName_;              ///< mDNS hostname
    const char* apPasswd_;              ///< AP mode password

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Handle WiFi connection failure
     *
     * Triggers AP mode after max retries exhausted.
     */
    void failWifi();

    /**
     * @brief Initialize WiFi connection
     *
     * Begins connection process using configured SSID/password.
     */
    void wifiSetup();

    /**
     * @brief Check WiFi connection status
     *
     * Monitors connection and handles reconnect/AP mode logic.
     */
    void checkWifiConnection();

    /**
     * @brief Update WiFi status LED
     *
     * Blinks LED during connection, solid when connected.
     */
    void updateWifiLed();
    
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    /**
     * @brief Default constructor
     *
     * Initializes WiFi controller in default state.
     * Must call begin() before use.
     */
    WifiController() = default;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize WiFi controller
     *
     * Sets up references and begins connection process.
     *
     * @param hal Pointer to hardware abstraction layer
     * @param settings Pointer to settings manager
     * @param buzzer Pointer to buzzer controller
     * @param hostName mDNS hostname for device
     * @param apPasswd Password for AP mode
     */
    void begin(IHAL* hal, SettingsManager* settings, BuzzerController* buzzer, const char* hostName, const char* apPasswd);

    // ========================================================================
    // MAIN UPDATE LOOP
    // ========================================================================

    /**
     * @brief Update WiFi controller state (call in loop)
     *
     * Handles connection monitoring, reconnection, AP mode, and LED updates.
     * Should be called every loop iteration.
     */
    void update();

    // ========================================================================
    // STATUS METHODS
    // ========================================================================

    /**
     * @brief Get complete WiFi status
     *
     * @return Current WifiStatus structure
     */
    WifiStatus getStatus() const;

    /**
     * @brief Check if connected to WiFi
     *
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Check if in AP mode
     *
     * @return true if AP mode is active
     */
    bool isInAPMode() const;

    /**
     * @brief Get local IP address
     *
     * @return IP address as string
     */
    String getIPAddress() const;

    /**
     * @brief Get connected SSID
     *
     * @return SSID string
     */
    String getSSID() const;

    /**
     * @brief Get WiFi signal strength
     *
     * @return RSSI in dBm
     */
    int getRSSI() const;

    // ========================================================================
    // CONTROL METHODS
    // ========================================================================

    /**
     * @brief Start WiFi connection
     *
     * Initiates connection to configured network.
     */
    void connectToWiFi();

    /**
     * @brief Start AP mode
     *
     * Activates access point mode for configuration.
     */
    void startAPMode();

    /**
     * @brief Disconnect from WiFi
     *
     * Disconnects and disables auto-reconnect.
     */
    void disconnect();

    // ========================================================================
    // LED CONTROL
    // ========================================================================

    /**
     * @brief Enable or disable WiFi status LED
     *
     * @param enabled true to enable LED feedback
     */
    void enableLed(bool enabled);

    /**
     * @brief Update WiFi LED state
     *
     * Called internally - blinks during connect, solid when connected.
     */
    void updateLed();
};

#endif // __WIFI_CONTROLLER_H__