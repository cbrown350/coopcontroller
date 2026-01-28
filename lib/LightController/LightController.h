#ifndef LIGHTCONTROLLER_H
#define LIGHTCONTROLLER_H

#include "SunriseSunset.h"
#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Light controller operational states
 *
 * Enumeration of all possible light states during operation.
 * Includes smooth fading transitions using sine wave calculation.
 */
enum class LightState {
    OFF,        ///< Light is off
    ON,         ///< Light is on at steady brightness
    FADING_IN,  ///< Light is currently fading in
    FADING_OUT, ///< Light is currently fading out
    FAULT       ///< Light fault detected
};

/**
 * @brief PWM light controller with smooth fading transitions
 *
 * Controls a PWM dimmable light for chicken coop with smooth sine-wave
 * fading transitions. Features include:
 *
 * - Smooth sine-wave fading transitions (configurable duration)
 * - Automatic scheduling based on time or sunset offset
 * - PWM brightness control (0-100%)
 * - Maximum brightness limit
 * - Statistics tracking (on time, fade time, cycles)
 * - Test mode for UI testing without hardware
 * - Fault detection
 *
 * Hardware:
 * - Uses ESP32 LEDC PWM on channel 0
 * - 1kHz frequency for flicker-free operation
 * - 8-bit resolution (256 levels)
 *
 * Fading:
 * - Uses sine wave calculation for natural transitions
 * - Configurable transition duration in minutes
 * - Smooth brightness changes reduce stress on chickens
 */
class LightController { // NOSONAR - complexity ok
private:
    IHAL* hal;                        ///< Hardware abstraction layer
    SunriseSunsetCalculator* sunriseSunset; ///< Sunrise/sunset calculator

    // State variables
    LightState currentState;           ///< Current operational state
    unsigned long stateStartTime;      ///< Timestamp when current state started
    bool autoMode;                     ///< Automatic scheduling enabled
    bool testMode;                     ///< Test mode (no hardware control)

    // PWM configuration
    static const int PWM_CHANNEL = 0;  ///< LEDC PWM channel
    static const int PWM_FREQ = 1000;  ///< PWM frequency in Hz (flicker-free)
    static const int PWM_RESOLUTION = 8; ///< PWM resolution bits (0-255)

    // Brightness values
    int currentBrightness;             ///< Current brightness level (0-100%)
    int targetBrightness;              ///< Target brightness for fade (0-100%)
    int maxBrightness;                 ///< Maximum user-configurable brightness (0-100%)

    // Fade configuration
    int transitionDurationMinutes;     ///< Duration of fade transitions
    unsigned long fadeStartTime;       ///< Timestamp when fade started
    int fadeStartBrightness;           ///< Brightness at fade start
    int fadeTargetBrightness;          ///< Target brightness for fade

    // Schedule configuration
    int onHour;                        ///< Hour to turn on light (24-hour format)
    int onMinute;                      ///< Minute to turn on light (0-59)
    String onMode;                     ///< "fixed" or "sunset_offset"
    int onSunsetOffsetMinutes;         ///< Minutes relative to sunset for on time
    int offHour;                       ///< Hour to turn off light (24-hour format)
    int sunriseOffsetMinutes;          ///< Minutes relative to sunrise
    int sunsetOffsetMinutes;           ///< Minutes relative to sunset

    // Statistics
    unsigned long totalOnTime;         ///< Cumulative time light is on
    unsigned long totalFadeInTime;     ///< Cumulative fade-in time
    unsigned long totalFadeOutTime;    ///< Cumulative fade-out time
    unsigned long totalCycles;         ///< Number of on/off cycles

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Change light state and update timing
     *
     * @param newState New LightState to transition to
     */
    void setState(LightState newState);

    /**
     * @brief Update PWM output based on current brightness
     *
     * Converts brightness percentage to PWM duty cycle and writes to LEDC.
     */
    void updatePWM();

