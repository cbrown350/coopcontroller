#include "config.h"
#include "IHAL.h"
#include "SettingsManager.h"
#include "Logger.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cassert>


SettingsManager::SettingsManager() : _hal(nullptr), isLoaded(false), wifiChanged(false), requestRestartAt(0) {
    // Initialize with default values
    settings = user_settings{};
}

void SettingsManager::begin(IHAL* hal)
{
  _hal = hal;
  if(!_hal->fsBegin()) {
      logger.logError("Failed to initialize filesystem in SettingsManager::begin");
  }

  // Check for NVS settings backup (from OTA filesystem update)
  restoreFromNVS();
}

SettingsManager &SettingsManager::getInstance() {
    static SettingsManager instance; // NOSONAR - maintain instance here
    return instance;
}

String SettingsManager::loadFile() {
    if (_hal == nullptr) {
        logger.logError("IHAL pointer not initialized - call SettingsManager::begin(&hal) first");
        return "";
    }

    auto file = _hal->fsOpen(SETTINGS_FILE, "r");
    if (!file) {
        logger.logWarning("Settings file not found, using defaults");
        // Set default values
        settings = user_settings{};
        printSettingsDebug();
        isLoaded = true;
        return "";
    }

    // Get file size
    size_t fileSize = _hal->fsSize(file);
    
    // Read file content into buffer
    String content = "";
    if (fileSize > 0) {
        uint8_t* buffer = new uint8_t[fileSize + 1];
        size_t bytesRead = _hal->fsRead(file, buffer, fileSize);
        buffer[bytesRead] = '\0'; // Null-terminate
        content = String((char*)buffer);
        delete[] buffer;
    }
    
    _hal->fsClose(file);
    return content;
}

bool SettingsManager::load() {
    if (isLoaded) {
        return true;
    }

    String content = loadFile();
    if (content.length() == 0) {
        return false; // already logged in loadFile
    }

    JsonDocument doc;   
    
    if (DeserializationError error = deserializeJson(doc, content); error) {
        logger.logError("Settings JSON parsing error, using defaults");
        settings = user_settings{};
        isLoaded = true;
        return false;
    }

    setFromJsonDoc(doc);
    printSettingsDebug();

    isLoaded = true;
    logger.logDebug("Settings loaded successfully");
    return true;
}

bool SettingsManager::save() {
    String content = loadFile(); 
    JsonDocument oldDoc;
    if (DeserializationError error = deserializeJson(oldDoc, content); !error) {
        oldDoc["passwd"] = oldDoc["passwd"] | ""; // ensure passwd key exists for comparison
        if (!settings.ssid.equals((String)oldDoc["ssid"]) ||
            !settings.passwd.equals((String)oldDoc["passwd"])) {
            logger.logWarning("WiFi SSID or password changed, AP mode will be disabled");
            wifiChanged = true;
            settings.ap_mode = false;
        }   
        if (oldDoc["ap_mode"] != settings.ap_mode) {
            logger.logWarning("WiFi AP mode setting changed");
            wifiChanged = true;
        } 
    } else {
        logger.logWarning("Could not parse existing settings JSON file, assuming WiFi settings changed");
        wifiChanged = true;
    }

    auto file = _hal->fsOpen(SETTINGS_FILE, "w");
    if (!file) {
        logger.logError("Failed to open settings file for writing");
        return false;
    }

    JsonDocument doc = toJsonDoc(true);
    String output;
    serializeJson(doc, output);
    
    size_t bytesWritten = _hal->fsWrite(file, (const uint8_t*)output.c_str(), output.length());
    if (bytesWritten != output.length()) {
        logger.logError("Failed to write settings to file");
        _hal->fsClose(file);
        return false;
    }

    _hal->fsClose(file);
    printSettingsDebug();
    logger.logDebug("Settings saved successfully");
    return true;
}

const user_settings &SettingsManager::getSettings() {
    if (!isLoaded) {
        load();
    }
    return settings;
}

void SettingsManager::printSettingsDebug() const {
    JsonDocument doc = toJsonDoc(false);
    logger.logDebug("Current Settings:");
    for (JsonPair kv : doc.as<JsonObject>()) {
        logger.logDebug("  " + String(kv.key().c_str()) + ": " + String(kv.value().as<String>()));
    }
}

// WiFi getters
String SettingsManager::getSSID() {
    return getSettings().ssid;
}

String SettingsManager::getPassword() {
    return getSettings().passwd;
}

bool SettingsManager::isAPMode() {
    return getSettings().ap_mode;
}

bool SettingsManager::getPumpAutoMode() const {
    return settings.pump_auto_mode;
}

bool SettingsManager::getHasConnected() {
    return getSettings().has_connected;
}

// Coop Controller getters
float SettingsManager::getTempThresholdOnF() {
    return getSettings().temp_threshold_on_f;
}

float SettingsManager::getTempThresholdOffF() {
    return getSettings().temp_threshold_off_f;
}

int SettingsManager::getWaterFlowErrorTimeoutSeconds() {
    return getSettings().water_flow_error_timeout_seconds;
}

int SettingsManager::getPumpErrorRetrySeconds() {
    return getSettings().pump_on_time_seconds; // Using pump_on_time as retry time
}

unsigned int SettingsManager::getPumpOnTimeSeconds() {
    return getSettings().pump_on_time_seconds;
}

unsigned int SettingsManager::getPumpOffTimeSeconds() {
    return getSettings().pump_off_time_seconds;
}

bool SettingsManager::getLightAutoMode() const {
    return settings.light_auto_mode;
}

