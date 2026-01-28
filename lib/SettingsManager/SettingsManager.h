#ifndef SETTINGS_DATA_H
#define SETTINGS_DATA_H

#include "config.h"

#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief User settings structure
 *
 * Contains all configurable settings for the chicken coop controller.
 * Stored in JSON format in non-volatile storage (LittleFS).
 *
 * Default values are defined for each setting to ensure
 * valid operation even on first boot.
 */
struct user_settings // NOSONAR
{
    String ssid;                         ///< WiFi SSID
    String passwd;                       ///< WiFi password
    bool   ap_mode = true;               ///< Start in AP mode on first boot
    bool   has_connected = false;        ///< Has successfully connected to WiFi
    
    // ========================================================================
    // COOP CONTROLLER SPECIFIC SETTINGS
    // ========================================================================

    float  temp_threshold_on_f = 34.0;     ///< Temperature threshold to turn ON pump (°F)
    float  temp_threshold_off_f = 36.0;    ///< Temperature threshold to turn OFF pump (°F)
    unsigned int pump_on_time_seconds = 300;    ///< Pump ON time in automatic cycling (seconds)
    unsigned int pump_off_time_seconds = 600;   ///< Pump OFF time in automatic cycling (seconds)
    bool   pump_auto_mode = true;          ///< Enable automatic pump control based on temperature
    bool   light_auto_mode = false;        ///< Enable automatic light control
    int    light_on_minute = 0;            ///< Minute to turn on light (0-59)
    String light_on_mode = "fixed";        ///< Light on mode: 'fixed' or 'sunset_offset'
    int    light_on_sunset_offset_minutes = 0; ///< Minutes before/after sunset for light on
    int    light_on_hour = 6;              ///< Hour to turn on light (24-hour format)
    int    light_off_hour = 21;            ///< Hour to turn off light (24-hour format)
    int    light_brightness_percent = 80;  ///< Maximum light brightness percentage (0-100)
    int    light_transition_duration_minutes = 15; ///< Light fade transition duration (minutes)
    int    water_flow_error_timeout_seconds = 120; ///< Water flow error detection timeout (seconds)
    String log_level = DEFAULT_LOGLEVEL;   ///< Log level: "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR"
    
    // ========================================================================
    // WATER METER CALIBRATION
    // ========================================================================

    float  pulses_per_gallon = 450.0;       ///< Pulses per gallon for water meter calibration
    unsigned int water_meter_timeout_seconds = 300; ///< Timeout before water meter considered disconnected

    // ========================================================================
    // WIFI CONNECTION SETTINGS
    // ========================================================================

    unsigned int wifi_max_retries = 5;      ///< Maximum WiFi connection retry attempts
    unsigned int wifi_retry_delay_seconds = 30;  ///< Delay between WiFi retry attempts (seconds)
    int    wifi_ap_duration_minutes = 10;   ///< How long to stay in AP mode before retrying
    int    watchdog_timeout_seconds = 30;   ///< Watchdog timeout in seconds (range 10-120)
    bool   wifi_led_enabled = true;         ///< Enable WiFi status LED

    // ========================================================================
    // BUZZER SETTINGS
    // ========================================================================

    bool   buzzer_enabled = true;           ///< Enable buzzer alerts
    String buzzer_type = "ACTIVE";          ///< Buzzer type: "ACTIVE" or "PASSIVE"

    // ========================================================================
    // DOOR CONTROL SETTINGS
    // ========================================================================

    bool   door_auto_mode = false;          ///< Enable automatic door control
    int    door_open_timeout_seconds = 30;  ///< Door open timeout (seconds)
    int    door_close_timeout_seconds = 30; ///< Door close timeout (seconds)
    int    sunrise_offset_minutes = 0;      ///< Sunrise offset for door opening (minutes)
    int    sunset_offset_minutes = 0;       ///< Sunset offset for door closing (minutes)

    // ========================================================================
    // LOCATION SETTINGS
    // ========================================================================

    float  latitude = (float)40.7128;       ///< Latitude for sunrise/sunset (default: NYC)
    float  longitude = (float)-74.0060;      ///< Longitude for sunrise/sunset (default: NYC)
    int    timezone_offset_hours = -5;      ///< UTC timezone offset in hours (default: EST)

    // ========================================================================
    // FUTURE FEATURES (Task 3.5k)
    // ========================================================================

