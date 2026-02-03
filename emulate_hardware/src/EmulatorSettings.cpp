#include "EmulatorSettings.h"

// Global instance
EmulatorSettings emulatorSettings;

bool EmulatorSettings::begin() {
    if (!LittleFS.begin(true)) {  // Format on fail
        Serial.println("[EmulatorSettings] Failed to mount LittleFS");
        return false;
    }

    Serial.println("[EmulatorSettings] LittleFS mounted");
    _initialized = true;

    // Load settings from file
    if (!load()) {
        Serial.println("[EmulatorSettings] No settings found, using defaults");
        save();  // Save defaults
    }

    return true;
}

bool EmulatorSettings::load() {
    if (!_initialized) {
        return false;
    }

    if (!LittleFS.exists(SETTINGS_FILE_PATH)) {
        Serial.println("[EmulatorSettings] Settings file not found");
        return false;
    }

    File file = LittleFS.open(SETTINGS_FILE_PATH, "r");
    if (!file) {
        Serial.println("[EmulatorSettings] Failed to open settings file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[EmulatorSettings] JSON parse error: %s\n", error.c_str());
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();
    return fromJson(obj);
}

bool EmulatorSettings::save() {
    if (!_initialized) {
        return false;
    }

    File file = LittleFS.open(SETTINGS_FILE_PATH, "w");
    if (!file) {
        Serial.println("[EmulatorSettings] Failed to create settings file");
        return false;
    }

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    toJson(obj, true);  // Include password when saving

    if (serializeJson(doc, file) == 0) {
        Serial.println("[EmulatorSettings] Failed to write settings");
        file.close();
        return false;
    }

    file.close();
    Serial.println("[EmulatorSettings] Settings saved");
    return true;
}

void EmulatorSettings::resetToDefaults() {
    _wifiSsid = "";
    _wifiPassword = "";
    _apMode = true;

    _doorTravelTimeMs = DEFAULT_DOOR_TRAVEL_TIME_MS;
    _pulsesPerGallon = DEFAULT_PULSES_PER_GALLON;
    _flowRateGPM = DEFAULT_FLOW_RATE_GPM;
    _autoSimulateDoor = true;
    _autoGeneratePulses = true;

    _logLevel = "INFO";

    Serial.println("[EmulatorSettings] Reset to defaults");
}

void EmulatorSettings::toJson(JsonObject& obj, bool includePassword) const {
    // WiFi settings
    obj["ssid"] = _wifiSsid;
    if (includePassword) {
        obj["passwd"] = _wifiPassword;
    }
    obj["ap_mode"] = _apMode;

    // Emulator configuration
    obj["door_travel_time_ms"] = _doorTravelTimeMs;
    obj["pulses_per_gallon"] = _pulsesPerGallon;
    obj["flow_rate_gpm"] = _flowRateGPM;
    obj["auto_simulate_door"] = _autoSimulateDoor;
    obj["auto_generate_pulses"] = _autoGeneratePulses;

    // Logging
    obj["log_level"] = _logLevel;

    // Device info
    obj["hostname"] = hostName;
    obj["firmware_version"] = firmwareVersion;
}

bool EmulatorSettings::fromJson(const JsonObject& obj) {
    // WiFi settings
    if (obj["ssid"].is<const char*>()) {
        _wifiSsid = obj["ssid"].as<String>();
    }
    if (obj["passwd"].is<const char*>()) {
        _wifiPassword = obj["passwd"].as<String>();
    }
    if (obj["ap_mode"].is<bool>()) {
        _apMode = obj["ap_mode"].as<bool>();
    }

    // Emulator configuration
    if (obj["door_travel_time_ms"].is<uint32_t>()) {
        _doorTravelTimeMs = obj["door_travel_time_ms"].as<uint32_t>();
    }
    if (obj["pulses_per_gallon"].is<float>()) {
        _pulsesPerGallon = obj["pulses_per_gallon"].as<float>();
    }
    if (obj["flow_rate_gpm"].is<float>()) {
        _flowRateGPM = obj["flow_rate_gpm"].as<float>();
    }
    if (obj["auto_simulate_door"].is<bool>()) {
        _autoSimulateDoor = obj["auto_simulate_door"].as<bool>();
    }
    if (obj["auto_generate_pulses"].is<bool>()) {
        _autoGeneratePulses = obj["auto_generate_pulses"].as<bool>();
    }

    // Logging
    if (obj["log_level"].is<const char*>()) {
        _logLevel = obj["log_level"].as<String>();
    }

    Serial.println("[EmulatorSettings] Settings loaded from JSON");
    return true;
}

void EmulatorSettings::applyToStateManager(EmulatorStateManager& stateManager) const {
    EmulatorConfig config;
    config.doorTravelTimeMs = _doorTravelTimeMs;
    config.autoSimulateDoor = _autoSimulateDoor;
    config.pulsesPerGallon = _pulsesPerGallon;
    config.flowRateGPM = _flowRateGPM;
    config.autoGeneratePulses = _autoGeneratePulses;
    config.injectDoorFault = false;
    config.simulateFrozenLine = false;
    config.simulateDoorStuck = false;

    stateManager.setConfig(config);
    Serial.println("[EmulatorSettings] Applied settings to state manager");
}