int SettingsManager::getLightOnHour() {
    return getSettings().light_on_hour;
}

int SettingsManager::getLightOffHour() {
    return getSettings().light_off_hour;
}

int SettingsManager::getLightBrightnessPercent() const {
    return settings.light_brightness_percent;
}

int SettingsManager::getLightTransitionDurationMinutes() const {
    return settings.light_transition_duration_minutes;
}

String SettingsManager::getLogLevel() const {
    return settings.log_level;
}

// Water meter calibration getters
float SettingsManager::getPulsesPerGallon() const {
    return settings.pulses_per_gallon;
}

// Water meter timeout getters
unsigned int SettingsManager::getWaterMeterTimeoutSeconds() const {
    return settings.water_meter_timeout_seconds;
}

// Water meter per-pulse calculation getter
bool SettingsManager::getWaterMeterPerPulseCalculationEnabled() const {
    return settings.water_meter_per_pulse_calculation_enabled;
}

// Pump off flow monitoring getters
bool SettingsManager::getPumpOffFlowMonitoringEnabled() const {
    return settings.pump_off_flow_monitoring_enabled;
}

int SettingsManager::getPumpOffFlowGracePeriodSeconds() const {
    return settings.pump_off_flow_grace_period_seconds;
}

unsigned int SettingsManager::getPumpOffFlowPulseThreshold() const {
    return settings.pump_off_flow_pulse_threshold;
}

// Pump minimum daily cycles getters
bool SettingsManager::getPumpMinDailyCyclesEnabled() const {
    return settings.pump_min_daily_cycles_enabled;
}

unsigned int SettingsManager::getPumpMinDailyCycles() const {
    return settings.pump_min_daily_cycles;
}

unsigned int SettingsManager::getPumpMinCycleRunSeconds() const {
    return settings.pump_min_cycle_run_seconds;
}

// API Authentication getters
bool SettingsManager::getApiAuthEnabled() const {
    return settings.api_auth_enabled;
}

String SettingsManager::getApiUsername() const {
    return settings.api_username;
}

String SettingsManager::getApiPassword() const {
    return settings.api_password;
}

// WiFi connection settings getters
unsigned int SettingsManager::getWifiMaxRetries() {
    return getSettings().wifi_max_retries;
}

unsigned int SettingsManager::getWifiRetryDelaySeconds() {
    return getSettings().wifi_retry_delay_seconds;
}

int SettingsManager::getWifiAPDurationMinutes() {
    return getSettings().wifi_ap_duration_minutes;
}

int SettingsManager::getWatchdogTimeoutSeconds() {
    return getSettings().watchdog_timeout_seconds;
}

bool SettingsManager::getWifiLedEnabled() const {
    return settings.wifi_led_enabled;
}

String SettingsManager::getWifiBssidPreference() const {
    return settings.wifi_bssid_preference;
}

String SettingsManager::getHostname() const {
    return settings.hostname;
}

bool SettingsManager::getWifiChanged() const {
    return wifiChanged;
}

// Buzzer settings getters
bool SettingsManager::getBuzzerEnabled() const {
    return settings.buzzer_enabled;
}

String SettingsManager::getBuzzerType() const {
    return settings.buzzer_type;
}

// Door control settings getters
bool SettingsManager::getDoorAutoMode() const {
    return settings.door_auto_mode;
}

int SettingsManager::getDoorOpenTimeoutSeconds() const {
    return settings.door_open_timeout_seconds;
}

int SettingsManager::getDoorCloseTimeoutSeconds() const {
    return settings.door_close_timeout_seconds;
}

int SettingsManager::getSunriseOffsetMinutes() const {
    return settings.sunrise_offset_minutes;
}

int SettingsManager::getSunsetOffsetMinutes() const {
    return settings.sunset_offset_minutes;
}

// Location settings getters
float SettingsManager::getLatitude() const {
    return settings.latitude;
}

float SettingsManager::getLongitude() const {
    return settings.longitude;
}

int SettingsManager::getTimezoneOffsetHours() const {
    return settings.timezone_offset_hours;
}

String SettingsManager::getTimezonePosix() const {
    return settings.timezone_posix;
}

// Door advanced features getters
bool SettingsManager::getDoorAutoCloseAfterSunsetEnabled() const {
    return settings.door_auto_close_after_sunset_enabled;
}

int SettingsManager::getDoorAutoCloseAfterSunsetMinutes() const {
    return settings.door_auto_close_after_sunset_minutes;
}

bool SettingsManager::getDoorLockoutEnabled() const {
    return settings.door_lockout_enabled;
}

bool SettingsManager::getDoorTimeoutAutoCalcEnabled() const {
    return settings.door_timeout_auto_calc_enabled;
}

// WiFi setters
void SettingsManager::setSSID(const String &ssid) {
    // wifiChanged updated elsewhere
    settings.ssid = ssid;
}

void SettingsManager::setPassword(const String &password) {
    // wifiChanged updated elsewhere
    settings.passwd = password;
}

void SettingsManager::setAPMode(bool apMode) {
    // wifiChanged updated elsewhere
    settings.ap_mode = apMode;
}

void SettingsManager::setPumpAutoMode(bool mode) {
    settings.pump_auto_mode = mode;
}

void SettingsManager::setHasConnected(bool hasConnected) {
    settings.has_connected = hasConnected;
}

// Coop Controller setters
void SettingsManager::setTempThresholdOnF(float threshold) {
    settings.temp_threshold_on_f = threshold;
}

void SettingsManager::setTempThresholdOffF(float threshold) {
    settings.temp_threshold_off_f = threshold;
}