    /**
     * @brief Update fade transition progress
     *
     * Calculates current brightness based on sine wave fade progress.
     * Updates currentBrightness and updates PWM output.
     */
    void updateFade();

    /**
     * @brief Check if light should change based on schedule
     *
     * Compares current time with on/off schedule and triggers transitions.
     */
    void checkSchedule();

    /**
     * @brief Check if light should turn on by schedule
     *
     * @return true if current time matches on schedule
     */
    bool shouldTurnOnBySchedule() const;

    /**
     * @brief Check if light should turn off by schedule
     *
     * @return true if current time matches off schedule
     */
    bool shouldTurnOffBySchedule() const;

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

    /**
     * @brief Calculate sine wave brightness for fade
     *
     * Uses sine wave formula for natural transitions.
     *
     * @param progress Fade progress (0.0 to 1.0)
     * @return Brightness value (0-100)
     */
    int calculateSineWaveBrightness(float progress) const;

    /**
     * @brief Convert LightState to string
     *
     * @param state LightState to convert
     * @return String representation of state
     */
    String getStateStringForState(LightState state) const;
    
public:
    /**
     * @brief Default constructor
     *
     * Initializes light controller in default state.
     * Must call begin() before use.
     */
    LightController();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize light controller
     *
     * Sets up PWM channel and stores references to HAL and sunrise/sunset calculator.
     *
     * @param _hal Pointer to hardware abstraction layer
     * @param sunriseSunset Pointer to sunrise/sunset calculator
     */
    void begin(IHAL* _hal, SunriseSunsetCalculator* sunriseSunset);

    // ========================================================================
    // MAIN UPDATE LOOP
    // ========================================================================

    /**
     * @brief Update light controller state (call frequently)
     *
     * Handles fade transitions, schedule checking, and PWM updates.
     * Recommended call interval: 100ms
     */
    void update();

    // ========================================================================
    // MANUAL CONTROL
    // ========================================================================

    /**
     * @brief Turn light on at max brightness
     *
     * Immediately sets light to maximum configured brightness.
     */
    void turnOn();

    /**
     * @brief Turn light off
     *
     * Immediately turns off the light.
     */
    void turnOff();

    /**
     * @brief Set light brightness percentage
     *
     * @param percent Brightness level (0-100)
     */
    void setBrightness(int percent);

    /**
     * @brief Start fade-in transition
     *
     * Smoothly fades light from current to max brightness.
     */
    void fadeIn();

    /**
     * @brief Start fade-out transition
     *
     * Smoothly fades light from current to off.
     */
    void fadeOut();

    // ========================================================================
    // AUTOMATIC MODE CONTROL
    // ========================================================================

    /**
     * @brief Enable or disable automatic mode
     *
     * When enabled, light automatically turns on/off based on schedule.
     *
     * @param enabled true to enable auto mode, false to disable
     */
    void setAutoMode(bool enabled);

    /**
     * @brief Check if automatic mode is enabled
     *
     * @return true if auto mode is enabled
     */
    bool isAutoMode() const;

    // ========================================================================
    // TEST MODE
    // ========================================================================

    /**
     * @brief Enable or disable test mode
     *
     * Test mode allows UI testing without hardware.
     * PWM outputs are not activated in test mode.
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
     * @brief Get current light state
     *
     * @return Current LightState
     */
    LightState getState() const;

    /**
     * @brief Get state as human-readable string
     *
     * @return String representation of current state
     */
    String getStateString() const;

    /**
     * @brief Get current brightness percentage
     *
     * @return Current brightness (0-100)
     */
    int getCurrentBrightness() const;

    /**
     * @brief Get target brightness percentage
     *
     * @return Target brightness during fade (0-100)
     */
    int getTargetBrightness() const;

    /**
     * @brief Get fade transition progress percentage
     *
     * @return Fade progress (0-100) or 0 if not fading
     */
    int getFadeProgressPercentage() const;

    // ========================================================================
    // CONFIGURATION GETTERS/SETTERS
    // ========================================================================

