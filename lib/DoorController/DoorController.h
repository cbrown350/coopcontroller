#ifndef DOORCONTROLLER_H
#define DOORCONTROLLER_H

#include "BuzzerController.h"
#include "SunriseSunset.h"
#include "TriggerSource.h"
#include "WeatherManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>

/**
 * @brief Door operational states
 *
 * Enumeration of all possible door states during operation.
 * States progress through opening/closing transitions with timeout protection.
 */
enum class DoorState {
    IDLE,       ///< Door is stationary and fully open or closed
    OPENING,    ///< Door is currently opening (motor running)
    OPEN,       ///< Door is fully open (hall sensor triggered)
    CLOSING,    ///< Door is currently closing (motor running)
    CLOSED,     ///< Door is fully closed (hall sensor triggered)
    FAULT       ///< Door fault detected (timeout or sensor error)
};

/**
 * @brief Door position states
 *
 * Physical position of the door as detected by hall sensors.
 * Used for UI display and fault detection.
 */
enum class DoorPosition {
    UNKNOWN,    ///< Position cannot be determined
    OPEN,       ///< Hall open sensor triggered
    CLOSED,     ///< Hall closed sensor triggered
    PARTIAL     ///< Neither sensor triggered (door is partially open/closed)
};

/**
 * @brief Automatic chicken coop door controller
 *
 * Controls a motorized chicken coop door with automatic opening/closing
 * based on sunrise/sunset times. Features include:
 *
 * - Automatic scheduling based on sunrise/sunset with configurable offsets
 * - Hall sensor position detection (open/closed)
 * - Timeout protection for fault detection
 * - Manual control via switch or API
 * - Buzzer alerts for faults
 * - Statistics tracking (cycles, timing)
 * - Position memory across reboots
 * - Test mode for UI testing without hardware
 *
 * Motor Control:
 * - Uses H-bridge motor driver with two direction pins
 * - Open: OPEN_POS=HIGH, OPEN_NEG=LOW
 * - Close: OPEN_POS=LOW, OPEN_NEG=HIGH
 * - Stop: Both pins LOW
 *
 * Safety Features:
 * - Automatic stop on hall sensor trigger
 * - Timeout detection (configurable)
 * - Fault state on sensor errors
 * - Buzzer alerts on faults
 */
class DoorController { // NOSONAR - complexity ok
private:
    BuzzerController* buzzerController = nullptr;     ///< Buzzer for fault alerts
    SunriseSunsetCalculator* sunriseSunset = nullptr; ///< Sunrise/sunset time calculator
    WeatherManager* weatherManager = nullptr;         ///< Optional weather gate for auto-open

    // State variables
    DoorState currentState;         ///< Current operational state
    DoorPosition currentPosition;   ///< Physical door position
    unsigned long stateStartTime;   ///< Timestamp when current state started
    bool autoOpenEnabled;           ///< Automatic opening by schedule enabled
    bool autoCloseEnabled;          ///< Automatic closing by schedule enabled
    bool testMode;                  ///< Test mode (no hardware control)

    // Last movement direction for manual switch reversal
    DoorState lastMovementDirection; ///< Direction door was last moving

    // Manual switch debouncing
    unsigned long lastSwitchCheck;  ///< Last time manual switch was checked
    bool lastSwitchState;           ///< Previous state of manual switch
    static const int switchDebounceMs = 50; ///< Debounce delay in milliseconds

    // Configuration
    unsigned int openTimeoutSeconds;   ///< Maximum time to wait for open (default: 30)
    unsigned int closeTimeoutSeconds;  ///< Maximum time to wait for close (default: 30)
    int autoOpenOffsetMinutes;        ///< Minutes after (+) / before (-) sunrise to open door
    int autoCloseOffsetMinutes;       ///< Minutes after (+) / before (-) sunset to close door
    std::array<bool, 7> autoOpenDays;  ///< Days-of-week to auto-open (0=Sun..6=Sat)
    std::array<bool, 7> autoCloseDays; ///< Days-of-week to auto-close (0=Sun..6=Sat)
    bool lockoutEnabled;              ///< Prevents all door operations when true