void SettingsManager::setWaterFlowErrorTimeoutSeconds(int timeout) {
    settings.water_flow_error_timeout_seconds = timeout;
}

void SettingsManager::setPumpErrorRetrySeconds(int seconds) {
    settings.pump_on_time_seconds = seconds; // Using pump_on_time as retry time
}

void SettingsManager::setPumpOnTimeSeconds(int seconds) {
    settings.pump_on_time_seconds = seconds;
}

void SettingsManager::setPumpOffTimeSeconds(int seconds) {
    settings.pump_off_time_seconds = seconds;
}

void SettingsManager::setLightAutoMode(bool mode) {
    settings.light_auto_mode = mode;
}

void SettingsManager::setLightOnHour(int hour) {
    settings.light_on_hour = hour;
}

void SettingsManager::setLightOffHour(int hour) {
    settings.light_off_hour = hour;
}

void SettingsManager::setLightBrightnessPercent(int percent) {
    settings.light_brightness_percent = constrain(percent, 0, 100);
}

void SettingsManager::setLightTransitionDurationMinutes(int minutes) {
    settings.light_transition_duration_minutes = constrain(minutes, 1, 60);
}

void SettingsManager::setLogLevel(const String& level) {
    settings.log_level = level;
}

// Water meter calibration setters
void SettingsManager::setPulsesPerGallon(float value) {
    settings.pulses_per_gallon = value;
}

// Water meter timeout setters
void SettingsManager::setWaterMeterTimeoutSeconds(int seconds) {
    settings.water_meter_timeout_seconds = seconds;
}

// Water meter per-pulse calculation setter
void SettingsManager::setWaterMeterPerPulseCalculationEnabled(bool enabled) {
    settings.water_meter_per_pulse_calculation_enabled = enabled;
}

// Pump off flow monitoring setters
void SettingsManager::setPumpOffFlowMonitoringEnabled(bool enabled) {
    settings.pump_off_flow_monitoring_enabled = enabled;
}

void SettingsManager::setPumpOffFlowGracePeriodSeconds(int seconds) {
    settings.pump_off_flow_grace_period_seconds = seconds;
}

void SettingsManager::setPumpOffFlowPulseThreshold(unsigned int threshold) {
    settings.pump_off_flow_pulse_threshold = threshold;
}

// Pump minimum daily cycles setters
void SettingsManager::setPumpMinDailyCyclesEnabled(bool enabled) {
    settings.pump_min_daily_cycles_enabled = enabled;
}

void SettingsManager::setPumpMinDailyCycles(unsigned int cycles) {
    settings.pump_min_daily_cycles = constrain(cycles, 1, 12);
}

void SettingsManager::setPumpMinCycleRunSeconds(unsigned int seconds) {
    settings.pump_min_cycle_run_seconds = constrain(seconds, 30, 600);
}

// API Authentication setters
void SettingsManager::setApiAuthEnabled(bool enabled) {
    settings.api_auth_enabled = enabled;
}

void SettingsManager::setApiUsername(const String& username) {
    settings.api_username = username;
}

void SettingsManager::setApiPassword(const String& password) {
    settings.api_password = password;
}

// Historical data collection getters
bool SettingsManager::getHistoryEnabled() const {
    return settings.history_enabled;
}

unsigned int SettingsManager::getHistoryTempMinIntervalSeconds() const {
    return settings.history_temp_min_interval_seconds;
}

unsigned int SettingsManager::getHistoryFlowMinIntervalSeconds() const {
    return settings.history_flow_min_interval_seconds;
}

unsigned int SettingsManager::getHistoryBufferSize() const {
    return settings.history_buffer_size;
}

// Historical data collection setters
void SettingsManager::setHistoryEnabled(bool enabled) {
    settings.history_enabled = enabled;
}

void SettingsManager::setHistoryTempMinIntervalSeconds(unsigned int seconds) {
    settings.history_temp_min_interval_seconds = constrain(seconds, 10, 3600); // 10s to 1 hour
}

void SettingsManager::setHistoryFlowMinIntervalSeconds(unsigned int seconds) {
    settings.history_flow_min_interval_seconds = constrain(seconds, 5, 300); // 5s to 5 minutes
}

void SettingsManager::setHistoryBufferSize(unsigned int size) {
    settings.history_buffer_size = constrain(size, 60, 10080);
}

// Syslog configuration getters
String SettingsManager::getSyslogServer() const {
    return settings.syslog_server;
}

int SettingsManager::getSyslogPort() const {
    return settings.syslog_port;
}

// Syslog configuration setters
void SettingsManager::setSyslogServer(const String& server) {
    settings.syslog_server = server;
}

void SettingsManager::setSyslogPort(int port) {
    settings.syslog_port = constrain(port, 1, 65535);
}

// Flow calculation interval getters/setters
unsigned int SettingsManager::getFlowCalculationIntervalSeconds() const {
    return settings.flow_calculation_interval_seconds;
}

void SettingsManager::setFlowCalculationIntervalSeconds(unsigned int seconds) {
    settings.flow_calculation_interval_seconds = constrain(seconds, 5, 300);
}

// WiFi connection settings setters - request restart for these
void SettingsManager::setWifiMaxRetries(int retries) {
    settings.wifi_max_retries = retries;
}

void SettingsManager::setWifiRetryDelaySeconds(int seconds) {
    settings.wifi_retry_delay_seconds = seconds;
}

void SettingsManager::setWifiAPDurationMinutes(int minutes) {
    settings.wifi_ap_duration_minutes = minutes;
}

