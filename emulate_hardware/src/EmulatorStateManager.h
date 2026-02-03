#ifndef EMULATOR_STATE_MANAGER_H
#define EMULATOR_STATE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

/**
 * @brief Door position states for the emulator
 */
enum class DoorState {
    OPEN,
    CLOSED,
    OPENING,
    CLOSING,
    STOPPED,
    UNKNOWN
};

/**
 * @brief Motor direction based on H-bridge signals
 */
enum class MotorDirection {
    STOPPED,
    OPENING,  // Positive polarity
    CLOSING,  // Negative polarity
    BRAKE     // Both high or both low
};

/**
 * @brief Pattern tracking for buzzer or LED signals
 */
struct SignalPattern {
    bool isBlinking = false;           // True if signal is blinking (periodic)
    float frequencyHz = 0.0f;          // Blink frequency in Hz
    uint32_t periodMs = 0;             // Blink period in ms
    uint32_t onTimeMs = 0;             // Last measured on-time
    uint32_t offTimeMs = 0;            // Last measured off-time
    uint8_t dutyCycle = 0;             // Duty cycle 0-100%
    uint32_t cycleCount = 0;           // Number of complete cycles detected
    uint32_t totalOnTime = 0;          // Total time signal has been on
    uint32_t totalOffTime = 0;         // Total time signal has been off
};

/**
 * @brief Manual switch press type
 */
enum class SwitchPressType {
    NONE,
    SHORT,
    LONG
};

/**
 * @brief Manual switch emulation state
 */
struct ManualSwitchState {
    bool isPressed = false;
    SwitchPressType lastPressType = SwitchPressType::NONE;
    uint32_t pressStartTime = 0;       // When current press started
    uint32_t pressDuration = 0;        // Duration of current/last press
    uint32_t shortPressThresholdMs = DEFAULT_SHORT_PRESS_MS;
    uint32_t longPressThresholdMs = DEFAULT_LONG_PRESS_MS;
    uint32_t debounceMs = SWITCH_DEBOUNCE_MS;
    uint32_t autoReleaseTime = 0;      // When to auto-release (0 = manual release)
};

/**
 * @brief Monitored signal states from main controller
 */
struct MonitoredSignals {
    // Pump
    bool pumpActive = false;

    // Light
    bool lightActive = false;
    uint8_t lightBrightness = 0;  // 0-100% estimated from PWM

    // Door motor
    bool doorMotorPosActive = false;
    bool doorMotorNegActive = false;
    MotorDirection motorDirection = MotorDirection::STOPPED;

    // Buzzer
    bool buzzerActive = false;
    uint32_t buzzerOnDuration = 0;  // ms since buzzer turned on
    SignalPattern buzzerPattern;     // Pattern tracking for buzzer

    // WiFi LED
    bool wifiLedActive = false;
    uint32_t ledBlinkPeriod = 0;     // Estimated blink period in ms
    SignalPattern ledPattern;         // Pattern tracking for LED
};

/**
 * @brief Emulated output states sent to main controller
 */
struct EmulatedOutputs {
    // Water meter
    bool waterFlowEnabled = false;
    float flowRateGPM = DEFAULT_FLOW_RATE_GPM;
    uint32_t channel1PulseCount = 0;
    uint32_t channel2PulseCount = 0;

    // Door position
    DoorState doorState = DoorState::UNKNOWN;
    uint8_t doorPosition = 50;  // 0 = closed, 100 = open
    bool hallOpenActive = false;
    bool hallCloseActive = false;

    // Manual switch (enhanced state)
    ManualSwitchState manualSwitch;

    // Door fault
    bool doorFaultActive = false;
};

/**
 * @brief Emulator configuration settings
 */
struct EmulatorConfig {
    // Door
    uint32_t doorTravelTimeMs = DEFAULT_DOOR_TRAVEL_TIME_MS;
    bool autoSimulateDoor = true;  // Auto-respond to motor signals

    // Water
    float pulsesPerGallon = DEFAULT_PULSES_PER_GALLON;
    float flowRateGPM = DEFAULT_FLOW_RATE_GPM;
    bool autoGeneratePulses = true;  // Auto-pulse when pump running

    // Fault injection
    bool injectDoorFault = false;
    bool simulateFrozenLine = false;  // No water flow despite pump
    bool simulateDoorStuck = false;   // Hall sensors never trigger

    // Manual switch settings
    uint32_t shortPressMs = DEFAULT_SHORT_PRESS_MS;
    uint32_t longPressMs = DEFAULT_LONG_PRESS_MS;

    // Global manual override mode
    // When enabled, auto behaviors are bypassed and all outputs are directly controllable
    bool manualOverrideEnabled = false;

    // Manual override output states (only used when manualOverrideEnabled = true)
    bool overrideHallOpen = false;       // Force hall open sensor state
    bool overrideHallClose = false;      // Force hall close sensor state
    bool overrideDoorFault = false;      // Force door fault state
    bool overrideManualSwitch = false;   // Force manual switch state
    bool overrideWaterPulse1 = false;    // Force water pulse channel 1
    bool overrideWaterPulse2 = false;    // Force water pulse channel 2
};

/**
 * @brief Central state manager for the hardware emulator
 *
 * Tracks all monitored input signals from the main controller and
 * manages all emulated output signals sent back to the main controller.
 */
class EmulatorStateManager {
public:
    EmulatorStateManager() = default;

    /**
     * @brief Initialize GPIO pins and state
     */
    void begin();