    bool   door_auto_close_after_sunset_enabled = false; ///< Auto-close after sunset
    int    door_auto_close_after_sunset_minutes = 0;     ///< Minutes after sunset to close

    // ========================================================================
    // WATER METER PER-PULSE CALCULATION
    // ========================================================================

    bool   water_meter_per_pulse_calculation_enabled = false; ///< Enable per-pulse flow calc

    // ========================================================================
    // PUMP OFF FLOW MONITORING
    // ========================================================================

    bool   pump_off_flow_monitoring_enabled = false; ///< Enable pump OFF flow monitoring
    int    pump_off_flow_grace_period_seconds = 30;  ///< Grace period after pump turns off
};

/**
 * @brief Settings manager singleton
 *
 * Manages persistent storage and retrieval of all system settings.
 * Uses JSON format stored in LittleFS for non-volatile storage.
 *
 * Features:
 * - Singleton pattern for global access
 * - Automatic loading on first access
 * - Deferred restart for WiFi setting changes
 * - JSON import/export
 * - Factory reset capability
 * - Test support methods
 *
 * Usage:
 *   settingsManager.begin(&hal);
 *   settingsManager.load();
 *   float threshold = settingsManager.getTempThresholdOnF();
 *   settingsManager.setTempThresholdOnF(35.0);
 *   settingsManager.save();
 */
class SettingsManager // NOSONAR
{
   private:
    IHAL*         _hal;          ///< Hardware abstraction layer for filesystem access
    user_settings settings;      ///< Current settings values
    bool          isLoaded;      ///< Settings loaded from file flag
    bool          wifiChanged;   ///< WiFi settings changed flag (triggers restart)

    /**
     * @brief Private constructor for singleton
     */
    SettingsManager();

    // Delete copy constructor and assignment operator (singleton)
    SettingsManager(const SettingsManager &)            = delete;
    SettingsManager &operator=(const SettingsManager &) = delete;

    /**
     * @brief Load settings from file
     *
     * @return File contents as string
     */
    String loadFile();

   public:
    void begin(IHAL* hal);
    static SettingsManager &getInstance();

    // After saving wifi settings, requests restart, TODO: maybe use an event instead of this
    unsigned long requestRestartAt;

    bool load();
    bool save();
    void printSettingsDebug() const;

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
    unsigned int getPumpOnTimeSeconds();
    int    getLightOnMinute() const;
    void   setLightOnMinute(int minute);
    String getLightOnMode() const;
    void   setLightOnMode(const String& mode);
    int    getLightOnSunsetOffsetMinutes() const;
    void   setLightOnSunsetOffsetMinutes(int minutes);
    unsigned int getPumpOffTimeSeconds();
    bool   getLightAutoMode() const;
    int    getLightOnHour();
    int    getLightOffHour();
    int    getLightBrightnessPercent() const;
    int    getLightTransitionDurationMinutes() const;
    String getLogLevel() const;
    
    // Water meter calibration getter
    float  getPulsesPerGallon() const;
    
    // Water meter timeout getter
    unsigned int getWaterMeterTimeoutSeconds() const;
    
    // Water meter per-pulse calculation getter
    bool   getWaterMeterPerPulseCalculationEnabled() const;
    
    // Pump off flow monitoring getters
    bool   getPumpOffFlowMonitoringEnabled() const;
    int    getPumpOffFlowGracePeriodSeconds() const;
    
    // WiFi connection settings getters
    unsigned int getWifiMaxRetries();
    unsigned int getWifiRetryDelaySeconds();
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
    
    // Water meter per-pulse calculation setter
    void setWaterMeterPerPulseCalculationEnabled(bool enabled);
    
    // Pump off flow monitoring setters
    void setPumpOffFlowMonitoringEnabled(bool enabled);
    void setPumpOffFlowGracePeriodSeconds(int seconds);

    void factoryReset();
    void setFromJsonDoc(const JsonDocument &doc);
    String toJson(bool includePassword = true) const;
    JsonDocument toJsonDoc(bool includePassword = true) const;

#ifdef UNIT_TEST_DESKTOP
    // Test-only method to reset internal state
    void resetForTesting() {
        settings = user_settings{};
        isLoaded = true; // Mark as loaded so getters don't trigger unwanted loads
        wifiChanged = false;
        requestRestartAt = 0;
    }

    // Test-only method to mark settings as not loaded (for testing load() itself)
    void markAsNotLoadedForTesting() {
        isLoaded = false;
    }
#endif

};

#define settingsManager SettingsManager::getInstance()

#endif