#include "LightController.h"
#include "SettingsManager.h"
#include "Logger.h"
#include "SunriseSunset.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

// External instances
extern SunriseSunsetCalculator sunriseSunset;

LightController::LightController() :
    currentState(LightState::OFF),
    stateStartTime(0),
    autoMode(false),
    testMode(false),
    currentBrightness(0),
    targetBrightness(0),
    maxBrightness(80),
    transitionDurationMinutes(15),
    fadeStartTime(0),
    fadeStartBrightness(0),
    fadeTargetBrightness(0),
    onHour(6),
    onMinute(0),
    onMode("fixed"),
    onSunsetOffsetMinutes(0),
    offHour(21),
    sunriseOffsetMinutes(0),
    sunsetOffsetMinutes(0),
    totalOnTime(0),
    totalFadeInTime(0),
    totalFadeOutTime(0),
    totalCycles(0)
{
}

void LightController::begin() {
    logger.logInfo("Initializing Light Controller");
    
    // Load settings
    autoMode = settingsManager.getLightAutoMode();
    maxBrightness = settingsManager.getLightBrightnessPercent();
    transitionDurationMinutes = settingsManager.getLightTransitionDurationMinutes();
    onHour = settingsManager.getLightOnHour();
    onMinute = settingsManager.getLightOnMinute();
    onMode = settingsManager.getLightOnMode();
    onSunsetOffsetMinutes = settingsManager.getLightOnSunsetOffsetMinutes();
    offHour = settingsManager.getLightOffHour();
    
    // Initialize PWM
    logger.logInfo(String("PWM: Channel=") + String(PWM_CHANNEL) + String(", Freq=") + String(PWM_FREQ) + "Hz, Resolution=" + String(PWM_RESOLUTION) + "-bit, Pin=" + String(OUT_LIGHT_PIN));
    
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(OUT_LIGHT_PIN, PWM_CHANNEL);
    
    // Initialize to OFF state
    updatePWM();
    setState(LightState::OFF);
    
    logger.logInfo("Light Controller initialized");
}

void LightController::update() {
    if (testMode) {
        return; // Skip automatic updates in test mode
    }
    
    updateFade();
    checkSchedule();
}

void LightController::turnOn() {
    logger.logInfo("Light turned on manually");
    
    // Manual control: Turn on immediately at max brightness (no fade)
    currentBrightness = maxBrightness;
    targetBrightness = maxBrightness;
    updatePWM();
    setState(LightState::ON);
}

void LightController::turnOff() {
    logger.logInfo("Light turned off manually");
    
    // Manual control: Turn off immediately (no fade)
    currentBrightness = 0;
    targetBrightness = 0;
    updatePWM();
    setState(LightState::OFF);
}

void LightController::setBrightness(int percent) {
    targetBrightness = constrain(percent, 0, 100);
    currentBrightness = targetBrightness;
    updatePWM();
    
    logger.logInfo(String("Light brightness set to ") + String(targetBrightness) + "%");
}

void LightController::fadeIn() {
    // Fade can be started from any state (allows interruption)
    logger.logInfo("Starting fade in");
    fadeStartTime = millis();
    fadeStartBrightness = currentBrightness;
    fadeTargetBrightness = maxBrightness;
    setState(LightState::FADING_IN);
}

void LightController::fadeOut() {
    // Fade can be started from any state (allows interruption)
    logger.logInfo("Starting fade out");
    fadeStartTime = millis();
    fadeStartBrightness = currentBrightness;
    fadeTargetBrightness = 0;
    setState(LightState::FADING_OUT);
}

void LightController::setAutoMode(bool enabled) {
    autoMode = enabled;
    logger.logInfo(String("Light auto mode ") + String(enabled ? "enabled" : "disabled"));
}

bool LightController::isAutoMode() const {
    return autoMode;
}

void LightController::setTestMode(bool enabled) {
    testMode = enabled;
    logger.logInfo(String("Light test mode ") + String(enabled ? "enabled" : "disabled"));
}

bool LightController::isTestMode() const {
    return testMode;
}

LightState LightController::getState() const {
    return currentState;
}