    /**
     * @brief Update loop - call from main loop
     * Samples inputs, updates door simulation, generates pulses
     */
    void update();

    // ========================================================================
    // MONITORED SIGNAL ACCESS
    // ========================================================================

    const MonitoredSignals& getMonitoredSignals() const { return _monitored; }
    bool isPumpActive() const { return _monitored.pumpActive; }
    bool isLightActive() const { return _monitored.lightActive; }
    uint8_t getLightBrightness() const { return _monitored.lightBrightness; }
    MotorDirection getMotorDirection() const { return _monitored.motorDirection; }
    bool isBuzzerActive() const { return _monitored.buzzerActive; }
    bool isWifiLedActive() const { return _monitored.wifiLedActive; }

    // ========================================================================
    // EMULATED OUTPUT CONTROL
    // ========================================================================

    const EmulatedOutputs& getEmulatedOutputs() const { return _emulated; }

    // Door control
    void setDoorPosition(uint8_t position);
    void setDoorState(DoorState state);
    uint8_t getDoorPosition() const { return _emulated.doorPosition; }
    DoorState getDoorState() const { return _emulated.doorState; }

    // Water control
    void setWaterFlowEnabled(bool enabled);
    void setFlowRate(float gpm);
    void triggerSinglePulse(uint8_t channel);  // 1 or 2
    void resetPulseCounters();

    // Manual switch (enhanced)
    void pressManualSwitch();
    void releaseManualSwitch();
    void pulseManualSwitch(uint32_t durationMs = 200);
    void longPressManualSwitch(uint32_t durationMs = DEFAULT_LONG_PRESS_MS);
    bool isManualSwitchPressed() const { return _emulated.manualSwitch.isPressed; }
    SwitchPressType getLastPressType() const { return _emulated.manualSwitch.lastPressType; }
    uint32_t getCurrentPressDuration() const;
    void setManualSwitchThresholds(uint32_t shortMs, uint32_t longMs);

    // Pattern tracking accessors
    const SignalPattern& getBuzzerPattern() const { return _monitored.buzzerPattern; }
    const SignalPattern& getLedPattern() const { return _monitored.ledPattern; }

    // Manual override mode
    void setManualOverrideEnabled(bool enabled);
    bool isManualOverrideEnabled() const { return _config.manualOverrideEnabled; }
    void setOverrideHallOpen(bool state);
    void setOverrideHallClose(bool state);
    void setOverrideDoorFault(bool state);
    void setOverrideManualSwitch(bool state);
    void setOverrideWaterPulse(uint8_t channel, bool state);
    void clearAllOverrides();

    // Fault injection
    void setDoorFault(bool fault);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    EmulatorConfig& getConfig() { return _config; }
    void setConfig(const EmulatorConfig& config) { _config = config; }

    // ========================================================================
    // JSON SERIALIZATION
    // ========================================================================

    /**
     * @brief Serialize full state to JSON for API
     */
    void toJson(JsonObject& obj) const;

    /**
     * @brief Serialize only monitored signals
     */
    void monitoredToJson(JsonObject& obj) const;

    /**
     * @brief Serialize only emulated outputs
     */
    void emulatedToJson(JsonObject& obj) const;

private:
    MonitoredSignals _monitored;
    EmulatedOutputs _emulated;
    EmulatorConfig _config;

    // Timing
    uint32_t _lastSampleTime = 0;
    uint32_t _lastDoorUpdateTime = 0;
    uint32_t _lastPulseTime = 0;
    uint32_t _pulseStartTime = 0;
    bool _pulseInProgress = false;
    uint8_t _pulseChannel = 0;

    // PWM measurement
    uint32_t _lightPwmHighTime = 0;
    uint32_t _lightPwmLowTime = 0;
    uint32_t _lastLightChange = 0;
    bool _lastLightState = false;

    // Buzzer timing and pattern tracking
    uint32_t _buzzerOnTime = 0;
    uint32_t _buzzerOffTime = 0;
    bool _lastBuzzerState = false;
    uint32_t _buzzerLastTransition = 0;
    uint32_t _buzzerOnDurations[PATTERN_HISTORY_SIZE] = {0};
    uint32_t _buzzerOffDurations[PATTERN_HISTORY_SIZE] = {0};
    uint8_t _buzzerPatternIndex = 0;

    // LED timing and pattern tracking
    uint32_t _ledOnTime = 0;
    uint32_t _ledOffTime = 0;
    bool _lastLedState = false;
    uint32_t _ledLastTransition = 0;
    uint32_t _ledOnDurations[PATTERN_HISTORY_SIZE] = {0};
    uint32_t _ledOffDurations[PATTERN_HISTORY_SIZE] = {0};
    uint8_t _ledPatternIndex = 0;

    // Manual switch timing
    uint32_t _manualSwitchReleaseTime = 0;

    // Private methods
    void sampleInputs();
    void updateDoorSimulation();
    void updateWaterPulses();
    void updateHallSensors();
    void updateManualSwitch();
    void outputEmulatedSignals();

    // Pattern tracking helpers
    void updateBuzzerPattern(bool currentState);
    void updateLedPattern(bool currentState);
    void calculatePattern(SignalPattern& pattern, uint32_t* onDurations, uint32_t* offDurations, uint8_t count);

    // Helper for motor direction
    MotorDirection calculateMotorDirection(bool pos, bool neg);

    // Convert enum to string
    static const char* doorStateToString(DoorState state);
    static const char* motorDirectionToString(MotorDirection dir);
};

#endif // EMULATOR_STATE_MANAGER_H
