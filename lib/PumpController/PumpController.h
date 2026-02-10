#ifndef __PUMP_CONTROLLER_H__
#define __PUMP_CONTROLLER_H__

#include <Arduino.h>
#include "SensorManager.h"
#include "TriggerSource.h"
#include <stdint.h>

/**
 * @brief Pump operational states
 *
 * Enumeration of all possible pump states during operation.
 */
enum class PumpState {
    PUMP_OFF = 0,    ///< Pump is manually off
    PUMP_ON,         ///< Pump is manually on
    PUMP_AUTO,       ///< Pump is in automatic mode
    PUMP_ERROR       ///< Pump error state (flow error detected)
};

/**
 * @brief Complete pump status structure
 *
 * Contains all current pump information including state, timing,
 * temperature, error flags, and statistics.
 */
struct PumpStatus {
    PumpState state = PumpState::PUMP_AUTO;      ///< Current pump state
    bool is_active;                              ///< Is pump currently running
    unsigned long last_switch_time;              ///< Timestamp of last state change
    unsigned long current_cycle_start;           ///< When current cycle started
    int current_cycle_duration;                  ///< Duration of current cycle
    float temperature_f;                         ///< Current temperature reading
    bool temperature_below_threshold;            ///< Is temp below on threshold
    bool flow_error;                             ///< Flow error detected flag
    unsigned long time_until_retry;              ///< Time until next retry after error
    unsigned long total_on_time;                 ///< Cumulative pump on time
    unsigned long total_off_time;                ///< Cumulative pump off time
    unsigned long total_cycles;                  ///< Number of on/off cycles
    bool pump_off_flow_detected;                 ///< Flow detected when pump should be off
    bool scheduled_cycle_active;                 ///< Scheduled maintenance cycle running
    unsigned long time_until_next_scheduled;     ///< Time until next scheduled cycle (ms)
};

/**
 * @brief Water pump controller for chicken coop
 *
 * Controls a water pump with automatic cycling based on temperature
 * and flow monitoring. Features include:
 *
 * - Temperature-based automatic cycling
 * - Flow error detection (pump on but no flow)
 * - Pump-off flow detection (flow when pump should be off)
 * - Configurable on/off times
 * - Statistics tracking (on time, off time, cycles)
 * - Error state with automatic retry
 * - Buzzer alerts for flow errors
 *
 * Temperature Control:
 * - Pump cycles ON when temperature drops below threshold
 * - Pump cycles OFF when temperature rises above threshold
 * - Configurable on/off durations for cycling
 *
 * Flow Monitoring:
 * - Detects pump failure (running but no water flow)
 * - Detects leaks/ghost flow (flow when pump is off)
 * - Grace period after pump off to allow water to settle
 *
 * Error Handling:
 * - Enters ERROR state on flow error
 * - Automatic retry after timeout period
 * - Manual error clearing available
 */
class PumpController {
private:
    uint8_t pumpPin;                          ///< GPIO pin for pump control
    PumpStatus status;                        ///< Current pump status

    // Sensor references
    SensorManager* primarySensor_;    ///< Primary sensor for temperature reading
    SensorManager* flowSensor_;       ///< Sensor for flow detection

    // Timing variables
    unsigned long lastUpdateTime;            ///< Last time update() was called
    unsigned long cycleStartTime;            ///< When current cycle phase started
    bool cyclingActive;                      ///< Has cycling started (handles time=0 case)
    bool currentlyInOnPhase;                 ///< Are we currently in ON phase
    unsigned long offPhaseStartTime;         ///< When OFF phase started

    // Error detection
    unsigned long lastFlowCheckTime;          ///< Last time flow was checked
    unsigned long errorStartTime;            ///< When error state started
    bool waitingForRetry;                    ///< Waiting for retry after error

    // Pump off flow monitoring
    bool pump_off_flow_monitoring_enabled;   ///< Enable pump OFF flow monitoring
    int pump_off_flow_grace_period_seconds;  ///< Grace period after pump turns off
    unsigned long pump_turned_off_time;      ///< Timestamp when pump last turned off
    bool pump_has_been_off;                  ///< Has pump been off yet (handles time=0 case)
    bool pump_off_flow_detected;             ///< Flow detected when pump should be off

    // Scheduled maintenance cycles
    unsigned long lastCompletedCycleTime_;   ///< When last pump cycle completed (any type)
    bool scheduledCycleActive_;              ///< Is a scheduled maintenance cycle running
    unsigned long scheduledCycleStartTime_;  ///< When current scheduled cycle started

    // Trigger source tracking
    TriggerSource lastTriggerSource_;        ///< What triggered the last state change

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Set pump output state
     *
     * @param isOn true to turn pump on, false to turn off
     */
    void setPumpState(bool isOn);

    /**
     * @brief Update pump statistics
     *
     * Accumulates on/off time and cycle counters.
     */
    void updateStatistics();

    /**
     * @brief Handle automatic mode operation
     *
     * Manages temperature-based cycling and flow error detection.
     *
     * @param currentTime Current timestamp from millis()
     */
    void handleAutoMode(unsigned long currentTime);

    /**
     * @brief Check for flow error
     *
     * Detects if pump is running but no water flow is detected.
     *
     * @return true if flow error detected
     */
    bool checkFlowError() const;

    /**
     * @brief Check for flow when pump should be off
     *
     * Detects ghost flow or leaks when pump is off.
     *
     * @param currentTime Current timestamp from millis()
     */
    void checkPumpOffFlow(unsigned long currentTime);

