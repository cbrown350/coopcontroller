#ifndef __BUZZER_CONTROLLER_H__
#define __BUZZER_CONTROLLER_H__

#include <ArduinoJson.h>

#include <stdint.h>

/**
 * @brief Alert types for different system conditions
 *
 * Enumeration of all possible alert types that can trigger the buzzer.
 * Each alert type has a unique identifier and can be associated with
 * a specific beep pattern.
 */
enum class AlertType : uint8_t {
    PUMP_ERROR = 0,        ///< Water pump malfunction or flow error detected
    SENSOR_ERROR = 1,      ///< Sensor failure or disconnection detected
    WIFI_DISCONNECTED = 2, ///< WiFi connection lost
    LOW_MEMORY = 3,        ///< System memory critically low (>80% used)
    DOOR_FAULT = 4,        ///< Door controller fault or timeout
    LIGHT_FAULT = 5,       ///< Light controller fault detected
    SYSTEM_ERROR = 6,      ///< General system errors (watchdog, crash, etc.)
    TEST_ALERT = 7         ///< For testing purposes
};

/**
 * @brief Buzzer hardware types
 *
 * Different buzzer hardware types require different control methods.
 * Active buzzers are simple on/off, while passive buzzers need tone generation.
 */
enum class BuzzerType : uint8_t {
    ACTIVE = 0,    ///< Active buzzer (simple on/off, generates own tone)
    PASSIVE = 1    ///< Passive buzzer (requires PWM tone generation)
};

/**
 * @brief Alert beep pattern configuration
 *
 * Defines the timing and repetition pattern for alert beeps.
 * Patterns can be customized per alert type to create distinctive sounds.
 */
struct AlertPattern {
    unsigned int beep_duration_ms;      ///< Duration of each beep in milliseconds
    unsigned int pause_duration_ms;     ///< Duration between beeps in milliseconds
    unsigned int repeat_count;          ///< Number of beeps in pattern
    unsigned int pattern_pause_ms;      ///< Pause between pattern repetitions
    unsigned int max_cycles;            ///< Maximum number of pattern cycles (0 = infinite)
};

/**
 * @brief Buzzer alert controller for chicken coop system
 *
 * Controls a piezoelectric buzzer to provide audible alerts for various
 * system conditions. Supports both active and passive buzzers with customizable
 * beep patterns for different alert types. Includes silence functionality
 * and settings persistence.
 *
 * Features:
 * - Multiple alert patterns for different conditions
 * - Configurable beep duration, pause, and repetition
 * - Silence mode with configurable duration
 * - Active and passive buzzer support
 * - Settings persistence via SettingsManager
 * - Real-time alert status monitoring
 */
class BuzzerController {
public:
    /**
     * @brief Default constructor
     *
     * Initializes the buzzer controller in a default state.
     * Must call begin() before use.
     */
    BuzzerController() = default;

    /**
     * @brief Initialize the buzzer controller
     *
     * Configures the GPIO pin for buzzer control and loads settings.
     * Must be called before any other methods.
     *
     * @param pin GPIO pin number connected to the buzzer
     */
    void begin(uint8_t pin);

    /**
     * @brief Update buzzer state (call frequently in main loop)
     *
     * Handles timing for beep patterns, silence timeout, and state transitions.
     * Should be called every loop iteration for accurate timing.
     */
    void update();

    // ========================================================================
    // ALERT TRIGGERING METHODS
    // ========================================================================

    /**
     * @brief Trigger an alert with specified pattern
     *
     * Starts the beep pattern associated with the given alert type.
     * If already alerting, replaces the current alert with the new one.
     * Virtual method to allow mocking in tests.
     *
     * @param alertType Type of alert to trigger
     */
    virtual void triggerAlert(AlertType alertType);

    /**
     * @brief Silence all alerts for specified duration
     *
     * Disables all alert beeping for the specified time period.
     * Useful for temporary silencing during maintenance or testing.
     *
     * @param silenceDurationMs Duration to silence alerts in milliseconds (default: 300000 = 5 minutes)
     */
    void silenceAlerts(unsigned long silenceDurationMs = 300000);

    /**
     * @brief Trigger test alert for buzzer functionality
     *
     * Activates the TEST_ALERT pattern to verify buzzer operation.
     * Useful for system testing and diagnostics.
     */
    void testAlert();

    /**
     * @brief Clear a specific active alert
     *
     * Stops the specified alert if it's currently active.
     * If no other alerts are active, the buzzer will be silenced.
     * Virtual method to allow mocking in tests.
     *
     * @param alertType Type of alert to clear
     */
    virtual void clearAlert(AlertType alertType);

    // ========================================================================
    // CONFIGURATION METHODS
    // ========================================================================

