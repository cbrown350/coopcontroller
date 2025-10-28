#include "SettingsManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdlib.h>

#include "Logger.h"

SettingsManager &SettingsManager::getInstance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
{
    isLoaded                     = false;
    settings.ap_mode             = false;
    settings.ssid                = "";
    settings.passwd              = "";
    settings.enabled             = false;
    settings.has_connected       = false;
    
    // Initialize coop controller settings with defaults
    settings.temp_threshold_f    = 34.0;      // Default 34°F
    settings.pump_on_time_seconds = 300;     // Default 5 minutes (300 seconds)
    settings.pump_off_time_seconds = 600;    // Default 10 minutes (600 seconds)
    settings.pump_auto_mode     = true;      // Enable automatic pump control by default
    settings.light_auto_mode     = false;     // Disable automatic light control initially
    settings.light_on_hour       = 6;         // Default turn on at 6 AM
    settings.light_off_hour      = 20;        // Default turn off at 8 PM
    settings.debug_enabled       = false;     // Debug disabled by default
    
    // Initialize WiFi connection settings with defaults
    settings.wifi_max_retries = 5;          // Default 5 retries
    settings.wifi_retry_delay_seconds = 30;   // Default 30 seconds between retries
    settings.wifi_ap_duration_minutes = 10;   // Default 10 minutes in AP mode
}

bool SettingsManager::load()
{
    File file = LittleFS.open("/user_settings.json", "r");
    if (!file)
    {
        logger.log("Settings file not found, using defaults");
        isLoaded = true;
        return false;
    }

    JsonDocument doc;
    DeserializationError     error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.log("Settings JSON parsing error, using defaults");
        isLoaded = true;
        return false;
    }

    settings.ap_mode             = doc["ap_mode"] | false;
    settings.ssid                = doc["ssid"] | "";
    settings.passwd              = doc["passwd"] | "";
    settings.enabled             = doc["enabled"] | true;
    settings.has_connected       = doc["has_connected"] | false;
    
    // Load coop controller settings with defaults
    settings.temp_threshold_f    = doc["temp_threshold_f"] | 34.0;
    settings.pump_on_time_seconds = doc["pump_on_time_seconds"] | 300;
    settings.pump_off_time_seconds = doc["pump_off_time_seconds"] | 600;
    settings.pump_auto_mode     = doc["pump_auto_mode"] | true;
    settings.light_auto_mode     = doc["light_auto_mode"] | false;
    settings.light_on_hour       = doc["light_on_hour"] | 6;
    settings.light_off_hour      = doc["light_off_hour"] | 20;
    settings.debug_enabled       = doc["debug_enabled"] | false;
    
    // Load WiFi connection settings with defaults
    settings.wifi_max_retries = doc["wifi_max_retries"] | 5;
    settings.wifi_retry_delay_seconds = doc["wifi_retry_delay_seconds"] | 30;
    settings.wifi_ap_duration_minutes = doc["wifi_ap_duration_minutes"] | 10;

    isLoaded = true;
    return true;
}

bool SettingsManager::save()
{
    // Ensure settings are loaded before saving
    if (!isLoaded)
        load();
        
    String output = toJson(true);

    File file = LittleFS.open("/user_settings.json", "w");
    if (!file)
    {
        logger.log("Failed to open settings file for writing");
        return false;
    }

    if (file.print(output) == 0)
    {
        logger.log("Failed to write settings to file");
        file.close();
        return false;
    }

    file.close();
    logger.log("Settings saved successfully");
    if (wifiChanged)
    {
        logger.log("Wifi changed, requesting restart");
        requestRestartAt = millis() + 3000;
        wifiChanged      = false;
    }
    return true;
}

const user_settings &SettingsManager::getSettings()
{
    if (!isLoaded)
    {
        load();
    }
    return settings;
}

String SettingsManager::getSSID()
{
    return getSettings().ssid;
}

String SettingsManager::getPassword()
{
    return getSettings().passwd;
}

bool SettingsManager::isAPMode()
{
    return getSettings().ap_mode;
}

bool SettingsManager::getEnabled()
{
    return getSettings().enabled;
}

bool SettingsManager::getHasConnected()
{
    return getSettings().has_connected;
}

// Coop Controller getters
float SettingsManager::getTempThresholdF()
{
    return getSettings().temp_threshold_f;
}

int SettingsManager::getPumpOnTimeSeconds()
{
    return getSettings().pump_on_time_seconds;
}

int SettingsManager::getPumpOffTimeSeconds()
{
    return getSettings().pump_off_time_seconds;
}

bool SettingsManager::getPumpAutoMode()
{
    return getSettings().pump_auto_mode;
}

bool SettingsManager::getLightAutoMode()
{
    return getSettings().light_auto_mode;
}

