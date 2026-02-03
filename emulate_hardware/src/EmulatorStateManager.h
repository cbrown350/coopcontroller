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

    // WiFi LED
    bool wifiLedActive = false;
    uint32_t ledBlinkPeriod = 0;  // Estimated blink period in ms
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

    // Manual switch
    bool manualSwitchPressed = false;

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

    // Manual switch
    void pressManualSwitch();
    void releaseManualSwitch();
    void pulseManualSwitch(uint32_t durationMs = 200);

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

    // Buzzer timing
    uint32_t _buzzerOnTime = 0;

    // Manual switch timing
    uint32_t _manualSwitchReleaseTime = 0;

    // Private methods
    void sampleInputs();
    void updateDoorSimulation();
    void updateWaterPulses();
    void updateHallSensors();
    void updateManualSwitch();
    void outputEmulatedSignals();

    // Helper for motor direction
    MotorDirection calculateMotorDirection(bool pos, bool neg);

    // Convert enum to string
    static const char* doorStateToString(DoorState state);
    static const char* motorDirectionToString(MotorDirection dir);
};

#endif // EMULATOR_STATE_MANAGER_H
