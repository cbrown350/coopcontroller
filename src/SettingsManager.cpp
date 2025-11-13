#include "SettingsManager.h"
#include "Logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

SettingsManager::SettingsManager() : isLoaded(false), wifiChanged(false), requestRestartAt(0) {
    // Initialize with default values
    settings = user_settings{};
}

SettingsManager &SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

bool SettingsManager::load() {
    if (isLoaded) {
        return true;
    }

    File file = LittleFS.open("/user_settings.json", "r");
    if (!file) {
        logger.logInfo("Settings file not found, using defaults");
        // Set default values
        settings = user_settings{};
        isLoaded = true;
        return false;
    }

    String content = file.readString();
    file.close();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        logger.logError("Settings JSON parsing error, using defaults");
        settings = user_settings{};
        isLoaded = true;
        return false;
    }

    // Load WiFi settings
    settings.ssid = doc["ssid"] | "";
    settings.passwd = doc["passwd"] | "";
    settings.ap_mode = doc["ap_mode"] | false;
    settings.enabled = doc["enabled"] | true;
    settings.has_connected = doc["has_connected"] | false;
    
    // Load Coop Controller settings
    settings.temp_threshold_on_f = doc["temp_threshold_on_f"] | 34.0;
    settings.temp_threshold_off_f = doc["temp_threshold_off_f"] | 36.0;
    settings.pump_on_time_seconds = doc["pump_on_time_seconds"] | 300;
    settings.pump_off_time_seconds = doc["pump_off_time_seconds"] | 600;
    settings.pump_auto_mode = doc["pump_auto_mode"] | true;
    settings.light_auto_mode = doc["light_auto_mode"] | false;
    settings.light_on_hour = doc["light_on_hour"] | 6;
    settings.light_on_minute = doc["light_on_minute"] | 0;
    settings.light_on_mode = doc["light_on_mode"] | "fixed";
    settings.light_on_sunset_offset_minutes = doc["light_on_sunset_offset_minutes"] | 0;
    settings.light_off_hour = doc["light_off_hour"] | 21;
    settings.light_brightness_percent = doc["light_brightness_percent"] | 80;
    settings.light_transition_duration_minutes = doc["light_transition_duration_minutes"] | 15;
    settings.water_flow_error_timeout_seconds = doc["water_flow_error_timeout_seconds"] | 120;
    settings.log_level = doc["log_level"] | "INFO";
    
    // Load water meter calibration
    settings.pulses_per_gallon = doc["pulses_per_gallon"] | 450.0;
    settings.water_meter_timeout_seconds = doc["water_meter_timeout_seconds"] | 300;
    
    // Load WiFi connection settings
    settings.wifi_max_retries = doc["wifi_max_retries"] | 5;
    settings.wifi_retry_delay_seconds = doc["wifi_retry_delay_seconds"] | 30;
    settings.wifi_ap_duration_minutes = doc["wifi_ap_duration_minutes"] | 10;
    settings.watchdog_timeout_seconds = doc["watchdog_timeout_seconds"] | 30;
    settings.wifi_led_enabled = doc["wifi_led_enabled"] | true;
    
    // Load buzzer settings
    settings.buzzer_enabled = doc["buzzer_enabled"] | true;
    settings.buzzer_type = doc["buzzer_type"] | "ACTIVE";
    
    // Load door control settings
    settings.door_auto_mode = doc["door_auto_mode"] | false;
    settings.door_open_timeout_seconds = doc["door_open_timeout_seconds"] | 30;
    settings.door_close_timeout_seconds = doc["door_close_timeout_seconds"] | 30;
    settings.sunrise_offset_minutes = doc["sunrise_offset_minutes"] | 0;
    settings.sunset_offset_minutes = doc["sunset_offset_minutes"] | 0;
    
    // Load location settings
    settings.latitude = doc["latitude"] | 40.7128;
    settings.longitude = doc["longitude"] | -74.0060;
    settings.timezone_offset_hours = doc["timezone_offset_hours"] | -5;
    
    // Load door auto close settings
    settings.door_auto_close_after_sunset_enabled = doc["door_auto_close_after_sunset_enabled"] | false;
    settings.door_auto_close_after_sunset_minutes = doc["door_auto_close_after_sunset_minutes"] | 0;

    isLoaded = true;
    logger.logInfo("Settings loaded successfully");
    return true;
}

bool SettingsManager::save() {
    DynamicJsonDocument doc(4096);
    
    // WiFi settings
    doc["ssid"] = settings.ssid;
    if (settings.passwd.length() > 0) {
        doc["passwd"] = settings.passwd;
    }
    doc["ap_mode"] = settings.ap_mode;
    doc["enabled"] = settings.enabled;
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
    
    // Door auto close settings
    doc["door_auto_close_after_sunset_enabled"] = settings.door_auto_close_after_sunset_enabled;
    doc["door_auto_close_after_sunset_minutes"] = settings.door_auto_close_after_sunset_minutes;

    File file = LittleFS.open("/user_settings.json", "w");
    if (!file) {
        logger.logError("Failed to open settings file for writing");
        return false;
    }

    String output;
    serializeJson(doc, output);
    
    if (file.print(output) != output.length()) {
        logger.logError("Failed to write settings to file");
        file.close();
        return false;
    }

    file.close();
    logger.logInfo("Settings saved successfully");
    return true;
}

