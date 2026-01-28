#ifndef LIGHTCONTROLLER_H
#define LIGHTCONTROLLER_H

#include "SunriseSunset.h"
#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>

// Light state enumeration
enum class LightState {
    OFF,
    ON,
    FADING_IN,
    FADING_OUT,
    FAULT
};

class LightController { // NOSONAR - complexity ok
private:
    IHAL* hal;
    SunriseSunsetCalculator* sunriseSunset;

    // State variables
    LightState currentState;
    unsigned long stateStartTime;
    bool autoMode;
    bool testMode;
    
    // PWM configuration
    static const int PWM_CHANNEL = 0;
    static const int PWM_FREQ = 1000;  // 1kHz for flicker-free
    static const int PWM_RESOLUTION = 8;  // 8-bit (0-255)
    
    // Brightness values
    int currentBrightness;  // 0-100%
    int targetBrightness;   // 0-100%
    int maxBrightness;      // User-configurable max (0-100%)
    
    // Fade configuration
    int transitionDurationMinutes;
    unsigned long fadeStartTime;
    int fadeStartBrightness;
    int fadeTargetBrightness;
    
    // Schedule configuration
    int onHour;
    int onMinute;
    String onMode;  // "fixed" or "sunset_offset"
    int onSunsetOffsetMinutes;
    int offHour;
    int sunriseOffsetMinutes;
    int sunsetOffsetMinutes;
    
    // Statistics
    unsigned long totalOnTime;
    unsigned long totalFadeInTime;
    unsigned long totalFadeOutTime;
    unsigned long totalCycles;
    
    // Internal methods
    void setState(LightState newState);
    void updatePWM();
    void updateFade();
    void checkSchedule();
    bool shouldTurnOnBySchedule() const;
    bool shouldTurnOffBySchedule() const;
    time_t getTodaySunrise() const;
    time_t getTodaySunset() const;
    int calculateSineWaveBrightness(float progress) const;
    String getStateStringForState(LightState state) const;
    
public:
    LightController();
    
    // Initialization
    void begin(IHAL* _hal, SunriseSunsetCalculator* sunriseSunset);
    
    // Main update loop - call frequently (recommended 100ms)
    void update();
    
    // Manual control
    void turnOn();
    void turnOff();
    void setBrightness(int percent);  // 0-100%
    void fadeIn();
    void fadeOut();
    
    // Automatic mode control
    void setAutoMode(bool enabled);
    bool isAutoMode() const;
    
    // Test mode for UI testing without hardware
    void setTestMode(bool enabled);
    bool isTestMode() const;
    
    // State getters
    LightState getState() const;
    String getStateString() const;
    int getCurrentBrightness() const;  // Returns 0-100%
    int getTargetBrightness() const;   // Returns 0-100%
    
    // Progress calculation for UI (fade progress)
    int getFadeProgressPercentage() const;
    
    // Configuration getters/setters
    int getMaxBrightness() const;
    void setMaxBrightness(int percent);
    int getTransitionDurationMinutes() const;
    void setTransitionDurationMinutes(int minutes);
    int getOnHour() const;
    void setOnHour(int hour);
    int getOnMinute() const;
    void setOnMinute(int minute);
    String getOnMode() const;
    void setOnMode(const String& mode);
    int getOnSunsetOffsetMinutes() const;
    void setOnSunsetOffsetMinutes(int minutes);
    int getOffHour() const;
    void setOffHour(int hour);
    int getSunriseOffsetMinutes() const;
    void setSunriseOffsetMinutes(int minutes);
    int getSunsetOffsetMinutes() const;
    void setSunsetOffsetMinutes(int minutes);
    
    // Statistics
    unsigned long getTotalOnTime() const;
    unsigned long getTotalFadeInTime() const;
    unsigned long getTotalFadeOutTime() const;
    unsigned long getTotalCycles() const;
    void resetStatistics();
    
    // Status for API
    void toJson(JsonObject& json) const;
    String getNextScheduledAction() const;
    
    // Fault handling
    bool hasFault() const;
    void clearFault();
};

#endif // LIGHTCONTROLLER_H