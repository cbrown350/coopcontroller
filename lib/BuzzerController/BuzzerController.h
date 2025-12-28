#ifndef __BUZZER_CONTROLLER_H__
#define __BUZZER_CONTROLLER_H__

#include "IHAL.h"

#include <ArduinoJson.h>

#include <stdint.h>

// Alert types for different system conditions
enum class AlertType : uint8_t {
    PUMP_ERROR = 0,
    SENSOR_ERROR = 1,
    WIFI_DISCONNECTED = 2,
    LOW_MEMORY = 3,
    DOOR_FAULT = 4,
    LIGHT_FAULT = 5,
    SYSTEM_ERROR = 6,     // General system errors
    TEST_ALERT = 7        // For testing purposes
};

// Buzzer types
enum class BuzzerType : uint8_t {
    ACTIVE = 0,    // Active buzzer (simple on/off)
    PASSIVE = 1    // Passive buzzer (requires tone generation)
};

// Alert pattern structure
struct AlertPattern {
    int beep_duration_ms;      // Duration of each beep in milliseconds
    int pause_duration_ms;     // Duration between beeps in milliseconds
    int repeat_count;          // Number of beeps in pattern
    int pattern_pause_ms;      // Pause between pattern repetitions
    int max_cycles;            // Maximum number of pattern cycles (0 = infinite)
};

class BuzzerController {
public:
    BuzzerController() = default;
    
    void begin(IHAL* hal, uint8_t pin);
    void update();
    
    // Alert triggering methods
    void triggerAlert(AlertType alertType);
    void silenceAlerts(unsigned long silenceDurationMs = 300000); // Default 5 minutes
    void testAlert();
    void clearAlert(AlertType alertType);
    
    // Configuration methods
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setBuzzerType(BuzzerType type);
    BuzzerType getBuzzerType() const;
    
    // Status methods
    bool hasActiveAlert() const;
    AlertType getCurrentAlertType() const;
    unsigned long getSilenceRemainingMs() const;
    
    // Settings integration
    void loadFromSettings();
    void saveToSettings() const;
    String getAlertTypeString(AlertType type) const;
    
    // JSON serialization
    void toJson(JsonObject& json) const;

private:
    IHAL* _hal;
    uint8_t _pin;
    bool _enabled = true;
    BuzzerType _buzzerType = BuzzerType::ACTIVE;
    
    // Alert state
    bool _hasActiveAlert = false;
    AlertType _currentAlertType = AlertType::SYSTEM_ERROR;
    unsigned long _silenceUntil;
    unsigned long _lastAlertTime;
    
    // Pattern execution state
    AlertPattern _currentPattern;
    unsigned int _currentCycle;
    unsigned int _currentBeep;
    unsigned long _lastStateChange;
    bool _isBeeping;
    
    // Default patterns for each alert type
    static const AlertPattern DEFAULT_PATTERNS[]; // NOSONAR - intentional array
    
    // Private methods
    void executePattern();
    void startBeep();
    void stopBeep();
    void nextPatternStep();
    bool isSilenced() const;
    void logAlert(AlertType type, const char* action) const;
};


#endif // __BUZZER_CONTROLLER_H__