const user_settings &SettingsManager::getSettings() {
    if (!isLoaded) {
        load();
    }
    return settings;
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

int SettingsManager::getPumpOnTimeSeconds() {
    return getSettings().pump_on_time_seconds;
}

int SettingsManager::getPumpOffTimeSeconds() {
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
int SettingsManager::getWaterMeterTimeoutSeconds() const {
    return settings.water_meter_timeout_seconds;
}

// WiFi connection settings getters
int SettingsManager::getWifiMaxRetries() {
    return getSettings().wifi_max_retries;
}

int SettingsManager::getWifiRetryDelaySeconds() {
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

// Door auto close getters
bool SettingsManager::getDoorAutoCloseAfterSunsetEnabled() const {
    return settings.door_auto_close_after_sunset_enabled;
}

int SettingsManager::getDoorAutoCloseAfterSunsetMinutes() const {
    return settings.door_auto_close_after_sunset_minutes;
}

// WiFi setters
void SettingsManager::setSSID(const String &ssid) {
    settings.ssid = ssid;
    wifiChanged = true;
}

void SettingsManager::setPassword(const String &password) {
    settings.passwd = password;
    wifiChanged = true;
}

void SettingsManager::setAPMode(bool apMode) {
    settings.ap_mode = apMode;
    wifiChanged = true;
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

// WiFi connection settings setters
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

// Door auto close setters
void SettingsManager::setDoorAutoCloseAfterSunsetEnabled(bool enabled) {
    settings.door_auto_close_after_sunset_enabled = enabled;
}

void SettingsManager::setDoorAutoCloseAfterSunsetMinutes(int minutes) {
    settings.door_auto_close_after_sunset_minutes = minutes;
}

void SettingsManager::factoryReset() {
    logger.logWarning("Factory reset initiated - clearing all settings");
    
    // Create default settings
    user_settings newSettings{};
    
    // WiFi defaults
    newSettings.ssid = "";
    newSettings.passwd = "";
    newSettings.ap_mode = true;
    newSettings.enabled = true;
    newSettings.has_connected = false;
    
    // Coop Controller defaults
    newSettings.temp_threshold_on_f = 34.0;
    newSettings.temp_threshold_off_f = 36.0;
    newSettings.pump_on_time_seconds = 300;
    newSettings.pump_off_time_seconds = 600;
    newSettings.pump_auto_mode = true;
    newSettings.light_auto_mode = false;
    newSettings.light_on_hour = 6;
    newSettings.light_on_minute = 0;
    newSettings.light_on_mode = "fixed";
    newSettings.light_on_sunset_offset_minutes = 0;
    newSettings.light_off_hour = 21;
    newSettings.light_brightness_percent = 80;
    newSettings.light_transition_duration_minutes = 15;
    newSettings.water_flow_error_timeout_seconds = 120;
    newSettings.log_level = "INFO";
    
    // Water meter calibration defaults
    newSettings.pulses_per_gallon = 450.0;
    newSettings.water_meter_timeout_seconds = 300;
    
    // WiFi connection settings defaults
    newSettings.wifi_max_retries = 5;
    newSettings.wifi_retry_delay_seconds = 30;
    newSettings.wifi_ap_duration_minutes = 10;
    newSettings.watchdog_timeout_seconds = 30;
    newSettings.wifi_led_enabled = true;
    
    // Buzzer settings defaults
    newSettings.buzzer_enabled = true;
    newSettings.buzzer_type = "ACTIVE";
    
    // Door control settings defaults
    newSettings.door_auto_mode = false;
    newSettings.door_open_timeout_seconds = 30;
    newSettings.door_close_timeout_seconds = 30;
    newSettings.sunrise_offset_minutes = 0;
    newSettings.sunset_offset_minutes = 0;
    
    // Location settings defaults
    newSettings.latitude = 40.7128;  // NYC
    newSettings.longitude = -74.0060; // NYC
    newSettings.timezone_offset_hours = -5; // EST
    
    // Door auto close defaults
    newSettings.door_auto_close_after_sunset_enabled = false;
    newSettings.door_auto_close_after_sunset_minutes = 0;
    
    // Apply new settings
    settings = newSettings;
    isLoaded = true;
    wifiChanged = true;
    
    // Save to file
    save();
    
    logger.logWarning("Factory reset completed");
}

String SettingsManager::toJson(bool includePassword) const {
    DynamicJsonDocument doc(4096);
    
    // WiFi settings
    doc["ssid"] = settings.ssid;
    if (includePassword && settings.passwd.length() > 0) {
        doc["passwd"] = settings.passwd;
    }
    doc["ap_mode"] = settings.ap_mode;
    doc["enabled"] = settings.enabled;
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
    
    // Door auto close settings
    doc["door_auto_close_after_sunset_enabled"] = settings.door_auto_close_after_sunset_enabled;
    doc["door_auto_close_after_sunset_minutes"] = settings.door_auto_close_after_sunset_minutes;

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