    /**
     * @brief Handle scheduled maintenance pump cycles
     *
     * Runs pump at regular intervals to prevent water stagnation.
     * Temperature-triggered cycles count toward the minimum.
     *
     * @param currentTime Current timestamp from millis()
     */
    void handleScheduledCycles(unsigned long currentTime);

public:
    /**
     * @brief Default constructor
     *
     * Initializes pump controller in default state.
     * Must call begin() before use.
     */
    PumpController() = default;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize pump controller
     *
     * Configures GPIO pin and sensor references.
     * Primary sensor used for temperature, flow sensor for flow detection.
     *
     * @param primarySensor Sensor for temperature readings
     * @param flowSensor Sensor for flow detection (can be same as primary)
     * @param pin GPIO pin for pump control
     */
    void begin(SensorManager* primarySensor, SensorManager* flowSensor, uint8_t pin);

    // ========================================================================
    // MAIN UPDATE LOOP
    // ========================================================================

    /**
     * @brief Update pump controller state (call in loop)
     *
     * Handles automatic cycling, flow error detection, and statistics.
     * Should be called every loop iteration.
     */
    void update();

    // ========================================================================
    // CONTROL METHODS
    // ========================================================================

    /**
     * @brief Turn pump on manually
     *
     * Switches to PUMP_ON state regardless of temperature.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void turnOn(TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Turn pump off manually
     *
     * Switches to PUMP_OFF state regardless of temperature.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void turnOff(TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Enable or disable automatic mode
     *
     * In auto mode, pump cycles based on temperature thresholds.
     *
     * @param enabled true to enable auto mode
     * @param trigger What triggered this action (default: MANUAL)
     */
    void setAutoMode(bool enabled, TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Force a single pump cycle
     *
     * Runs one on/off cycle regardless of temperature.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void forceCycle(TriggerSource trigger = TriggerSource::WEB_UI);

    // ========================================================================
    // STATUS METHODS
    // ========================================================================

    /**
     * @brief Get complete pump status
     *
     * @return Current PumpStatus structure
     */
    PumpStatus getStatus() const { return status; }

    /**
     * @brief Check if pump is currently running
     *
     * @return true if pump is on
     */
    bool isPumpOn() const { return status.is_active; }

    /**
     * @brief Get current pump state
     *
     * @return Current PumpState
     */
    PumpState getState() const { return status.state; }

    /**
     * @brief Get current temperature reading
     *
     * @return Temperature in Fahrenheit
     */
    float getCurrentTemperature() const { return status.temperature_f; }

    /**
     * @brief Check if flow error is detected
     *
     * @return true if pump has flow error
     */
    bool hasFlowError() const { return status.flow_error; }

    /**
     * @brief Check if pump-off flow is detected
     *
     * @return true if flow detected when pump should be off
     */
    bool getPumpOffFlowDetected() const { return status.pump_off_flow_detected; }

    /**
     * @brief Check if scheduled maintenance cycle is active
     *
     * @return true if a scheduled cycle is currently running
     */
    bool isScheduledCycleActive() const { return status.scheduled_cycle_active; }

    /**
     * @brief Get time until next scheduled cycle
     *
     * @return Milliseconds until next scheduled cycle (0 if disabled or cycling)
     */
    unsigned long getTimeUntilNextScheduledCycle() const { return status.time_until_next_scheduled; }

    /**
     * @brief Get timestamp of current run start
     *
     * @return Timestamp when current run started
     */
    unsigned long getCurrentRunStartTime() const;

    /**
     * @brief Get last trigger source
     *
     * @return TriggerSource that caused the last state change
     */
    TriggerSource getLastTriggerSource() const { return lastTriggerSource_; }

    /**
     * @brief Get last trigger source as string
     *
     * @return String representation of last trigger source
     */
    String getLastTriggerSourceString() const { return triggerSourceToString(lastTriggerSource_); }

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get total pump on time
     *
     * @return Total on time in milliseconds
     */
    unsigned long getTotalOnTime() const { return status.total_on_time; }

    /**
     * @brief Get total pump off time
     *
     * @return Total off time in milliseconds
     */
    unsigned long getTotalOffTime() const { return status.total_off_time; }

    /**
     * @brief Get total number of cycles
     *
     * @return Total cycle count
     */
    unsigned long getTotalCycles() const { return status.total_cycles; }

    /**
     * @brief Get current cycle duration
     *
     * @return Duration of current cycle in milliseconds
     */
    unsigned long getCurrentCycleTime() const;

    /**
     * @brief Get time until next auto cycle switch
     *
     * @return Milliseconds until next on/off switch
     */
    unsigned long getTimeUntilNextSwitch() const;

    /**
     * @brief Get time until error retry
     *
     * @return Milliseconds until retry attempt (0 if not in error)
     */
    unsigned long getTimeUntilRetry() const { return status.time_until_retry; }

    // ========================================================================
    // STATUS STRINGS
    // ========================================================================

    /**
     * @brief Get state as human-readable string
     *
     * @return String representation of current state
     */
    String getStateString() const;

    /**
     * @brief Get status as JSON string
     *
     * @return JSON string containing complete pump status
     */
    String getStatusJson() const;

    // ========================================================================
    // RESET METHODS
    // ========================================================================

    /**
     * @brief Reset all statistics to zero
     *
     * Clears cumulative timing and cycle counters.
     */
    void resetStatistics();

    /**
     * @brief Clear flow error and resume operation
     *
     * Resets flow error flag and allows pump to operate.
     */
    void clearFlowError();

    /**
     * @brief Clear pump-off flow detection flag
     *
     * Resets the ghost flow/leak detection flag.
     */
    void clearPumpOffFlowDetected();
};

#endif // __PUMP_CONTROLLER_H__