    // Weather-gated auto-open. When a WeatherManager is attached and reports
    // inclement weather at auto-open time, opening is postponed and rechecked
    // roughly hourly instead of every loop (avoids log spam / API pressure).
    unsigned long weatherPostponeUntilMs; ///< millis() before which we skip the weather recheck (0 = check now)
    bool weatherPostponedOpen;            ///< True while an auto-open is being held back by weather

    // Timeout auto-calculation
    static const int MAX_TIMING_HISTORY = 10; ///< Max entries in timing history
    std::array<unsigned long, MAX_TIMING_HISTORY> openTimingHistory;   ///< History of open durations (ms)
    std::array<unsigned long, MAX_TIMING_HISTORY> closeTimingHistory;  ///< History of close durations (ms)
    int openTimingIndex;              ///< Next write index for open history (circular)
    int closeTimingIndex;             ///< Next write index for close history (circular)
    int openTimingCount;              ///< Total entries recorded for open
    int closeTimingCount;             ///< Total entries recorded for close
    bool autoCalcTimeoutEnabled;      ///< Auto-update timeouts from history

    // Statistics
    unsigned long totalOpenTime;   ///< Cumulative time spent opening
    unsigned long totalCloseTime;  ///< Cumulative time spent closing
    unsigned long totalCycles;     ///< Number of complete open/close cycles

    // Trigger source tracking
    TriggerSource lastTriggerSource_;  ///< What triggered the last state change

    // Static instance for ISR access
    static DoorController* instance; ///< Static pointer for interrupt handlers

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Change door state and update timing
     *
     * Updates current state, records timestamp, and logs the transition.
     *
     * @param newState New DoorState to transition to
     */
    void setState(DoorState newState);

    /**
     * @brief Set motor output pins for direction control
     *
     * Controls H-bridge motor driver pins.
     *
     * @param openPositive State for OPEN_POS pin
     * @param openNegative State for OPEN_NEG pin
     */
    void setMotorOutputs(bool openPositive, bool openNegative);

    /**
     * @brief Update door position based on hall sensors
     *
     * Reads hall sensor pins and updates currentPosition.
     * Triggers buzzer alert if both sensors active (fault condition).
     */
    void updatePosition();

    /**
     * @brief Check and debounce manual switch input
     *
     * Reads manual switch with debouncing and triggers open/close/stop.
     * Implements direction reversal logic for momentary switch.
     */
    void checkManualSwitch();

    /**
     * @brief Check for operation timeout and set fault if needed
     *
     * If door has been opening/closing too long, enters FAULT state.
     */
    void checkTimeout();

    /**
     * @brief Check if door should auto-open based on schedule
     *
     * Compares current time with sunrise + open offset. Only triggers
     * when auto-open is enabled and today is an enabled day-of-week.
     */
    void checkAutoOpenSchedule();

    /**
     * @brief Check if door should auto-close based on schedule
     *
     * Compares current time with sunset + close offset. Only triggers
     * when auto-close is enabled and today is an enabled day-of-week.
     */
    void checkAutoCloseSchedule();

    /**
     * @brief Get today's day-of-week index (0=Sun..6=Sat)
     *
     * Uses local time so the schedule matches the user's timezone.
     *
     * @return Day index 0-6, or -1 if time unavailable
     */
    int getTodayDayOfWeek() const;

    /**
     * @brief Get current local time in minutes since midnight
     *
     * Converts UTC system time to local time using timezone offset.
     * ESP32 system clock runs in UTC; sunrise/sunset are in local time.
     *
     * @return Minutes since midnight in local time, or -1 on error
     */
    int getCurrentLocalMinutes() const;

    /**
     * @brief Check if door should open by schedule
     *
     * @return true if current time is past sunrise + offset
     */
    bool shouldOpenBySchedule() const;

