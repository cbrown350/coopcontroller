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
    
    // Load door advanced features settings
    settings.door_auto_close_after_sunset_enabled = doc["door_auto_close_after_sunset_enabled"] | defaultSettings.door_auto_close_after_sunset_enabled;
    settings.door_auto_close_after_sunset_minutes = doc["door_auto_close_after_sunset_minutes"] | defaultSettings.door_auto_close_after_sunset_minutes;
    settings.door_lockout_enabled = doc["door_lockout_enabled"] | defaultSettings.door_lockout_enabled;
    settings.door_timeout_auto_calc_enabled = doc["door_timeout_auto_calc_enabled"] | defaultSettings.door_timeout_auto_calc_enabled;

    // Load API authentication settings
    settings.api_auth_enabled = doc["api_auth_enabled"] | defaultSettings.api_auth_enabled;
    settings.api_username = doc["api_username"] | defaultSettings.api_username;
    settings.api_password = doc["api_password"] | defaultSettings.api_password;
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
