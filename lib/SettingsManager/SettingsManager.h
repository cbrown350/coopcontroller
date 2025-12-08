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
    int    pump_on_time_seconds;    // Pump ON time in seconds (default 30)
    int    pump_off_time_seconds;   // Pump OFF time in seconds (default 300)
    bool   pump_auto_mode;          // Enable automatic pump control based on temperature
    bool   light_auto_mode;         // Enable automatic light control
    int    light_on_minute;           // Minute to turn on light (0-59)
    String light_on_mode;           // 'fixed' or 'sunset_offset'
    int    light_on_sunset_offset_minutes; // Minutes before/after sunset for light on (default: 0)
    int    light_on_hour;           // Hour to turn on light (24-hour format)
    int    light_off_hour;          // Hour to turn off light (24-hour format)
    int    light_brightness_percent; // Maximum brightness percentage (0-100)
    int    light_transition_duration_minutes; // Fade transition duration in minutes
    int    water_flow_error_timeout_seconds; // Timeout for water flow error detection in seconds (default 10 = 10 seconds)
    String log_level;               // Log level: "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR" (default "INFO")
    
    // Water meter calibration
    float  pulses_per_gallon;       // Pulses per gallon for water meter calibration (default 450.0)
    int    water_meter_timeout_seconds; // Timeout in seconds before water meter considered disconnected (default 300)
    
    // WiFi connection settings
    int    wifi_max_retries;        // Maximum number of WiFi connection retries (default 5)
    int    wifi_retry_delay_seconds;  // Delay between WiFi retry attempts in seconds (default 30)
    int    wifi_ap_duration_minutes;  // How long to stay in AP mode before retrying (default 10)
    int    watchdog_timeout_seconds; // Watchdog timeout in seconds (default 30, range 10-120)
    bool   wifi_led_enabled;        // Enable WiFi status LED (default: true)
    
    // Buzzer settings
    bool   buzzer_enabled;         // Enable buzzer alerts (default: true)
    String buzzer_type;            // Buzzer type: "ACTIVE" or "PASSIVE" (default: "ACTIVE")
    
    // Door control settings
    bool   door_auto_mode;          // Enable automatic door control (default: false)
    int    door_open_timeout_seconds; // Door open timeout in seconds (default: 30)
    int    door_close_timeout_seconds; // Door close timeout in seconds (default: 30)
    int    sunrise_offset_minutes;   // Sunrise offset for door opening (default: 0)
    int    sunset_offset_minutes;    // Sunset offset for door closing (default: 0)
    
    // Location settings for sunrise/sunset calculations
    float  latitude;               // Latitude for sunrise/sunset calculations (default: 40.7128 - NYC)
    float  longitude;              // Longitude for sunrise/sunset calculations (default: -74.0060 - NYC)
    int    timezone_offset_hours;   // UTC timezone offset in hours (default: -5 - EST)
    
    // Task 3.5k preparation
    bool   door_auto_close_after_sunset_enabled; // Enable auto-close X minutes after sunset (default: false)
    int    door_auto_close_after_sunset_minutes;  // Minutes after sunset to auto-close (default: 0)
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
    
    String loadFile();

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
    int    getLightOnMinute() const;
    void   setLightOnMinute(int minute);
    String getLightOnMode() const;
    void   setLightOnMode(const String& mode);
    int    getLightOnSunsetOffsetMinutes() const;
    void   setLightOnSunsetOffsetMinutes(int minutes);
    int    getPumpOffTimeSeconds();
    bool   getLightAutoMode() const;
    int    getLightOnHour();
    int    getLightOffHour();
    int    getLightBrightnessPercent() const;
    int    getLightTransitionDurationMinutes() const;
    String getLogLevel() const;
    
    // Water meter calibration getter
    float  getPulsesPerGallon() const;
    
    // Water meter timeout getter
    int    getWaterMeterTimeoutSeconds() const;
    
    // WiFi connection settings getters
    int    getWifiMaxRetries();
    int    getWifiRetryDelaySeconds();
    int    getWifiAPDurationMinutes();
    int    getWatchdogTimeoutSeconds();
    bool   getWifiLedEnabled() const;
    bool   getWifiChanged() const;
    
    // Buzzer settings getters
    bool   getBuzzerEnabled() const;
    String getBuzzerType() const;
    
    // Door control settings getters
    bool   getDoorAutoMode() const;
    int    getDoorOpenTimeoutSeconds() const;
    int    getDoorCloseTimeoutSeconds() const;
    int getSunriseOffsetMinutes() const;
    int getSunsetOffsetMinutes() const;
    
    // Location settings getters
    float getLatitude() const;
    float getLongitude() const;
    int getTimezoneOffsetHours() const;
    
    // Task 3.5k preparation getters
    bool getDoorAutoCloseAfterSunsetEnabled() const;
    int getDoorAutoCloseAfterSunsetMinutes() const;

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
    void setLightBrightnessPercent(int percent);
    void setLightTransitionDurationMinutes(int minutes);
    void setLogLevel(const String& level);
    
    // Water meter calibration setters
    void setPulsesPerGallon(float value);
    
    // Water meter timeout setter
    void setWaterMeterTimeoutSeconds(int seconds);
    
    // WiFi connection settings setters - request restart for these
    void setWifiMaxRetries(int retries);
    void setWifiRetryDelaySeconds(int seconds);
    void setWifiAPDurationMinutes(int minutes);
    void setWatchdogTimeoutSeconds(int seconds);
    void setWifiLedEnabled(bool enabled);
    void setWifiChanged(bool changed);
    
    // Buzzer settings setters
    void setBuzzerEnabled(bool enabled);
    void setBuzzerType(const String& type);
    
    // Door control settings setters
    void setDoorAutoMode(bool enabled);
    void setDoorOpenTimeoutSeconds(int seconds);
    void setDoorCloseTimeoutSeconds(int seconds);
    void setSunriseOffsetMinutes(int minutes);
    void setSunsetOffsetMinutes(int minutes);
    
    // Location settings setters
    void setLatitude(float latitude);
    void setLongitude(float longitude);
    void setTimezoneOffsetHours(int offset);
    
    // Task 3.5k preparation setters
    void setDoorAutoCloseAfterSunsetEnabled(bool enabled);
    void setDoorAutoCloseAfterSunsetMinutes(int minutes);

    void factoryReset();
    String toJson(bool includePassword = true) const;
};

#define settingsManager SettingsManager::getInstance()

#endif