void SettingsManager::setWatchdogTimeoutSeconds(int seconds) {
    settings.watchdog_timeout_seconds = seconds;
}

void SettingsManager::setWifiLedEnabled(bool enabled) {
    settings.wifi_led_enabled = enabled;
}

void SettingsManager::setWifiBssidPreference(const String& bssid) {
    settings.wifi_bssid_preference = bssid;
}

void SettingsManager::setHostname(const String& hostname) {
    settings.hostname = hostname;
}

void SettingsManager::setWifiChanged(bool changed) {
    wifiChanged = changed;
}

// Buzzer settings setters
void SettingsManager::setBuzzerEnabled(bool enabled) {
    settings.buzzer_enabled = enabled;
}

void SettingsManager::setBuzzerType(const String& type) {
    settings.buzzer_type = type;
}

// Door control settings setters
void SettingsManager::setDoorAutoMode(bool enabled) {
    settings.door_auto_mode = enabled;
}

void SettingsManager::setDoorOpenTimeoutSeconds(int seconds) {
    settings.door_open_timeout_seconds = seconds;
}

void SettingsManager::setDoorCloseTimeoutSeconds(int seconds) {
    settings.door_close_timeout_seconds = seconds;
}

void SettingsManager::setSunriseOffsetMinutes(int minutes) {
    settings.sunrise_offset_minutes = minutes;
}

void SettingsManager::setSunsetOffsetMinutes(int minutes) {
    settings.sunset_offset_minutes = minutes;
}

// Location settings setters
void SettingsManager::setLatitude(float latitude) {
    settings.latitude = latitude;
}

void SettingsManager::setLongitude(float longitude) {
    settings.longitude = longitude;
}

void SettingsManager::setTimezoneOffsetHours(int offset) {
    settings.timezone_offset_hours = offset;
}

void SettingsManager::setTimezonePosix(const String& tz) {
    settings.timezone_posix = tz;
}

// Door advanced features setters
void SettingsManager::setDoorAutoCloseAfterSunsetEnabled(bool enabled) {
    settings.door_auto_close_after_sunset_enabled = enabled;
}

void SettingsManager::setDoorAutoCloseAfterSunsetMinutes(int minutes) {
    settings.door_auto_close_after_sunset_minutes = minutes;
}

void SettingsManager::setDoorLockoutEnabled(bool enabled) {
    settings.door_lockout_enabled = enabled;
}

void SettingsManager::setDoorTimeoutAutoCalcEnabled(bool enabled) {
    settings.door_timeout_auto_calc_enabled = enabled;
}

bool SettingsManager::backupToNVS() {
    if (_hal == nullptr) {
        logger.logError("Cannot backup to NVS: HAL not initialized");
        return false;
    }

    String json = toJson(true);  // Include passwords in backup
    if (json.length() == 0) {
        logger.logError("Cannot backup to NVS: empty settings JSON");
        return false;
    }

    bool result = _hal->nvsWriteString(NVS_SETTINGS_NAMESPACE, NVS_SETTINGS_KEY, json);
    if (result) {
        logger.logInfo("Settings backed up to NVS (" + String(json.length()) + " bytes)");
    } else {
        logger.logError("Failed to backup settings to NVS");
    }
    return result;
}

bool SettingsManager::restoreFromNVS() {
    if (_hal == nullptr) {
        logger.logError("Cannot restore from NVS: HAL not initialized");
        return false;
    }

    String json = _hal->nvsReadString(NVS_SETTINGS_NAMESPACE, NVS_SETTINGS_KEY);
    if (json.length() == 0) {
        return false;  // No backup found, not an error
    }

    logger.logInfo("Found NVS settings backup (" + String(json.length()) + " bytes), restoring...");

    JsonDocument doc;
    if (DeserializationError error = deserializeJson(doc, json); error) {
        logger.logError("Failed to parse NVS backup JSON: " + String(error.c_str()));
        // Clear corrupt backup
        _hal->nvsRemove(NVS_SETTINGS_NAMESPACE, NVS_SETTINGS_KEY);
        return false;
    }

    setFromJsonDoc(doc);
    isLoaded = true;

    // Write restored settings directly to file, bypassing save() which calls
    // loadFile() and resets settings to defaults when no file exists (the exact
    // scenario after OTA filesystem flash)
    JsonDocument outDoc = toJsonDoc(true);
    String output;
    serializeJson(outDoc, output);

    bool saved = false;
    auto file = _hal->fsOpen(SETTINGS_FILE, "w");
    if (file) {
        size_t bytesWritten = _hal->fsWrite(file, (const uint8_t*)output.c_str(), output.length());
        saved = (bytesWritten == output.length());
        _hal->fsClose(file);
    }

    if (saved) {
        // Only clear NVS backup after successful filesystem write
        _hal->nvsRemove(NVS_SETTINGS_NAMESPACE, NVS_SETTINGS_KEY);
        logger.logInfo("Settings restored from NVS backup and saved to filesystem");
    } else {
        logger.logWarning("Settings restored from NVS but failed to save to filesystem - NVS backup retained for next boot");
    }

    return true;
}

void SettingsManager::factoryReset() {
    logger.logWarning("Factory reset initiated - clearing all settings");
    
    // Create default settings
    user_settings newSettings{};
    
    // Apply new settings
    settings = newSettings;
    isLoaded = true;
    wifiChanged = true;
    
    // Save to file
    save();
    
    logger.logWarning("Factory reset completed");
}