    /**
     * @brief Check if door should close by schedule
     *
     * @return true if current time is past sunset + offset
     */
    bool shouldCloseBySchedule() const;

    /**
     * @brief Get today's sunrise time as time_t
     *
     * @return Sunrise timestamp for today
     */
    time_t getTodaySunrise() const;

    /**
     * @brief Get today's sunset time as time_t
     *
     * @return Sunset timestamp for today
     */
    time_t getTodaySunset() const;

    // ========================================================================
    // ISR-SAFE METHODS
    // ========================================================================

    /**
     * @brief Handle hall open sensor interrupt
     *
     * Called from ISR when hall open sensor triggers.
     * Stops motor and sets position to OPEN.
     */
    void handleHallOpenISR();

    /**
     * @brief Handle hall closed sensor interrupt
     *
     * Called from ISR when hall closed sensor triggers.
     * Stops motor and sets position to CLOSED.
     */
    void handleHallClosedISR();

public:
    /**
     * @brief Default constructor
     *
     * Initializes door controller in default state.
     * Must call begin() before use.
     */
    DoorController();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize door controller
     *
     * Configures GPIO pins and sets up interrupt handlers.
     * Stores references to buzzer and sunrise/sunset calculator.
     *
     * @param buzzerController Pointer to buzzer controller for alerts
     * @param sunriseSunset Pointer to sunrise/sunset calculator
     */
    void begin(BuzzerController* buzzerController, SunriseSunsetCalculator* sunriseSunset);

    /**
     * @brief Attach an optional weather gate for automatic opening
     *
     * When set and the WeatherManager's gate is active, auto-open is held
     * back during inclement weather and rechecked about once an hour until
     * either the weather clears or the daily open window ends. Passing
     * nullptr disables weather gating (schedule-only behavior).
     *
     * @param weatherManager Pointer to WeatherManager (may be nullptr)
     */
    void setWeatherManager(WeatherManager* weatherManager);

    /**
     * @brief Check if an auto-open is currently postponed due to weather
     *
     * @return true if weather is holding back a scheduled open right now
     */
    bool isWeatherPostponed() const;

    // ========================================================================
    // MAIN UPDATE LOOP
    // ========================================================================

    /**
     * @brief Update door controller state (call frequently)
     *
     * Handles all door operations including:
     * - Manual switch checking
     * - Timeout detection
     * - Schedule checking
     * - Position updates
     *
     * Recommended call interval: 100ms
     */
    void update();

    // ========================================================================
    // MANUAL CONTROL
    // ========================================================================

    /**
     * @brief Start opening the door
     *
     * Activates motor to open door. Stops when hall open sensor triggers.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void open(TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Start closing the door
     *
     * Activates motor to close door. Stops when hall closed sensor triggers.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void close(TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Stop door movement
     *
     * Immediately stops the motor. Door remains in current position.
     *
     * @param trigger What triggered this action (default: MANUAL)
     */
    void stop(TriggerSource trigger = TriggerSource::WEB_UI);

    // ========================================================================
    // AUTOMATIC MODE CONTROL
    // ========================================================================