    /**
     * @brief Enable or disable buzzer alerts
     *
     * When disabled, the buzzer will not sound for any alerts.
     * Useful for temporary disabling during testing or maintenance.
     *
     * @param enabled true to enable alerts, false to disable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if buzzer alerts are enabled
     *
     * @return true if alerts are enabled, false if disabled
     */
    bool isEnabled() const;

    /**
     * @brief Set the buzzer hardware type
     *
     * Configures the controller for the attached buzzer type.
     * Active buzzers use simple on/off control.
     * Passive buzzers require PWM tone generation.
     *
     * @param type BuzzerType::ACTIVE or BuzzerType::PASSIVE
     */
    void setBuzzerType(BuzzerType type);

    /**
     * @brief Get the current buzzer hardware type
     *
     * @return Current BuzzerType setting
     */
    BuzzerType getBuzzerType() const;

    // ========================================================================
    // STATUS METHODS
    // ========================================================================

    /**
     * @brief Check if an alert is currently active
     *
     * @return true if buzzer is currently beeping, false otherwise
     */
    bool hasActiveAlert() const;

    /**
     * @brief Get the currently active alert type
     *
     * @return AlertType of the current alert (or SYSTEM_ERROR if none)
     */
    AlertType getCurrentAlertType() const;

    /**
     * @brief Get remaining silence time
     *
     * @return Milliseconds remaining until silence period ends (0 if not silenced)
     */
    unsigned long getSilenceRemainingMs() const;

    // ========================================================================
    // SETTINGS INTEGRATION
    // ========================================================================

    /**
     * @brief Load buzzer settings from SettingsManager
     *
     * Retrieves and applies buzzer configuration from persistent storage.
     * Includes enabled state and buzzer type.
     */
    void loadFromSettings();

    /**
     * @brief Save current buzzer settings to SettingsManager
     *
     * Persists current buzzer configuration to non-volatile storage.
     */
    void saveToSettings() const;

    /**
     * @brief Convert AlertType to human-readable string
     *
     * @param type AlertType to convert
     * @return String representation of the alert type
     */
    String getAlertTypeString(AlertType type) const;

    // ========================================================================
    // JSON SERIALIZATION
    // ========================================================================

    /**
     * @brief Serialize buzzer state to JSON
     *
     * Populates the provided JsonObject with current buzzer status
     * including enabled state, alert status, and configuration.
     *
     * @param json JsonObject to populate with buzzer state
     */
    void toJson(JsonObject& json) const;

private:
    uint8_t _pin;                           ///< GPIO pin for buzzer control
    bool _enabled = true;                   ///< Buzzer enabled flag
    BuzzerType _buzzerType = BuzzerType::ACTIVE; ///< Hardware buzzer type

    // Alert state
    bool _hasActiveAlert = false;           ///< Currently beeping flag
    AlertType _currentAlertType = AlertType::SYSTEM_ERROR; ///< Active alert type
    unsigned long _silenceUntil;            ///< Timestamp when silence period ends
    unsigned long _lastAlertTime;           ///< Timestamp of last alert trigger

    // Pattern execution state
    AlertPattern _currentPattern;           ///< Currently executing pattern
    unsigned int _currentCycle;             ///< Current pattern cycle count
    unsigned int _currentBeep;              ///< Current beep in pattern
    unsigned long _lastStateChange;         ///< Timestamp of last beep state change
    bool _isBeeping;                        ///< Current beep output state

    /**
     * @brief Default beep patterns for each alert type
     *
     * Array of AlertPattern structures defining the beep pattern for each
     * AlertType. Indexed by AlertType enum value.
     */
    static const AlertPattern DEFAULT_PATTERNS[]; // NOSONAR - intentional array

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Execute current beep pattern
     *
     * Handles timing and state transitions for beep pattern execution.
     * Called from update() method.
     */
    void executePattern();

    /**
     * @brief Start beep output
     *
     * Activates the buzzer based on hardware type (active or passive).
     */
    void startBeep();

    /**
     * @brief Stop beep output
     *
     * Deactivates the buzzer output.
     */
    void stopBeep();

    /**
     * @brief Advance to next step in beep pattern
     *
     * Calculates timing for next beep or pause in the pattern.
     */
    void nextPatternStep();

    /**
     * @brief Check if currently in silence period
     *
     * @return true if silenced, false otherwise
     */
    bool isSilenced() const;

    /**
     * @brief Log alert action to logger
     *
     * Records alert trigger/clear actions for debugging.
     *
     * @param type AlertType being logged
     * @param action Action description (e.g., "triggered", "cleared")
     */
    void logAlert(AlertType type, const char* action) const;
};


#endif // __BUZZER_CONTROLLER_H__