void SettingsManager::setFromJsonDoc(const JsonDocument &doc) {

    user_settings defaultSettings{};
    
    // Load WiFi settings
    settings.ssid = doc["ssid"] | defaultSettings.ssid;
    settings.passwd = doc["passwd"] | defaultSettings.passwd;
    settings.ap_mode = doc["ap_mode"] | defaultSettings.ap_mode;
    settings.has_connected = doc["has_connected"] | defaultSettings.has_connected;
    
    // Load Coop Controller settings
    settings.temp_threshold_on_f = doc["temp_threshold_on_f"] | defaultSettings.temp_threshold_on_f;
    settings.temp_threshold_off_f = doc["temp_threshold_off_f"] | defaultSettings.temp_threshold_off_f;
    settings.pump_on_time_seconds = doc["pump_on_time_seconds"] | defaultSettings.pump_on_time_seconds;
    settings.pump_off_time_seconds = doc["pump_off_time_seconds"] | defaultSettings.pump_off_time_seconds;
    settings.pump_auto_mode = doc["pump_auto_mode"] | defaultSettings.pump_auto_mode;
    settings.light_auto_mode = doc["light_auto_mode"] | defaultSettings.light_auto_mode;
    settings.light_on_hour = doc["light_on_hour"] | defaultSettings.light_on_hour;
    settings.light_on_minute = doc["light_on_minute"] | defaultSettings.light_on_minute;
    settings.light_on_mode = doc["light_on_mode"] | defaultSettings.light_on_mode;
    settings.light_on_sunset_offset_minutes = doc["light_on_sunset_offset_minutes"] | defaultSettings.light_on_sunset_offset_minutes;
    settings.light_off_hour = doc["light_off_hour"] | defaultSettings.light_off_hour;
    settings.light_brightness_percent = doc["light_brightness_percent"] | defaultSettings.light_brightness_percent;
    settings.light_transition_duration_minutes = doc["light_transition_duration_minutes"] | defaultSettings.light_transition_duration_minutes;
    settings.water_flow_error_timeout_seconds = doc["water_flow_error_timeout_seconds"] | defaultSettings.water_flow_error_timeout_seconds;
    settings.log_level = doc["log_level"] | defaultSettings.log_level;
    
    // Load water meter calibration
    settings.pulses_per_gallon = doc["pulses_per_gallon"] | defaultSettings.pulses_per_gallon;
    settings.water_meter_timeout_seconds = doc["water_meter_timeout_seconds"] | defaultSettings.water_meter_timeout_seconds;
    settings.water_meter_per_pulse_calculation_enabled = doc["water_meter_per_pulse_calculation_enabled"] | defaultSettings.water_meter_per_pulse_calculation_enabled;
    settings.pump_off_flow_monitoring_enabled = doc["pump_off_flow_monitoring_enabled"] | defaultSettings.pump_off_flow_monitoring_enabled;
    settings.pump_off_flow_grace_period_seconds = doc["pump_off_flow_grace_period_seconds"] | defaultSettings.pump_off_flow_grace_period_seconds;
    settings.pump_off_flow_pulse_threshold = doc["pump_off_flow_pulse_threshold"] | defaultSettings.pump_off_flow_pulse_threshold;

    // Load pump minimum daily cycles settings
    settings.pump_min_daily_cycles_enabled = doc["pump_min_daily_cycles_enabled"] | defaultSettings.pump_min_daily_cycles_enabled;
    settings.pump_min_daily_cycles = doc["pump_min_daily_cycles"] | defaultSettings.pump_min_daily_cycles;
    settings.pump_min_cycle_run_seconds = doc["pump_min_cycle_run_seconds"] | defaultSettings.pump_min_cycle_run_seconds;

    // Load WiFi connection settings
    settings.wifi_max_retries = doc["wifi_max_retries"] | defaultSettings.wifi_max_retries;
    settings.wifi_retry_delay_seconds = doc["wifi_retry_delay_seconds"] | defaultSettings.wifi_retry_delay_seconds;
    settings.wifi_ap_duration_minutes = doc["wifi_ap_duration_minutes"] | defaultSettings.wifi_ap_duration_minutes;
    if (settings.wifi_ap_duration_minutes < MIN_AP_TIME) {
        settings.wifi_ap_duration_minutes = MIN_AP_TIME; 
    }
    settings.watchdog_timeout_seconds = doc["watchdog_timeout_seconds"] | defaultSettings.watchdog_timeout_seconds;
    settings.wifi_led_enabled = doc["wifi_led_enabled"] | defaultSettings.wifi_led_enabled;
    if (doc["wifi_bssid_preference"].is<const char*>()) settings.wifi_bssid_preference = doc["wifi_bssid_preference"].as<String>();
    if (doc["hostname"].is<const char*>()) settings.hostname = doc["hostname"].as<String>();

    // Load buzzer settings
    settings.buzzer_enabled = doc["buzzer_enabled"] | defaultSettings.buzzer_enabled;
    settings.buzzer_type = doc["buzzer_type"] | defaultSettings.buzzer_type;
    
    // Load door control settings
    settings.door_auto_mode = doc["door_auto_mode"] | defaultSettings.door_auto_mode;
    settings.door_open_timeout_seconds = doc["door_open_timeout_seconds"] | defaultSettings.door_open_timeout_seconds;
    settings.door_close_timeout_seconds = doc["door_close_timeout_seconds"] | defaultSettings.door_close_timeout_seconds;
    settings.sunrise_offset_minutes = doc["sunrise_offset_minutes"] | defaultSettings.sunrise_offset_minutes;
    settings.sunset_offset_minutes = doc["sunset_offset_minutes"] | defaultSettings.sunset_offset_minutes;
    
    // Load location settings
    settings.latitude = doc["latitude"] | defaultSettings.latitude;
    settings.longitude = doc["longitude"] | defaultSettings.longitude;
    settings.timezone_offset_hours = doc["timezone_offset_hours"] | defaultSettings.timezone_offset_hours;
    if (doc["timezone_posix"].is<const char*>()) {
        settings.timezone_posix = doc["timezone_posix"].as<String>();
    } else {
        settings.timezone_posix = defaultSettings.timezone_posix;
    }

    // Load door advanced features settings
    settings.door_auto_close_after_sunset_enabled = doc["door_auto_close_after_sunset_enabled"] | defaultSettings.door_auto_close_after_sunset_enabled;
    settings.door_auto_close_after_sunset_minutes = doc["door_auto_close_after_sunset_minutes"] | defaultSettings.door_auto_close_after_sunset_minutes;
    settings.door_lockout_enabled = doc["door_lockout_enabled"] | defaultSettings.door_lockout_enabled;
    settings.door_timeout_auto_calc_enabled = doc["door_timeout_auto_calc_enabled"] | defaultSettings.door_timeout_auto_calc_enabled;

    // Load API authentication settings
    settings.api_auth_enabled = doc["api_auth_enabled"] | defaultSettings.api_auth_enabled;
    settings.api_username = doc["api_username"] | defaultSettings.api_username;
    settings.api_password = doc["api_password"] | defaultSettings.api_password;

    // Load syslog configuration
    if (doc["syslog_server"].is<const char*>() && strlen(doc["syslog_server"].as<const char*>()) > 0) settings.syslog_server = doc["syslog_server"].as<String>();
    else settings.syslog_server = defaultSettings.syslog_server;
    settings.syslog_port = doc["syslog_port"] | defaultSettings.syslog_port;

    // Load flow calculation interval
    settings.flow_calculation_interval_seconds = doc["flow_calculation_interval_seconds"] | defaultSettings.flow_calculation_interval_seconds;

    // Load historical data collection settings
    settings.history_enabled = doc["history_enabled"] | defaultSettings.history_enabled;
    settings.history_temp_min_interval_seconds = doc["history_temp_min_interval_seconds"] | defaultSettings.history_temp_min_interval_seconds;
    settings.history_flow_min_interval_seconds = doc["history_flow_min_interval_seconds"] | defaultSettings.history_flow_min_interval_seconds;
    // Backward compat: old history_sample_interval_seconds maps to temp interval
    if (doc["history_sample_interval_seconds"].is<unsigned int>()) {
        settings.history_temp_min_interval_seconds = doc["history_sample_interval_seconds"].as<unsigned int>();
    }
    settings.history_buffer_size = doc["history_buffer_size"] | defaultSettings.history_buffer_size;

    // Load OTA update settings
    settings.auto_update_enabled = doc["auto_update_enabled"] | defaultSettings.auto_update_enabled;
    settings.update_check_interval_hours = doc["update_check_interval_hours"] | defaultSettings.update_check_interval_hours;
    if (doc["manifest_url"].is<const char*>()) settings.manifest_url = doc["manifest_url"].as<String>();

    // Load notification settings - Telegram
    settings.telegram_enabled = doc["telegram_enabled"] | defaultSettings.telegram_enabled;
    if (doc["telegram_bot_token"].is<const char*>()) settings.telegram_bot_token = doc["telegram_bot_token"].as<String>();
    if (doc["telegram_chat_id"].is<const char*>()) settings.telegram_chat_id = doc["telegram_chat_id"].as<String>();
    settings.telegram_polling_interval_seconds = doc["telegram_polling_interval_seconds"] | defaultSettings.telegram_polling_interval_seconds;

    // Load notification settings - Email
    settings.email_enabled = doc["email_enabled"] | defaultSettings.email_enabled;
    if (doc["email_smtp_server"].is<const char*>()) settings.email_smtp_server = doc["email_smtp_server"].as<String>();
    settings.email_smtp_port = doc["email_smtp_port"] | defaultSettings.email_smtp_port;
    if (doc["email_smtp_username"].is<const char*>()) settings.email_smtp_username = doc["email_smtp_username"].as<String>();
    if (doc["email_smtp_password"].is<const char*>()) settings.email_smtp_password = doc["email_smtp_password"].as<String>();
    if (doc["email_from"].is<const char*>()) settings.email_from = doc["email_from"].as<String>();
    if (doc["email_to"].is<const char*>()) settings.email_to = doc["email_to"].as<String>();

    // Load notification preferences
    settings.notify_pump_error = doc["notify_pump_error"] | defaultSettings.notify_pump_error;
    settings.notify_sensor_error = doc["notify_sensor_error"] | defaultSettings.notify_sensor_error;
    settings.notify_door_fault = doc["notify_door_fault"] | defaultSettings.notify_door_fault;
    settings.notify_wifi_disconnect = doc["notify_wifi_disconnect"] | defaultSettings.notify_wifi_disconnect;
    settings.notify_system_error = doc["notify_system_error"] | defaultSettings.notify_system_error;
}