    /**
     * @brief Enable or disable automatic opening
     *
     * When enabled, door automatically opens at sunrise + open offset
     * on the configured days-of-week.
     *
     * @param enabled true to enable auto-open
     * @param trigger What triggered this action (default: WEB_UI)
     */
    void setAutoOpenEnabled(bool enabled, TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Enable or disable automatic closing
     *
     * When enabled, door automatically closes at sunset + close offset
     * on the configured days-of-week.
     *
     * @param enabled true to enable auto-close
     * @param trigger What triggered this action (default: WEB_UI)
     */
    void setAutoCloseEnabled(bool enabled, TriggerSource trigger = TriggerSource::WEB_UI);

    /**
     * @brief Check if any automatic scheduling is enabled
     *
     * Convenience for status/MQTT: true if auto-open OR auto-close is enabled.
     *
     * @return true if any automatic mode is enabled
     */
    bool isAutoMode() const;

    bool isAutoOpenEnabled() const;
    bool isAutoCloseEnabled() const;

    // ========================================================================
    // TEST MODE
    // ========================================================================

    /**
     * @brief Enable or disable test mode
     *
     * Test mode allows UI testing without hardware.
     * Motor outputs are not activated in test mode.
     *
     * @param enabled true to enable test mode
     */
    void setTestMode(bool enabled);

    /**
     * @brief Check if test mode is enabled
     *
     * @return true if test mode is enabled
     */
    bool isTestMode() const;

    // ========================================================================
    // STATE GETTERS
    // ========================================================================

    /**
     * @brief Get current door state
     *
     * @return Current DoorState
     */
    DoorState getState() const;

    /**
     * @brief Get current door position
     *
     * @return Current DoorPosition
     */
    DoorPosition getPosition() const;

    /**
     * @brief Get state as human-readable string
     *
     * @return String representation of current state
     */
    String getStateString() const;
    const char* getStateCStr() const;

    /**
     * @brief Get position as human-readable string
     *
     * @return String representation of current position
     */
    String getPositionString() const;
    const char* getPositionCStr() const;

    /**
     * @brief Get door operation progress percentage
     *
     * Calculates progress based on time in current state vs timeout.
     * Useful for UI progress bars.
     *
     * @return Progress percentage (0-100) or 0 if not moving
     */
    int getProgressPercentage() const;

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
    const char* getLastTriggerSourceCStr() const { return triggerSourceToCStr(lastTriggerSource_); }

    // ========================================================================
    // LOCKOUT CONTROL
    // ========================================================================

    /**
     * @brief Enable or disable door lockout
     *
     * When enabled, all door operations (open/close/manual switch/schedule)
     * are blocked. Useful for maintenance or manual intervention.
     *
     * @param enabled true to lock out door operations
     */
    void setLockoutEnabled(bool enabled);

    /**
     * @brief Check if door lockout is enabled
     *
     * @return true if lockout is active
     */
    bool isLockoutEnabled() const;

    // ========================================================================
    // TIMEOUT AUTO-CALCULATION
    // ========================================================================

    /**
     * @brief Get recommended open timeout based on history
     *
     * @return Recommended timeout in seconds (max historical + 1s buffer), or 0 if no history
     */
    unsigned int getRecommendedOpenTimeout() const;

    /**
     * @brief Get recommended close timeout based on history
     *
     * @return Recommended timeout in seconds (max historical + 1s buffer), or 0 if no history
     */
    unsigned int getRecommendedCloseTimeout() const;

    /**
     * @brief Enable or disable automatic timeout calculation
     *
     * When enabled, door timeouts are automatically updated based on
     * historical operation durations after each successful operation.
     *
     * @param enabled true to enable auto-calculation
     */
    void setAutoCalcTimeoutEnabled(bool enabled);

    /**
     * @brief Check if automatic timeout calculation is enabled
     *
     * @return true if auto-calculation is active
     */
    bool isAutoCalcTimeoutEnabled() const;

    /**
     * @brief Get number of recorded open timing entries
     *
     * @return Count of open timing history entries (0 to MAX_TIMING_HISTORY)
     */
    int getOpenTimingCount() const;

    /**
     * @brief Get number of recorded close timing entries
     *
     * @return Count of close timing history entries (0 to MAX_TIMING_HISTORY)
     */
    int getCloseTimingCount() const;

    // ========================================================================
    // CONFIGURATION GETTERS/SETTERS
    // ========================================================================

    /**
     * @brief Get door open timeout
     *
     * @return Timeout in seconds
     */
    unsigned int getOpenTimeoutSeconds() const;

    /**
     * @brief Set door open timeout
     *
     * @param seconds Timeout in seconds
     */
    void setOpenTimeoutSeconds(unsigned int seconds);

    /**
     * @brief Get door close timeout
     *
     * @return Timeout in seconds
     */
    unsigned int getCloseTimeoutSeconds() const;

    /**
     * @brief Set door close timeout
     *
     * @param seconds Timeout in seconds
     */
    void setCloseTimeoutSeconds(unsigned int seconds);

    /**
     * @brief Get auto-open offset (minutes after/before sunrise)
     *
     * @return Offset in minutes (positive = after sunrise, negative = before)
     */
    int getAutoOpenOffsetMinutes() const;

    /**
     * @brief Set auto-open offset (minutes after/before sunrise)
     *
     * @param minutes Offset in minutes (positive = after sunrise, negative = before)
     */
    void setAutoOpenOffsetMinutes(int minutes);

    /**
     * @brief Get auto-close offset (minutes after/before sunset)
     *
     * @return Offset in minutes (positive = after sunset, negative = before)
     */
    int getAutoCloseOffsetMinutes() const;

    /**
     * @brief Set auto-close offset (minutes after/before sunset)
     *
     * @param minutes Offset in minutes (positive = after sunset, negative = before)
     */
    void setAutoCloseOffsetMinutes(int minutes);

    /**
     * @brief Get whether auto-open is enabled for a given day-of-week
     *
     * @param dayIdx 0=Sun..6=Sat
     * @return true if auto-open is active on that day
     */
    bool getAutoOpenDay(int dayIdx) const;

    /**
     * @brief Enable/disable auto-open for a given day-of-week
     *
     * @param dayIdx 0=Sun..6=Sat
     * @param enabled true to enable auto-open on that day
     */
    void setAutoOpenDay(int dayIdx, bool enabled);

    /**
     * @brief Get whether auto-close is enabled for a given day-of-week
     *
     * @param dayIdx 0=Sun..6=Sat
     * @return true if auto-close is active on that day
     */
    bool getAutoCloseDay(int dayIdx) const;

    /**
     * @brief Enable/disable auto-close for a given day-of-week
     *
     * @param dayIdx 0=Sun..6=Sat
     * @param enabled true to enable auto-close on that day
     */
    void setAutoCloseDay(int dayIdx, bool enabled);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get total time spent opening door
     *
     * @return Total opening time in milliseconds
     */
    unsigned long getTotalOpenTime() const;

    /**
     * @brief Get total time spent closing door
     *
     * @return Total closing time in milliseconds
     */
    unsigned long getTotalCloseTime() const;

    /**
     * @brief Get total number of open/close cycles
     *
     * @return Total cycle count
     */
    unsigned long getTotalCycles() const;

    /**
     * @brief Reset all statistics to zero
     *
     * Clears cumulative timing and cycle counters.
     */
    void resetStatistics();

    // ========================================================================
    // STATUS FOR API
    // ========================================================================

    /**
     * @brief Serialize door status to JSON
     *
     * Populates JsonObject with current door state, position,
     * configuration, and statistics.
     *
     * @param json JsonObject to populate
     */
    void toJson(JsonObject& json) const;

    /**
     * @brief Get next scheduled action as string
     *
     * Returns human-readable description of next automatic action.
     *
     * @return String describing next scheduled action
     */
    String getNextScheduledAction() const;

    // ========================================================================
    // FAULT HANDLING
    // ========================================================================

    /**
     * @brief Check if door is in fault state
     *
     * @return true if current state is FAULT
     */
    bool hasFault() const;

    /**
     * @brief Clear fault state and return to IDLE
     *
     * Resets door to normal operation after fault.
     * Does not clear hardware sensor issues.
     */
    void clearFault();

    /**
     * @brief Check if fault is hardware-related
     *
     * Hardware faults indicate sensor issues vs timeout faults.
     *
     * @return true if fault is due to hardware/sensor issue
     */
    bool isHardwareFault() const;

    // ========================================================================
    // POSITION MEMORY
    // ========================================================================

    /**
     * @brief Save current position to persistent storage
     *
     * Stores door position for recovery after reboot.
     */
    void notifyPosition() const;

    /**
     * @brief Restore saved position from persistent storage
     *
     * Recovers door position after system restart.
     */
    void restorePosition();
};

#endif // DOORCONTROLLER_H