int SettingsManager::getLightOnHour()
{
    return getSettings().light_on_hour;
}

int SettingsManager::getLightOffHour()
{
    return getSettings().light_off_hour;
}

bool SettingsManager::getDebugEnabled()
{
    return getSettings().debug_enabled;
}

void SettingsManager::setTempThresholdF(float threshold)
{
    if (!isLoaded)
        load();
    settings.temp_threshold_f = threshold;
}

void SettingsManager::setPumpOnTimeSeconds(int seconds)
{
    if (!isLoaded)
        load();
    settings.pump_on_time_seconds = seconds;
}

void SettingsManager::setPumpOffTimeSeconds(int seconds)
{
    if (!isLoaded)
        load();
    settings.pump_off_time_seconds = seconds;
}

void SettingsManager::setPumpAutoMode(bool enabled)
{
    if (!isLoaded)
        load();
    settings.pump_auto_mode = enabled;
}

void SettingsManager::setLightAutoMode(bool enabled)
{
    if (!isLoaded)
        load();
    settings.light_auto_mode = enabled;
}

void SettingsManager::setLightOnHour(int hour)
{
    if (!isLoaded)
        load();
    settings.light_on_hour = hour;
}

void SettingsManager::setLightOffHour(int hour)
{
    if (!isLoaded)
        load();
    settings.light_off_hour = hour;
}

void SettingsManager::setDebugEnabled(bool enabled)
{
    if (!isLoaded)
        load();
    settings.debug_enabled = enabled;
}

// WiFi connection settings getters
int SettingsManager::getWifiMaxRetries()
{
    return getSettings().wifi_max_retries;
}

int SettingsManager::getWifiRetryDelaySeconds()
{
    return getSettings().wifi_retry_delay_seconds;
}

int SettingsManager::getWifiAPDurationMinutes()
{
    return getSettings().wifi_ap_duration_minutes;
}

// WiFi connection settings setters
void SettingsManager::setWifiMaxRetries(int retries)
{
    if (!isLoaded)
        load();
    settings.wifi_max_retries = retries;
}

void SettingsManager::setWifiRetryDelaySeconds(int seconds)
{
    if (!isLoaded)
        load();
    settings.wifi_retry_delay_seconds = seconds;
}

void SettingsManager::setWifiAPDurationMinutes(int minutes)
{
    if (!isLoaded)
        load();
    settings.wifi_ap_duration_minutes = minutes;
}

void SettingsManager::setSSID(const String &ssid)
{
    if (!isLoaded)
        load();
    if (settings.ssid != ssid)
    {
        settings.ssid = ssid;
        wifiChanged   = true;
    }
}

void SettingsManager::setPassword(const String &password)
{
    if (!isLoaded)
        load();
    if (settings.passwd != password)
    {
        settings.passwd = password;
        wifiChanged     = true;
    }
}

void SettingsManager::setAPMode(bool apMode)
{
    if (!isLoaded)
        load();
    if (settings.ap_mode != apMode)
    {
        settings.ap_mode = apMode;
        wifiChanged      = true;
    }
}

void SettingsManager::setEnabled(bool enabled)
{
    if (!isLoaded)
        load();
    settings.enabled = enabled;
}

void SettingsManager::setHasConnected(bool hasConnected)
{
    if (!isLoaded)
        load();
    settings.has_connected = hasConnected;
}

String SettingsManager::toJson(bool includePassword) const
{
    String                   output;
    JsonDocument doc;

    doc["ap_mode"]             = settings.ap_mode;
    doc["ssid"]                = settings.ssid;
    doc["enabled"]             = settings.enabled;
    doc["has_connected"]       = settings.has_connected;
    
    // Coop controller settings
    doc["temp_threshold_f"]    = settings.temp_threshold_f;
    doc["pump_on_time_seconds"] = settings.pump_on_time_seconds;
    doc["pump_off_time_seconds"] = settings.pump_off_time_seconds;
    doc["pump_auto_mode"]     = settings.pump_auto_mode;
    doc["light_auto_mode"]     = settings.light_auto_mode;
    doc["light_on_hour"]       = settings.light_on_hour;
    doc["light_off_hour"]      = settings.light_off_hour;
    doc["debug_enabled"]       = settings.debug_enabled;
    
    // WiFi connection settings
    doc["wifi_max_retries"] = settings.wifi_max_retries;
    doc["wifi_retry_delay_seconds"] = settings.wifi_retry_delay_seconds;
    doc["wifi_ap_duration_minutes"] = settings.wifi_ap_duration_minutes;

    if (includePassword)
    {
        doc["passwd"] = settings.passwd;
    }

    serializeJson(doc, output);
    return output;
}
