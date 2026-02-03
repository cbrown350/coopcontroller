#ifndef EMULATOR_SETTINGS_H
#define EMULATOR_SETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "EmulatorStateManager.h"

/**
 * @brief Settings file path in LittleFS
 */
constexpr const char* SETTINGS_FILE_PATH = "/emulator_settings.json";

/**
 * @brief Manages persistent settings for the hardware emulator
 *
 * Handles WiFi credentials, emulator configuration, and scenario presets.
 * Settings are stored in LittleFS as JSON.
 */
class EmulatorSettings {
public:
    EmulatorSettings() = default;

    /**
     * @brief Initialize filesystem and load settings
     * @return true if initialization successful
     */
    bool begin();

    /**
     * @brief Load settings from filesystem
     * @return true if settings loaded successfully
     */
    bool load();

    /**
     * @brief Save settings to filesystem
     * @return true if settings saved successfully
     */
    bool save();

    /**
     * @brief Reset all settings to defaults
     */
    void resetToDefaults();

    // ========================================================================
    // WIFI SETTINGS
    // ========================================================================

    const String& getWifiSsid() const { return _wifiSsid; }
    const String& getWifiPassword() const { return _wifiPassword; }
    bool isApMode() const { return _apMode; }

    void setWifiSsid(const String& ssid) { _wifiSsid = ssid; }
    void setWifiPassword(const String& password) { _wifiPassword = password; }
    void setApMode(bool apMode) { _apMode = apMode; }

    // ========================================================================
    // EMULATOR CONFIGURATION
    // ========================================================================

    uint32_t getDoorTravelTimeMs() const { return _doorTravelTimeMs; }
    float getPulsesPerGallon() const { return _pulsesPerGallon; }
    float getFlowRateGPM() const { return _flowRateGPM; }
    bool getAutoSimulateDoor() const { return _autoSimulateDoor; }
    bool getAutoGeneratePulses() const { return _autoGeneratePulses; }

    void setDoorTravelTimeMs(uint32_t ms) { _doorTravelTimeMs = ms; }
    void setPulsesPerGallon(float ppg) { _pulsesPerGallon = ppg; }
    void setFlowRateGPM(float gpm) { _flowRateGPM = gpm; }
    void setAutoSimulateDoor(bool auto_) { _autoSimulateDoor = auto_; }
    void setAutoGeneratePulses(bool auto_) { _autoGeneratePulses = auto_; }

    // ========================================================================
    // LOGGING
    // ========================================================================

    const String& getLogLevel() const { return _logLevel; }
    void setLogLevel(const String& level) { _logLevel = level; }

    // ========================================================================
    // JSON SERIALIZATION
    // ========================================================================

    /**
     * @brief Serialize settings to JSON (for API response)
     * @param includePassword Whether to include WiFi password
     */
    void toJson(JsonObject& obj, bool includePassword = false) const;

    /**
     * @brief Load settings from JSON (for API update)
     */
    bool fromJson(const JsonObject& obj);

    /**
     * @brief Apply emulator config settings to state manager
     */
    void applyToStateManager(EmulatorStateManager& stateManager) const;

private:
    // WiFi
    String _wifiSsid = "";
    String _wifiPassword = "";
    bool _apMode = true;  // Start in AP mode by default

    // Emulator configuration
    uint32_t _doorTravelTimeMs = DEFAULT_DOOR_TRAVEL_TIME_MS;
    float _pulsesPerGallon = DEFAULT_PULSES_PER_GALLON;
    float _flowRateGPM = DEFAULT_FLOW_RATE_GPM;
    bool _autoSimulateDoor = true;
    bool _autoGeneratePulses = true;

    // Logging
    String _logLevel = "INFO";

    bool _initialized = false;
};

// Global settings instance
extern EmulatorSettings emulatorSettings;

#endif // EMULATOR_SETTINGS_H