JsonDocument SettingsManager::toJsonDoc(bool includePassword) const {
    JsonDocument doc;
    
    // WiFi settings
    doc["ssid"] = settings.ssid;
    if (includePassword && settings.passwd.length() != 0) {
        doc["passwd"] = settings.passwd;
    }
    doc["ap_mode"] = settings.ap_mode;
    doc["has_connected"] = settings.has_connected;
    
    // Coop Controller settings
    doc["temp_threshold_on_f"] = settings.temp_threshold_on_f;
    doc["temp_threshold_off_f"] = settings.temp_threshold_off_f;
    doc["pump_on_time_seconds"] = settings.pump_on_time_seconds;
    doc["pump_off_time_seconds"] = settings.pump_off_time_seconds;
    doc["pump_auto_mode"] = settings.pump_auto_mode;
    doc["light_auto_mode"] = settings.light_auto_mode;
    doc["light_on_hour"] = settings.light_on_hour;
    doc["light_on_minute"] = settings.light_on_minute;
    doc["light_on_mode"] = settings.light_on_mode;
    doc["light_on_sunset_offset_minutes"] = settings.light_on_sunset_offset_minutes;
    doc["light_off_hour"] = settings.light_off_hour;
    doc["light_brightness_percent"] = settings.light_brightness_percent;
    doc["light_transition_duration_minutes"] = settings.light_transition_duration_minutes;
    doc["water_flow_error_timeout_seconds"] = settings.water_flow_error_timeout_seconds;
    doc["log_level"] = settings.log_level;
    
    // Water meter calibration
    doc["pulses_per_gallon"] = settings.pulses_per_gallon;
    doc["water_meter_timeout_seconds"] = settings.water_meter_timeout_seconds;
    doc["water_meter_per_pulse_calculation_enabled"] = settings.water_meter_per_pulse_calculation_enabled;
    doc["pump_off_flow_monitoring_enabled"] = settings.pump_off_flow_monitoring_enabled;
    doc["pump_off_flow_grace_period_seconds"] = settings.pump_off_flow_grace_period_seconds;
    doc["pump_off_flow_pulse_threshold"] = settings.pump_off_flow_pulse_threshold;

    // Pump minimum daily cycles settings
    doc["pump_min_daily_cycles_enabled"] = settings.pump_min_daily_cycles_enabled;
    doc["pump_min_daily_cycles"] = settings.pump_min_daily_cycles;
    doc["pump_min_cycle_run_seconds"] = settings.pump_min_cycle_run_seconds;

    // WiFi connection settings
    doc["wifi_max_retries"] = settings.wifi_max_retries;
    doc["wifi_retry_delay_seconds"] = settings.wifi_retry_delay_seconds;
    doc["wifi_ap_duration_minutes"] = settings.wifi_ap_duration_minutes;
    doc["watchdog_timeout_seconds"] = settings.watchdog_timeout_seconds;
    doc["wifi_led_enabled"] = settings.wifi_led_enabled;
    doc["wifi_bssid_preference"] = settings.wifi_bssid_preference;
    doc["hostname"] = settings.hostname;

    // Buzzer settings
    doc["buzzer_enabled"] = settings.buzzer_enabled;
    doc["buzzer_type"] = settings.buzzer_type;
    
    // Door control settings
    doc["door_auto_mode"] = settings.door_auto_mode;
    doc["door_open_timeout_seconds"] = settings.door_open_timeout_seconds;
    doc["door_close_timeout_seconds"] = settings.door_close_timeout_seconds;
    doc["sunrise_offset_minutes"] = settings.sunrise_offset_minutes;
    doc["sunset_offset_minutes"] = settings.sunset_offset_minutes;
    
    // Location settings
    doc["latitude"] = settings.latitude;
    doc["longitude"] = settings.longitude;
    doc["timezone_offset_hours"] = settings.timezone_offset_hours;
    doc["timezone_posix"] = settings.timezone_posix;

    // Door advanced features settings
    doc["door_auto_close_after_sunset_enabled"] = settings.door_auto_close_after_sunset_enabled;
    doc["door_auto_close_after_sunset_minutes"] = settings.door_auto_close_after_sunset_minutes;
    doc["door_lockout_enabled"] = settings.door_lockout_enabled;
    doc["door_timeout_auto_calc_enabled"] = settings.door_timeout_auto_calc_enabled;

    // API authentication settings
    doc["api_auth_enabled"] = settings.api_auth_enabled;
    doc["api_username"] = settings.api_username;
    if (includePassword && settings.api_password.length() != 0) {
        doc["api_password"] = settings.api_password;
    }

    // Syslog configuration
    doc["syslog_server"] = settings.syslog_server;
    doc["syslog_port"] = settings.syslog_port;

    // Flow calculation interval
    doc["flow_calculation_interval_seconds"] = settings.flow_calculation_interval_seconds;

    // Historical data collection
    doc["history_enabled"] = settings.history_enabled;
    doc["history_temp_min_interval_seconds"] = settings.history_temp_min_interval_seconds;
    doc["history_flow_min_interval_seconds"] = settings.history_flow_min_interval_seconds;
    doc["history_buffer_size"] = settings.history_buffer_size;

    // OTA update settings
    doc["auto_update_enabled"] = settings.auto_update_enabled;
    doc["update_check_interval_hours"] = settings.update_check_interval_hours;
    doc["manifest_url"] = settings.manifest_url;

    // Notification settings - Telegram
    doc["telegram_enabled"] = settings.telegram_enabled;
    if (includePassword && settings.telegram_bot_token.length() != 0) {
        doc["telegram_bot_token"] = settings.telegram_bot_token;
    }
    doc["telegram_chat_id"] = settings.telegram_chat_id;
    doc["telegram_polling_interval_seconds"] = settings.telegram_polling_interval_seconds;

    // Notification settings - Email
    doc["email_enabled"] = settings.email_enabled;
    doc["email_smtp_server"] = settings.email_smtp_server;
    doc["email_smtp_port"] = settings.email_smtp_port;
    doc["email_smtp_username"] = settings.email_smtp_username;
    if (includePassword && settings.email_smtp_password.length() != 0) {
        doc["email_smtp_password"] = settings.email_smtp_password;
    }
    doc["email_from"] = settings.email_from;
    doc["email_to"] = settings.email_to;

    // Notification preferences
    doc["notify_pump_error"] = settings.notify_pump_error;
    doc["notify_sensor_error"] = settings.notify_sensor_error;
    doc["notify_door_fault"] = settings.notify_door_fault;
    doc["notify_wifi_disconnect"] = settings.notify_wifi_disconnect;
    doc["notify_system_error"] = settings.notify_system_error;

    return doc;
}

