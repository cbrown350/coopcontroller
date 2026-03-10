#ifndef SETTINGS_DATA_H
#define SETTINGS_DATA_H

#include "config.h"

#include "IHAL.h"

#include <stdint.h>
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
    String wifi_bssid_preference = "";       ///< Preferred WiFi BSSID (empty = auto-select)
    String hostname = TOSTRING(HOST_NAME);   ///< Device hostname for mDNS/AP (default from build flag)

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
    // DOOR ADVANCED FEATURES
    // ========================================================================

    bool   door_auto_close_after_sunset_enabled = false; ///< Auto-close after sunset
    int    door_auto_close_after_sunset_minutes = 0;     ///< Minutes after sunset to close
    bool   door_lockout_enabled = false;                 ///< Prevent all door operations when true
    bool   door_timeout_auto_calc_enabled = false;       ///< Auto-calculate door timeouts from history

    // ========================================================================
    // WATER METER PER-PULSE CALCULATION
    // ========================================================================

    bool   water_meter_per_pulse_calculation_enabled = false; ///< Enable per-pulse flow calc

    // ========================================================================
    // PUMP OFF FLOW MONITORING
    // ========================================================================

    bool   pump_off_flow_monitoring_enabled = false; ///< Enable pump OFF flow monitoring
    int    pump_off_flow_grace_period_seconds = 30;  ///< Grace period after pump turns off
    unsigned int pump_off_flow_pulse_threshold = 5;   ///< Min pulses to trigger leak alert (avoids false alarms)

    // ========================================================================
    // PUMP MINIMUM DAILY CYCLES
    // ========================================================================

    bool         pump_min_daily_cycles_enabled = false;  ///< Enable minimum daily pump cycles
    unsigned int pump_min_daily_cycles = 3;              ///< Minimum cycles per 24 hours
    unsigned int pump_min_cycle_run_seconds = 120;       ///< Duration of each scheduled cycle

    // ========================================================================
    // SYSLOG CONFIGURATION
    // ========================================================================

    String syslog_server = syslogServer;              ///< Syslog server address (empty = use compile-time default)
    uint16_t    syslog_port = (uint16_t)atoi(syslogPort);   ///< Syslog server port (default: 514)

    // ========================================================================
    // FLOW CALCULATION INTERVAL
    // ========================================================================

    unsigned int flow_calculation_interval_seconds = 60; ///< Flow rate calculation interval (seconds, default: 60)

    // ========================================================================
    // API AUTHENTICATION
    // ========================================================================

    bool   api_auth_enabled = false;        ///< Enable/disable API authentication
    String api_username = "admin";          ///< API authentication username
    String api_password = "";               ///< API authentication password (empty = no auth)

    // ========================================================================
    // HISTORICAL DATA COLLECTION
    // ========================================================================

    bool         history_enabled = true;             ///< Enable historical data collection
    unsigned int history_temp_min_interval_seconds = 60; ///< Min interval for temp recordings (default: 60s)
    unsigned int history_flow_min_interval_seconds = 10; ///< Min interval for flow recordings (default: 10s)
    unsigned int history_buffer_size = HISTORY_DEFAULT_BUFFER_SIZE;      ///< Buffer size (default: 200, max: 500)

    // ========================================================================
    // OTA UPDATE SETTINGS
    // ========================================================================

    bool         auto_update_enabled = false;        ///< Enable automatic update checks
    unsigned int update_check_interval_hours = 24;   ///< Check interval in hours (1-168)
    String       manifest_url = "";                  ///< URL to version_manifest.json

    // ========================================================================
    // NOTIFICATION SETTINGS
    // ========================================================================

    // Telegram
    bool   telegram_enabled = false;               ///< Enable Telegram notifications
    String telegram_bot_token = "";                ///< Telegram Bot API token
    String telegram_chat_id = "";                  ///< Telegram chat ID for notifications
    unsigned int telegram_polling_interval_seconds = 20; ///< Bot command polling interval (10-300s)

    // Email
    bool     email_enabled = false;                ///< Enable email notifications
    String   email_smtp_server = "";               ///< SMTP server URL or HTTP email API endpoint
    uint16_t email_smtp_port = 587;                ///< SMTP port (default: 587 for TLS)
    String   email_smtp_username = "";             ///< SMTP username / API key
    String   email_smtp_password = "";             ///< SMTP password / API secret
    String   email_from = "";                      ///< From email address
    String   email_to = "";                        ///< Recipient email address

    // Notification preferences (which alerts trigger notifications)
    bool   notify_pump_error = true;               ///< Notify on pump/flow errors
    bool   notify_sensor_error = true;             ///< Notify on sensor failures
    bool   notify_door_fault = true;               ///< Notify on door faults
    bool   notify_wifi_disconnect = false;         ///< Notify on WiFi disconnect
    bool   notify_system_error = true;             ///< Notify on system errors
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
    unsigned int getPumpOffFlowPulseThreshold() const;
    void setPumpOffFlowPulseThreshold(unsigned int threshold);

    // Pump minimum daily cycles getters
    bool         getPumpMinDailyCyclesEnabled() const;
    unsigned int getPumpMinDailyCycles() const;
    unsigned int getPumpMinCycleRunSeconds() const;

    // WiFi connection settings getters
    unsigned int getWifiMaxRetries();
    unsigned int getWifiRetryDelaySeconds();
    int    getWifiAPDurationMinutes();
    int    getWatchdogTimeoutSeconds();
    bool   getWifiLedEnabled() const;
    String getWifiBssidPreference() const;
    String getHostname() const;
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
    
    // Door advanced features getters
    bool getDoorAutoCloseAfterSunsetEnabled() const;
    int getDoorAutoCloseAfterSunsetMinutes() const;
    bool getDoorLockoutEnabled() const;
    bool getDoorTimeoutAutoCalcEnabled() const;

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
    void setWifiBssidPreference(const String& bssid);
    void setHostname(const String& hostname);
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
    
    // Door advanced features setters
    void setDoorAutoCloseAfterSunsetEnabled(bool enabled);
    void setDoorAutoCloseAfterSunsetMinutes(int minutes);
    void setDoorLockoutEnabled(bool enabled);
    void setDoorTimeoutAutoCalcEnabled(bool enabled);
    
    // Water meter per-pulse calculation setter
    void setWaterMeterPerPulseCalculationEnabled(bool enabled);
    
    // Pump off flow monitoring setters
    void setPumpOffFlowMonitoringEnabled(bool enabled);
    void setPumpOffFlowGracePeriodSeconds(int seconds);

    // Pump minimum daily cycles setters
    void setPumpMinDailyCyclesEnabled(bool enabled);
    void setPumpMinDailyCycles(unsigned int cycles);
    void setPumpMinCycleRunSeconds(unsigned int seconds);

    // Syslog configuration getters
    String getSyslogServer() const;
    int    getSyslogPort() const;

    // Syslog configuration setters
    void setSyslogServer(const String& server);
    void setSyslogPort(int port);

    // Flow calculation interval getters/setters
    unsigned int getFlowCalculationIntervalSeconds() const;
    void setFlowCalculationIntervalSeconds(unsigned int seconds);

    // API Authentication getters
    bool   getApiAuthEnabled() const;
    String getApiUsername() const;
    String getApiPassword() const;

    // API Authentication setters
    void setApiAuthEnabled(bool enabled);
    void setApiUsername(const String& username);
    void setApiPassword(const String& password);

    // Historical data collection getters
    bool         getHistoryEnabled() const;
    unsigned int getHistoryTempMinIntervalSeconds() const;
    unsigned int getHistoryFlowMinIntervalSeconds() const;
    unsigned int getHistoryBufferSize() const;

    // Historical data collection setters
    void setHistoryEnabled(bool enabled);
    void setHistoryTempMinIntervalSeconds(unsigned int seconds);
    void setHistoryFlowMinIntervalSeconds(unsigned int seconds);
    void setHistoryBufferSize(unsigned int size);

    // OTA update settings getters
    bool         getAutoUpdateEnabled() const;
    unsigned int getUpdateCheckIntervalHours() const;
    String       getManifestUrl() const;

    // OTA update settings setters
    void setAutoUpdateEnabled(bool enabled);
    void setUpdateCheckIntervalHours(unsigned int hours);
    void setManifestUrl(const String& url);

    // Telegram notification getters
    bool   getTelegramEnabled() const;
    String getTelegramBotToken() const;
    String getTelegramChatId() const;
    unsigned int getTelegramPollingIntervalSeconds() const;

    // Telegram notification setters
    void setTelegramEnabled(bool enabled);
    void setTelegramBotToken(const String& token);
    void setTelegramChatId(const String& chatId);
    void setTelegramPollingIntervalSeconds(unsigned int seconds);

    // Email notification getters
    bool     getEmailEnabled() const;
    String   getEmailSmtpServer() const;
    uint16_t getEmailSmtpPort() const;
    String   getEmailSmtpUsername() const;
    String   getEmailSmtpPassword() const;
    String   getEmailFrom() const;
    String   getEmailTo() const;

    // Email notification setters
    void setEmailEnabled(bool enabled);
    void setEmailSmtpServer(const String& server);
    void setEmailSmtpPort(uint16_t port);
    void setEmailSmtpUsername(const String& username);
    void setEmailSmtpPassword(const String& password);
    void setEmailFrom(const String& from);
    void setEmailTo(const String& to);

    // Notification preference getters
    bool getNotifyPumpError() const;
    bool getNotifySensorError() const;
    bool getNotifyDoorFault() const;
    bool getNotifyWifiDisconnect() const;
    bool getNotifySystemError() const;

    // Notification preference setters
    void setNotifyPumpError(bool enabled);
    void setNotifySensorError(bool enabled);
    void setNotifyDoorFault(bool enabled);
    void setNotifyWifiDisconnect(bool enabled);
    void setNotifySystemError(bool enabled);

    void factoryReset();
    void setFromJsonDoc(const JsonDocument &doc);
    String toJson(bool includePassword = true) const;
    JsonDocument toJsonDoc(bool includePassword = true) const;

    /**
     * @brief Backup current settings to NVS
     *
     * Serializes all settings to JSON and stores in NVS partition.
     * NVS survives LittleFS/SPIFFS partition flashing during OTA updates.
     * Call this before flashing a new filesystem image.
     *
     * @return true if backup successful
     */
    bool backupToNVS();

    /**
     * @brief Restore settings from NVS backup
     *
     * Reads settings JSON from NVS, applies to current settings,
     * saves to LittleFS file, and clears the NVS backup.
     * Called automatically during begin() if a backup exists.
     *
     * @return true if restore successful
     */
    bool restoreFromNVS();

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