    /**
     * @brief Get maximum brightness setting
     *
     * @return Maximum brightness percentage (0-100)
     */
    int getMaxBrightness() const;

    /**
     * @brief Set maximum brightness
     *
     * @param percent Maximum brightness (0-100)
     */
    void setMaxBrightness(int percent);

    /**
     * @brief Get fade transition duration
     *
     * @return Transition duration in minutes
     */
    int getTransitionDurationMinutes() const;

    /**
     * @brief Set fade transition duration
     *
     * @param minutes Transition duration in minutes
     */
    void setTransitionDurationMinutes(int minutes);

    /**
     * @brief Get on schedule hour (fixed mode)
     *
     * @return Hour (0-23)
     */
    int getOnHour() const;

    /**
     * @brief Set on schedule hour (fixed mode)
     *
     * @param hour Hour (0-23)
     */
    void setOnHour(int hour);

    /**
     * @brief Get on schedule minute (fixed mode)
     *
     * @return Minute (0-59)
     */
    int getOnMinute() const;

    /**
     * @brief Set on schedule minute (fixed mode)
     *
     * @param minute Minute (0-59)
     */
    void setOnMinute(int minute);

    /**
     * @brief Get on schedule mode
     *
     * @return "fixed" or "sunset_offset"
     */
    String getOnMode() const;

    /**
     * @brief Set on schedule mode
     *
     * @param mode "fixed" or "sunset_offset"
     */
    void setOnMode(const String& mode);

    /**
     * @brief Get sunset offset for on time
     *
     * @return Minutes relative to sunset (positive = after)
     */
    int getOnSunsetOffsetMinutes() const;

    /**
     * @brief Set sunset offset for on time
     *
     * @param minutes Minutes relative to sunset (positive = after)
     */
    void setOnSunsetOffsetMinutes(int minutes);

    /**
     * @brief Get off schedule hour
     *
     * @return Hour (0-23)
     */
    int getOffHour() const;

    /**
     * @brief Set off schedule hour
     *
     * @param hour Hour (0-23)
     */
    void setOffHour(int hour);

    /**
     * @brief Get sunrise offset
     *
     * @return Minutes relative to sunrise
     */
    int getSunriseOffsetMinutes() const;

    /**
     * @brief Set sunrise offset
     *
     * @param minutes Minutes relative to sunrise
     */
    void setSunriseOffsetMinutes(int minutes);

    /**
     * @brief Get sunset offset
     *
     * @return Minutes relative to sunset
     */
    int getSunsetOffsetMinutes() const;

    /**
     * @brief Set sunset offset
     *
     * @param minutes Minutes relative to sunset
     */
    void setSunsetOffsetMinutes(int minutes);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get total time light has been on
     *
     * @return Total on time in milliseconds
     */
    unsigned long getTotalOnTime() const;

    /**
     * @brief Get total fade-in time
     *
     * @return Total fade-in time in milliseconds
     */
    unsigned long getTotalFadeInTime() const;

    /**
     * @brief Get total fade-out time
     *
     * @return Total fade-out time in milliseconds
     */
    unsigned long getTotalFadeOutTime() const;

    /**
     * @brief Get total number of on/off cycles
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
     * @brief Serialize light status to JSON
     *
     * Populates JsonObject with current light state, brightness,
     * configuration, and statistics.
     *
     * @param json JsonObject to populate
     */
    void toJson(JsonObject& json) const;

    /**
     * @brief Get next scheduled action as string
     *
     * Returns human-readable description of next scheduled action.
     *
     * @return String describing next scheduled action
     */
    String getNextScheduledAction() const;

    // ========================================================================
    // FAULT HANDLING
    // ========================================================================

    /**
     * @brief Check if light is in fault state
     *
     * @return true if current state is FAULT
     */
    bool hasFault() const;

    /**
     * @brief Clear fault state and return to OFF
     *
     * Resets light to normal operation after fault.
     */
    void clearFault();
};

#endif // LIGHTCONTROLLER_H