String SettingsManager::toJson(bool includePassword) const {
    JsonDocument doc = toJsonDoc(includePassword);

    String output;
    serializeJson(doc, output);
    return output;
}

// New light timing getters/setters
int SettingsManager::getLightOnMinute() const {
    return settings.light_on_minute;
}

void SettingsManager::setLightOnMinute(int minute) {
    settings.light_on_minute = minute;
}

String SettingsManager::getLightOnMode() const {
    return settings.light_on_mode;
}

void SettingsManager::setLightOnMode(const String& mode) {
    settings.light_on_mode = mode;
}

int SettingsManager::getLightOnSunsetOffsetMinutes() const {
    return settings.light_on_sunset_offset_minutes;
}

void SettingsManager::setLightOnSunsetOffsetMinutes(int minutes) {
    settings.light_on_sunset_offset_minutes = minutes;
}

// OTA update settings getters
bool SettingsManager::getAutoUpdateEnabled() const {
    return settings.auto_update_enabled;
}

unsigned int SettingsManager::getUpdateCheckIntervalHours() const {
    return settings.update_check_interval_hours;
}

String SettingsManager::getManifestUrl() const {
    return settings.manifest_url;
}

// OTA update settings setters
void SettingsManager::setAutoUpdateEnabled(bool enabled) {
    settings.auto_update_enabled = enabled;
}

