#include "LightController.h"
#include "SettingsManager.h"
#include "Logger.h"
#include "SunriseSunset.h"
#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>


LightController::LightController() :
    hal(nullptr),
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
    totalCycles(0),
    lastTriggerSource_(TriggerSource::STARTUP)
{}

void LightController::begin(IHAL* _hal, SunriseSunsetCalculator* _sunriseSunset) {
    this->sunriseSunset = _sunriseSunset;
    this->hal = _hal;
    
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
    logger.logfInfo("PWM: Channel=%d, Freq=%d Hz, Resolution=%d-bit, Pin=%d", PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION, OUT_LIGHT_PIN);
    
    hal->pwmSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    hal->pwmAttachPin(OUT_LIGHT_PIN, PWM_CHANNEL);
    
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

void LightController::turnOn(TriggerSource trigger) {
    lastTriggerSource_ = trigger;
    logger.logfInfo("Light turned on (trigger: %s)", triggerSourceToString(trigger).c_str());

    // Manual control: Turn on immediately at max brightness (no fade)
    currentBrightness = maxBrightness;
    targetBrightness = maxBrightness;
    updatePWM();
    setState(LightState::ON);
}

void LightController::turnOff(TriggerSource trigger) {
    lastTriggerSource_ = trigger;
    logger.logfInfo("Light turned off (trigger: %s)", triggerSourceToString(trigger).c_str());

    // Manual control: Turn off immediately (no fade)
    currentBrightness = 0;
    targetBrightness = 0;
    updatePWM();
    setState(LightState::OFF);
}

void LightController::setBrightness(int percent, TriggerSource trigger) {
    targetBrightness = constrain(percent, 0, 100);
    currentBrightness = targetBrightness;
    lastTriggerSource_ = trigger;
    updatePWM();

    logger.logfInfo("Light brightness set to %d%% (trigger: %s)", targetBrightness, triggerSourceToString(trigger).c_str());
}

void LightController::fadeIn(TriggerSource trigger) {
    // Fade can be started from any state (allows interruption)
    lastTriggerSource_ = trigger;
    logger.logfInfo("Starting fade in (trigger: %s)", triggerSourceToString(trigger).c_str());
    fadeStartTime = millis();
    fadeStartBrightness = currentBrightness;
    fadeTargetBrightness = maxBrightness;
    setState(LightState::FADING_IN);
}

void LightController::fadeOut(TriggerSource trigger) {
    // Fade can be started from any state (allows interruption)
    lastTriggerSource_ = trigger;
    logger.logfInfo("Starting fade out (trigger: %s)", triggerSourceToString(trigger).c_str());
    fadeStartTime = millis();
    fadeStartBrightness = currentBrightness;
    fadeTargetBrightness = 0;
    setState(LightState::FADING_OUT);
}

void LightController::setAutoMode(bool enabled, TriggerSource trigger) {
    autoMode = enabled;
    lastTriggerSource_ = trigger;
    logger.logfInfo("Light auto mode %s (trigger: %s)", enabled ? "enabled" : "disabled", triggerSourceToString(trigger).c_str());
}

bool LightController::isAutoMode() const {
    return autoMode;
}

void LightController::setTestMode(bool enabled) {
    testMode = enabled;
    logger.logfInfo("Light test mode %s", enabled ? "enabled" : "disabled");
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
    
    logger.logfInfo("Light max brightness set to %d%%", maxBrightness);
}

int LightController::getTransitionDurationMinutes() const {
    return transitionDurationMinutes;
}

void LightController::setTransitionDurationMinutes(int minutes) {
    transitionDurationMinutes = constrain(minutes, 1, 60);
    settingsManager.setLightTransitionDurationMinutes(transitionDurationMinutes);
    
    logger.logfInfo("Light transition duration set to %d minutes", transitionDurationMinutes);
}

int LightController::getOnHour() const {
    return onHour;
}

void LightController::setOnHour(int hour) {
    onHour = constrain(hour, 0, 23);
    settingsManager.setLightOnHour(onHour);
    
    logger.logfInfo("Light on hour set to %d", onHour);
}

int LightController::getOnMinute() const {
    return onMinute;
}

void LightController::setOnMinute(int minute) {
    onMinute = constrain(minute, 0, 59);
    settingsManager.setLightOnMinute(onMinute);
    
    logger.logfInfo("Light on minute set to %d", onMinute);
}

String LightController::getOnMode() const {
    return onMode;
}

void LightController::setOnMode(const String& mode) {
    onMode = mode;
    settingsManager.setLightOnMode(onMode);
    
    logger.logfInfo("Light on mode set to %s", onMode.c_str());
}

int LightController::getOnSunsetOffsetMinutes() const {
    return onSunsetOffsetMinutes;
}

void LightController::setOnSunsetOffsetMinutes(int minutes) {
    onSunsetOffsetMinutes = minutes;
    settingsManager.setLightOnSunsetOffsetMinutes(onSunsetOffsetMinutes);
    
    logger.logfInfo("Light on sunset offset set to %d minutes", onSunsetOffsetMinutes);
}

int LightController::getOffHour() const {
    return offHour;
}

void LightController::setOffHour(int hour) {
    offHour = constrain(hour, 0, 23);
    settingsManager.setLightOffHour(offHour);
    
    logger.logfInfo("Light off hour set to %d", offHour);
}

int LightController::getSunriseOffsetMinutes() const {
    return sunriseOffsetMinutes;
}

void LightController::setSunriseOffsetMinutes(int minutes) {
    sunriseOffsetMinutes = minutes;
    settingsManager.setSunriseOffsetMinutes(sunriseOffsetMinutes);
    
    logger.logfInfo("Light sunrise offset set to %d minutes", sunriseOffsetMinutes);
}

int LightController::getSunsetOffsetMinutes() const {
    return sunsetOffsetMinutes;
}

void LightController::setSunsetOffsetMinutes(int minutes) {
    sunsetOffsetMinutes = minutes;
    settingsManager.setSunsetOffsetMinutes(sunsetOffsetMinutes);
    
    logger.logfInfo("Light sunset offset set to %d minutes", sunsetOffsetMinutes);
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

void LightController::toJson(JsonObject& json) const { // NOSONAR - json is written
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
    if (!hal->getLocalTime(&timeinfo, 0)) {
        return "Time not available";
    }
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    
    // Calculate next on time
    int onTimeMinutes;
    if (onMode == "sunset_offset") {
        int sunsetMinutes = sunriseSunset->getSunsetMinutes();
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
        
        // Helper to convert LightState to string
        auto stateToStr = [](LightState s) -> const char* {
            switch (s) {
                case LightState::OFF: return "OFF";
                case LightState::ON: return "ON";
                case LightState::FADING_IN: return "FADING_IN";
                case LightState::FADING_OUT: return "FADING_OUT";
                case LightState::FAULT: return "FAULT";
                default: return "UNKNOWN";
            }
        };

        logger.logfInfo("Light state changed from %s to %s", stateToStr(oldState), stateToStr(newState));
        
        // Update statistics
        if (newState == LightState::ON) {
            totalCycles++;
        }
    }
}

void LightController::updatePWM() { // NOSONAR - modifies light state
    int pwmValue = (currentBrightness * 255) / 100; // Scale to 0-255
    hal->pwmWrite(PWM_CHANNEL, pwmValue);
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
    double progress = static_cast<double>(elapsed) / static_cast<double>(fadeDuration);
    
    // Use sine wave for smooth, natural transitions
    double sineProgress = sin(progress * PI / 2.0);  // Quarter sine wave (0 to π/2)
    
    // Calculate brightness based on fade direction
    if (currentState == LightState::FADING_IN) {
        currentBrightness = static_cast<int>(fadeStartBrightness + (fadeTargetBrightness - fadeStartBrightness) * sineProgress);
    } else {
        currentBrightness = static_cast<int>(fadeStartBrightness - (fadeStartBrightness - fadeTargetBrightness) * sineProgress);
    }
    
    updatePWM();
}

void LightController::checkSchedule() {
    if (!autoMode) {
        return;
    }

    // Auto mode uses fades for smooth transitions
    if (shouldTurnOnBySchedule()) {
        fadeIn(TriggerSource::TIMER);
    } else if (shouldTurnOffBySchedule()) {
        fadeOut(TriggerSource::TIMER);
    }
}

bool LightController::shouldTurnOnBySchedule() const {
    // Only trigger if currently OFF
    if (currentState != LightState::OFF) {
        return false;
    }
    
    struct tm timeinfo;
    if (!hal->getLocalTime(&timeinfo, 0)) {
        return false;
    }
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    
    // Calculate on time based on mode
    int onTimeMinutes;
    if (onMode == "sunset_offset") {
        int sunsetMinutes = sunriseSunset->getSunsetMinutes();
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
    if (!hal->getLocalTime(&timeinfo, 0)) {
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
    auto sineValue = static_cast<float>(sin(progress * PI / 2.0));
    return (int)(sineValue * 100);
}