String LightController::getStateString() const {
    switch (currentState) {
        case LightState::OFF: return "OFF";
        case LightState::ON: return "ON";
        case LightState::FADING_IN: return "FADING_IN";
        case LightState::FADING_OUT: return "FADING_OUT";
        case LightState::FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

int LightController::getCurrentBrightness() const {
    return currentBrightness;
}

int LightController::getTargetBrightness() const {
    return targetBrightness;
}

int LightController::getFadeProgressPercentage() const {
    if (currentState != LightState::FADING_IN && currentState != LightState::FADING_OUT) {
        return 100;
    }
    
    unsigned long fadeDuration = transitionDurationMinutes * 60 * 1000UL; // Convert to milliseconds
    unsigned long elapsed = millis() - fadeStartTime;
    
    if (elapsed >= fadeDuration) {
        return 100;
    }
    
    return (elapsed * 100) / fadeDuration;
}

int LightController::getMaxBrightness() const {
    return maxBrightness;
}

void LightController::setMaxBrightness(int percent) {
    maxBrightness = constrain(percent, 0, 100);
    settingsManager.setLightBrightnessPercent(maxBrightness);
    
    // Update target brightness if light is on
    if (currentState == LightState::ON) {
        targetBrightness = maxBrightness;
        currentBrightness = maxBrightness;
        updatePWM();
    }
    
    logger.logInfo(String("Light max brightness set to ") + String(maxBrightness) + "%");
}

int LightController::getTransitionDurationMinutes() const {
    return transitionDurationMinutes;
}

void LightController::setTransitionDurationMinutes(int minutes) {
    transitionDurationMinutes = constrain(minutes, 1, 60);
    settingsManager.setLightTransitionDurationMinutes(transitionDurationMinutes);
    
    logger.logInfo(String("Light transition duration set to ") + String(transitionDurationMinutes) + " minutes");
}

int LightController::getOnHour() const {
    return onHour;
}

void LightController::setOnHour(int hour) {
    onHour = constrain(hour, 0, 23);
    settingsManager.setLightOnHour(onHour);
    
    logger.logInfo(String("Light on hour set to ") + String(onHour));
}

int LightController::getOnMinute() const {
    return onMinute;
}

void LightController::setOnMinute(int minute) {
    onMinute = constrain(minute, 0, 59);
    settingsManager.setLightOnMinute(onMinute);
    
    logger.logInfo(String("Light on minute set to ") + String(onMinute));
}

String LightController::getOnMode() const {
    return onMode;
}

void LightController::setOnMode(const String& mode) {
    onMode = mode;
    settingsManager.setLightOnMode(onMode);
    
    logger.logInfo(String("Light on mode set to ") + onMode);
}

int LightController::getOnSunsetOffsetMinutes() const {
    return onSunsetOffsetMinutes;
}

void LightController::setOnSunsetOffsetMinutes(int minutes) {
    onSunsetOffsetMinutes = minutes;
    settingsManager.setLightOnSunsetOffsetMinutes(onSunsetOffsetMinutes);
    
    logger.logInfo(String("Light on sunset offset set to ") + String(onSunsetOffsetMinutes) + " minutes");
}

int LightController::getOffHour() const {
    return offHour;
}

void LightController::setOffHour(int hour) {
    offHour = constrain(hour, 0, 23);
    settingsManager.setLightOffHour(offHour);
    
    logger.logInfo(String("Light off hour set to ") + String(offHour));
}

int LightController::getSunriseOffsetMinutes() const {
    return sunriseOffsetMinutes;
}

void LightController::setSunriseOffsetMinutes(int minutes) {
    sunriseOffsetMinutes = minutes;
    settingsManager.setSunriseOffsetMinutes(sunriseOffsetMinutes);
    
    logger.logInfo(String("Light sunrise offset set to ") + String(sunriseOffsetMinutes) + " minutes");
}

int LightController::getSunsetOffsetMinutes() const {
    return sunsetOffsetMinutes;
}

void LightController::setSunsetOffsetMinutes(int minutes) {
    sunsetOffsetMinutes = minutes;
    settingsManager.setSunsetOffsetMinutes(sunsetOffsetMinutes);
    
    logger.logInfo(String("Light sunset offset set to ") + String(sunsetOffsetMinutes) + " minutes");
}

unsigned long LightController::getTotalOnTime() const {
    return totalOnTime;
}

unsigned long LightController::getTotalFadeInTime() const {
    return totalFadeInTime;
}

unsigned long LightController::getTotalFadeOutTime() const {
    return totalFadeOutTime;
}

unsigned long LightController::getTotalCycles() const {
    return totalCycles;
}

void LightController::resetStatistics() {
    totalOnTime = 0;
    totalFadeInTime = 0;
    totalFadeOutTime = 0;
    totalCycles = 0;
    
    logger.logInfo("Light statistics reset");
}

void LightController::toJson(JsonObject& json) const {
    json["state"] = getStateString();
    json["auto_mode"] = autoMode;
    json["current_brightness"] = currentBrightness;
    json["target_brightness"] = targetBrightness;
    json["max_brightness"] = maxBrightness;
    json["transition_duration_minutes"] = transitionDurationMinutes;
    json["on_hour"] = onHour;
    json["on_minute"] = onMinute;
    json["on_mode"] = onMode;
    json["on_sunset_offset_minutes"] = onSunsetOffsetMinutes;
    json["off_hour"] = offHour;
    json["sunrise_offset_minutes"] = sunriseOffsetMinutes;
    json["sunset_offset_minutes"] = sunsetOffsetMinutes;
    json["total_on_time"] = totalOnTime;
    json["total_fade_in_time"] = totalFadeInTime;
    json["total_fade_out_time"] = totalFadeOutTime;
    json["total_cycles"] = totalCycles;
    json["fade_progress_percentage"] = getFadeProgressPercentage();
    json["next_scheduled_action"] = getNextScheduledAction();
}

String LightController::getNextScheduledAction() const {
    if (!autoMode) {
        return "Auto mode disabled";
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time not available";
    }
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    
    // Calculate next on time
    int onTimeMinutes;
    if (onMode == "sunset_offset") {
        int sunsetMinutes = sunriseSunset.getSunsetMinutes();
        onTimeMinutes = sunsetMinutes + onSunsetOffsetMinutes;
        
        // Normalize to 0-1439 (24 hours * 60 minutes)
        if (onTimeMinutes < 0) onTimeMinutes += 1440;
        if (onTimeMinutes >= 1440) onTimeMinutes -= 1440;
    } else {
        onTimeMinutes = onHour * 60 + onMinute;
    }
    
    // Calculate next off time
    int offTimeMinutes = offHour * 60;
    
    String nextAction = "";
    
    if (currentState == LightState::OFF || currentState == LightState::FADING_OUT) {
        if (onTimeMinutes > currentTimeMinutes) {
            nextAction = "Turn on at " + String(onTimeMinutes / 60) + ":" + String(onTimeMinutes % 60);
        } else {
            nextAction = "Turn on tomorrow at " + String(onTimeMinutes / 60) + ":" + String(onTimeMinutes % 60);
        }
    } else {
        if (offTimeMinutes > currentTimeMinutes) {
            nextAction = "Turn off at " + String(offTimeMinutes / 60) + ":" + String(offTimeMinutes % 60);
        } else {
            nextAction = "Turn off tomorrow at " + String(offTimeMinutes / 60) + ":" + String(offTimeMinutes % 60);
        }
    }
    
    return nextAction;
}

bool LightController::hasFault() const {
    return currentState == LightState::FAULT;
}

void LightController::clearFault() {
    if (currentState == LightState::FAULT) {
        logger.logInfo("Light fault cleared");
        setState(LightState::OFF);
    }
}

// Private methods

void LightController::setState(LightState newState) {
    if (newState != currentState) {
        LightState oldState = currentState;
        currentState = newState;
        stateStartTime = millis();
        
        // Create state strings for logging
        String oldStateStr, newStateStr;
        switch (oldState) {
            case LightState::OFF: oldStateStr = "OFF"; break;
            case LightState::ON: oldStateStr = "ON"; break;
            case LightState::FADING_IN: oldStateStr = "FADING_IN"; break;
            case LightState::FADING_OUT: oldStateStr = "FADING_OUT"; break;
            case LightState::FAULT: oldStateStr = "FAULT"; break;
            default: oldStateStr = "UNKNOWN"; break;
        }
        switch (newState) {
            case LightState::OFF: newStateStr = "OFF"; break;
            case LightState::ON: newStateStr = "ON"; break;
            case LightState::FADING_IN: newStateStr = "FADING_IN"; break;
            case LightState::FADING_OUT: newStateStr = "FADING_OUT"; break;
            case LightState::FAULT: newStateStr = "FAULT"; break;
            default: newStateStr = "UNKNOWN"; break;
        }
        
        logger.logInfo(String("Light state changed from ") + oldStateStr + String(" to ") + newStateStr);
        
        // Update statistics
        if (newState == LightState::ON) {
            totalCycles++;
        }
    }
}

void LightController::updatePWM() {
    int pwmValue = (currentBrightness * 255) / 100; // Scale to 0-255
    ledcWrite(PWM_CHANNEL, pwmValue);
}

void LightController::updateFade() {
    if (currentState != LightState::FADING_IN && currentState != LightState::FADING_OUT) {
        return;
    }
    
    unsigned long fadeDuration = transitionDurationMinutes * 60 * 1000UL; // Convert to milliseconds
    unsigned long elapsed = millis() - fadeStartTime;
    
    if (elapsed >= fadeDuration) {
        // Fade complete
        currentBrightness = fadeTargetBrightness;
        updatePWM();
        
        if (currentState == LightState::FADING_IN) {
            setState(LightState::ON);
            totalFadeInTime += elapsed;
        } else {
            setState(LightState::OFF);
            totalFadeOutTime += elapsed;
        }
        
        return;
    }
    
    // Calculate progress (0.0 to 1.0)
    float progress = (float)elapsed / fadeDuration;
    
    // Use sine wave for smooth, natural transitions
    float sineProgress = sin(progress * PI / 2.0);  // Quarter sine wave (0 to π/2)
    
    // Calculate brightness based on fade direction
    if (currentState == LightState::FADING_IN) {
        currentBrightness = fadeStartBrightness + (fadeTargetBrightness - fadeStartBrightness) * sineProgress;
    } else {
        currentBrightness = fadeStartBrightness - (fadeStartBrightness - fadeTargetBrightness) * sineProgress;
    }
    
    updatePWM();
}

void LightController::checkSchedule() {
    if (!autoMode) {
        return;
    }
    
    // Auto mode uses fades for smooth transitions
    if (shouldTurnOnBySchedule()) {
        fadeIn();
    } else if (shouldTurnOffBySchedule()) {
        fadeOut();
    }
}

bool LightController::shouldTurnOnBySchedule() const {
    // Only trigger if currently OFF
    if (currentState != LightState::OFF) {
        return false;
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    
    // Calculate on time based on mode
    int onTimeMinutes;
    if (onMode == "sunset_offset") {
        int sunsetMinutes = sunriseSunset.getSunsetMinutes();
        onTimeMinutes = sunsetMinutes + onSunsetOffsetMinutes;
        
        // Normalize to 0-1439 (24 hours * 60 minutes)
        if (onTimeMinutes < 0) onTimeMinutes += 1440;
        if (onTimeMinutes >= 1440) onTimeMinutes -= 1440;
    } else {
        onTimeMinutes = onHour * 60 + onMinute;
    }
    
    // Check if it's time to turn on
    return currentTimeMinutes >= onTimeMinutes && currentTimeMinutes < (offHour * 60);
}

bool LightController::shouldTurnOffBySchedule() const {
    // Only trigger if currently ON
    if (currentState != LightState::ON) {
        return false;
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    
    // Check if it's time to turn off
    return currentTimeMinutes >= (offHour * 60);
}

int LightController::calculateSineWaveBrightness(float progress) const {
    // Use sine wave for smooth transitions
    float sineValue = sin(progress * PI / 2.0);
    return (int)(sineValue * 100);
}