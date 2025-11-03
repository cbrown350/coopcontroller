#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef SETTINGS_DATA_H
#define SETTINGS_DATA_H

struct user_settings
{
    String ssid;
    String passwd;
    bool   ap_mode;
    bool   enabled;
    bool   has_connected;
    
    // Coop Controller specific settings
    float  temp_threshold_on_f;     // Temperature threshold to turn ON pump in Fahrenheit (default 34F)
    float  temp_threshold_off_f;    // Temperature threshold to turn OFF pump in Fahrenheit (default 36F)
    int    pump_on_time_seconds;    // Pump ON time in seconds (default 150)
    int    pump_off_time_seconds;   // Pump OFF time in seconds (default 300)
    bool   pump_auto_mode;          // Enable automatic pump control based on temperature
    bool   light_auto_mode;         // Enable automatic light control (future feature)
    int    light_on_hour;           // Hour to turn on light (24-hour format)
    int    light_off_hour;          // Hour to turn off light (24-hour format)
    int    water_flow_error_timeout_seconds; // Timeout for water flow error detection in seconds (default 120 = 2 minutes)
    String log_level;               // Log level: "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR" (default "INFO")
    // int    pump_error_retry_seconds; // Time to wait before retrying pump after flow error (default 120 = 2 minutes)
    
    // Water meter calibration
    float  pulses_per_gallon;       // Pulses per gallon for water meter calibration (default 450.0)
    
    // WiFi connection settings
    int    wifi_max_retries;        // Maximum number of WiFi connection retries (default 5)
    int    wifi_retry_delay_seconds;  // Delay between WiFi retry attempts in seconds (default 30)
    int    wifi_ap_duration_minutes;  // How long to stay in AP mode before retrying (default 10)
    int    watchdog_timeout_seconds; // Watchdog timeout in seconds (default 30, range 10-120)
};

class SettingsManager
{
   private:
    user_settings settings;
    bool          isLoaded;
    bool          wifiChanged;

    SettingsManager();

    SettingsManager(const SettingsManager &)            = delete;
    SettingsManager &operator=(const SettingsManager &) = delete;

   public:
    static SettingsManager &getInstance();

    // After saving wifi settings, requests restart, TODO: maybe use an event instead of this
    unsigned long requestRestartAt;

    bool load();
    bool save();

    //  (loads if not already loaded)
    const user_settings &getSettings();

    String getSSID();
    String getPassword();
    bool   isAPMode();
    bool   getPumpAutoMode() const;
    bool   getHasConnected();
    
    // Coop Controller getters
    float  getTempThresholdOnF();
    float  getTempThresholdOffF();
    int    getWaterFlowErrorTimeoutSeconds();
    int    getPumpErrorRetrySeconds();
    int    getPumpOnTimeSeconds();
    int    getPumpOffTimeSeconds();
    bool   getLightAutoMode() const;
    int    getLightOnHour();
    int    getLightOffHour();
    String getLogLevel() const;
    
    // Water meter calibration getter
    float  getPulsesPerGallon() const;
    
    // WiFi connection settings getters
    int    getWifiMaxRetries();
    int    getWifiRetryDelaySeconds();
    int    getWifiAPDurationMinutes();
    int    getWatchdogTimeoutSeconds();

    void setSSID(const String &ssid);
    void setPassword(const String &password);
    void setAPMode(bool apMode);
    void setPumpAutoMode(bool mode);
    void setHasConnected(bool hasConnected);
    
    // Coop Controller setters - don't request restart for these
    void setTempThresholdOnF(float threshold);
    void setTempThresholdOffF(float threshold);
    void setWaterFlowErrorTimeoutSeconds(int timeout);
    void setPumpErrorRetrySeconds(int seconds);
    void setPumpOnTimeSeconds(int seconds);
    void setPumpOffTimeSeconds(int seconds);
    void setLightAutoMode(bool mode);
    void setLightOnHour(int hour);
    void setLightOffHour(int hour);
    void setLogLevel(const String& level);
    
    // Water meter calibration setters
    void setPulsesPerGallon(float value);
    
    // WiFi connection settings setters - request restart for these
    void setWifiMaxRetries(int retries);
    void setWifiRetryDelaySeconds(int seconds);
    void setWifiAPDurationMinutes(int minutes);
    void setWatchdogTimeoutSeconds(int seconds);

    String toJson(bool includePassword = true) const;
};

#define settingsManager SettingsManager::getInstance()

#endif