void SettingsManager::setUpdateCheckIntervalHours(unsigned int hours) {
    settings.update_check_interval_hours = constrain(hours, 1, 168);
}

void SettingsManager::setManifestUrl(const String& url) {
    settings.manifest_url = url;
}

// Telegram notification getters
bool SettingsManager::getTelegramEnabled() const { return settings.telegram_enabled; }
String SettingsManager::getTelegramBotToken() const { return settings.telegram_bot_token; }
String SettingsManager::getTelegramChatId() const { return settings.telegram_chat_id; }
unsigned int SettingsManager::getTelegramPollingIntervalSeconds() const { return settings.telegram_polling_interval_seconds; }

// Telegram notification setters
void SettingsManager::setTelegramEnabled(bool enabled) { settings.telegram_enabled = enabled; }
void SettingsManager::setTelegramBotToken(const String& token) { settings.telegram_bot_token = token; }
void SettingsManager::setTelegramChatId(const String& chatId) { settings.telegram_chat_id = chatId; }
void SettingsManager::setTelegramPollingIntervalSeconds(unsigned int seconds) {
    if (seconds >= 10 && seconds <= 300) settings.telegram_polling_interval_seconds = seconds;
}

// Email notification getters
bool SettingsManager::getEmailEnabled() const { return settings.email_enabled; }
String SettingsManager::getEmailSmtpServer() const { return settings.email_smtp_server; }
uint16_t SettingsManager::getEmailSmtpPort() const { return settings.email_smtp_port; }
String SettingsManager::getEmailSmtpUsername() const { return settings.email_smtp_username; }
String SettingsManager::getEmailSmtpPassword() const { return settings.email_smtp_password; }
String SettingsManager::getEmailFrom() const { return settings.email_from; }
String SettingsManager::getEmailTo() const { return settings.email_to; }

// Email notification setters
void SettingsManager::setEmailEnabled(bool enabled) { settings.email_enabled = enabled; }
void SettingsManager::setEmailSmtpServer(const String& server) { settings.email_smtp_server = server; }
void SettingsManager::setEmailSmtpPort(uint16_t port) { settings.email_smtp_port = constrain(port, 1, 65535); }
void SettingsManager::setEmailSmtpUsername(const String& username) { settings.email_smtp_username = username; }
void SettingsManager::setEmailSmtpPassword(const String& password) { settings.email_smtp_password = password; }
void SettingsManager::setEmailFrom(const String& from) { settings.email_from = from; }
void SettingsManager::setEmailTo(const String& to) { settings.email_to = to; }

// Notification preference getters
bool SettingsManager::getNotifyPumpError() const { return settings.notify_pump_error; }
bool SettingsManager::getNotifySensorError() const { return settings.notify_sensor_error; }
bool SettingsManager::getNotifyDoorFault() const { return settings.notify_door_fault; }
bool SettingsManager::getNotifyWifiDisconnect() const { return settings.notify_wifi_disconnect; }
bool SettingsManager::getNotifySystemError() const { return settings.notify_system_error; }

// Notification preference setters
void SettingsManager::setNotifyPumpError(bool enabled) { settings.notify_pump_error = enabled; }
void SettingsManager::setNotifySensorError(bool enabled) { settings.notify_sensor_error = enabled; }
void SettingsManager::setNotifyDoorFault(bool enabled) { settings.notify_door_fault = enabled; }
void SettingsManager::setNotifyWifiDisconnect(bool enabled) { settings.notify_wifi_disconnect = enabled; }
void SettingsManager::setNotifySystemError(bool enabled) { settings.notify_system_error = enabled; }
