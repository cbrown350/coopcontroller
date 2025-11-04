#ifndef __BUZZER_CONTROLLER_H__
#define __BUZZER_CONTROLLER_H__

#include <Arduino.h>
#include <ArduinoJson.h>

// Alert types for different system conditions
enum class AlertType : uint8_t {
    PUMP_ERROR = 0,
    SENSOR_ERROR = 1,
    WIFI_DISCONNECTED = 2,
    LOW_MEMORY = 3,
    DOOR_FAULT = 4,      // Future-proof for door system
    SYSTEM_ERROR = 5,     // General system errors
    TEST_ALERT = 255      // For testing purposes
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
    BuzzerController(int pin = BUZZER_B_PIN);
    
public:
    void begin();
    void update();
    
    // Alert triggering methods
    void triggerAlert(AlertType alertType);
    void silenceAlerts(unsigned long silenceDurationMs = 300000); // Default 5 minutes
    void testAlert();
    
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
    void saveToSettings();
    String getAlertTypeString(AlertType type) const;
    
    // JSON serialization
    void toJson(JsonObject& json) const;

private:
    int _pin;
    bool _enabled;
    BuzzerType _buzzerType;
    
    // Alert state
    bool _hasActiveAlert;
    AlertType _currentAlertType;
    unsigned long _silenceUntil;
    unsigned long _lastAlertTime;
    
    // Pattern execution state
    AlertPattern _currentPattern;
    int _currentCycle;
    int _currentBeep;
    unsigned long _lastStateChange;
    bool _isBeeping;
    
    // Default patterns for each alert type
    static const AlertPattern DEFAULT_PATTERNS[];
    
    // Private methods
    void executePattern();
    void startBeep();
    void stopBeep();
    void nextPatternStep();
    bool isSilenced() const;
    void logAlert(AlertType type, const char* action);
};

// Convenience macro for easier access
#define buzzerController buzzerController

#endif // __BUZZER_CONTROLLER_H__