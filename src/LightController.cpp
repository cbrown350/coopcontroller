#include "LightController.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "BuzzerController.h"
#include <time.h>
#include <math.h>

// External references
extern BuzzerController buzzerController;

LightController::LightController() {
    currentState = LightState::OFF;
    stateStartTime = 0;
    autoMode = false;
    testMode = false;
    
    currentBrightness = 0;
    targetBrightness = 0;
    maxBrightness = 100;
    
    transitionDurationMinutes = 5;
    fadeStartTime = 0;
    fadeStartBrightness = 0;
    fadeTargetBrightness = 0;
    
    onHour = 6;
    offHour = 21;
    sunriseOffsetMinutes = 0;
    sunsetOffsetMinutes = 0;
    
    totalOnTime = 0;
    totalFadeInTime = 0;
    totalFadeOutTime = 0;
    totalCycles = 0;
}

void LightController::begin() {
    // Initialize PWM for light output
    if (!testMode) {
        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(OUT_LIGHT_PIN, PWM_CHANNEL);
        ledcWrite(PWM_CHANNEL, 0);  // Start with light off
    }
    
    logger.logInfo("Light controller initialized with PWM");
    logger.logfInfo("PWM: Channel=%d, Freq=%dHz, Resolution=%d-bit, Pin=%d",
                    PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION, OUT_LIGHT_PIN);
    
    // Load configuration from settings
    maxBrightness = settingsManager.getSettings().light_brightness_percent;
    transitionDurationMinutes = settingsManager.getSettings().light_transition_duration_minutes;
    onHour = settingsManager.getLightOnHour();
    offHour = settingsManager.getLightOffHour();
    sunriseOffsetMinutes = settingsManager.getSunriseOffsetMinutes();
    sunsetOffsetMinutes = settingsManager.getSunsetOffsetMinutes();
    autoMode = settingsManager.getLightAutoMode();
    
    logger.logfInfo("Light config: Max brightness=%d%%, Transition=%dmin, Schedule=%d:00-%d:00, Auto=%s",
                    maxBrightness, transitionDurationMinutes, onHour, offHour,
                    autoMode ? "ON" : "OFF");
}

void LightController::update() {
    unsigned long currentTime = millis();
    
    // Update fade if in progress
    if (currentState == LightState::FADING_IN || currentState == LightState::FADING_OUT) {
        updateFade();
    }
    
    // Track time in states
    static unsigned long lastStateUpdate = 0;
    if (currentTime - lastStateUpdate >= 1000) {  // Update every second
        lastStateUpdate = currentTime;
        
        if (currentState == LightState::ON || currentState == LightState::FADING_IN) {
            totalOnTime++;
        }
        if (currentState == LightState::FADING_IN) {
            totalFadeInTime++;
        }
        if (currentState == LightState::FADING_OUT) {
            totalFadeOutTime++;
        }
    }
    
    // Check automatic schedule
    if (autoMode && (currentState == LightState::OFF || currentState == LightState::ON)) {
        static unsigned long lastScheduleCheck = 0;
        if (currentTime - lastScheduleCheck >= 60000) {  // Check every minute
            lastScheduleCheck = currentTime;
            checkSchedule();
        }
    }
    
    // Periodic status logging
    static unsigned long lastStatusLog = 0;
    if (currentTime - lastStatusLog >= 300000) {  // Log every 5 minutes
        lastStatusLog = currentTime;
        logger.logfDebug("Light status: State=%s, Brightness=%d%%, Auto=%s",
                        getStateString().c_str(), currentBrightness,
                        autoMode ? "ON" : "OFF");
    }
}

void LightController::updateFade() {
    unsigned long elapsed = millis() - fadeStartTime;
    unsigned long totalDuration = transitionDurationMinutes * 60000UL;  // Convert to milliseconds
    
    if (elapsed >= totalDuration) {
        // Fade complete
        currentBrightness = fadeTargetBrightness;
        
        if (currentState == LightState::FADING_IN) {
            setState(LightState::ON);
        } else if (currentState == LightState::FADING_OUT) {
            setState(LightState::OFF);
        }
        
        updatePWM();
        return;
    }
    
    // Calculate progress (0.0 to 1.0)
    float progress = (float)elapsed / (float)totalDuration;
    
    // Apply sine wave for smooth transition
    int sineBrightness = calculateSineWaveBrightness(progress);
    
    // Interpolate between start and target using sine curve
    currentBrightness = fadeStartBrightness + 
                       ((fadeTargetBrightness - fadeStartBrightness) * sineBrightness) / 100;
    
    updatePWM();
    
    // Debug logging every 30 seconds during fade
    static unsigned long lastFadeLog = 0;
    if (millis() - lastFadeLog >= 30000) {
        lastFadeLog = millis();
        logger.logfDebug("Fading: Progress=%d%%, Brightness=%d%% (%d->%d)",
                        (int)(progress * 100), currentBrightness,
                        fadeStartBrightness, fadeTargetBrightness);
    }
}

int LightController::calculateSineWaveBrightness(float progress) const {
    // Use sine wave for natural-looking fade
    // y = (sin(x * π - π/2) + 1) / 2
    // Maps 0.0-1.0 input to 0.0-1.0 output with smooth acceleration/deceleration
    float radians = progress * PI - (PI / 2.0);
    float sineValue = sin(radians);
    float normalized = (sineValue + 1.0) / 2.0;
    return (int)(normalized * 100.0);
}

void LightController::updatePWM() {
    if (testMode) {
        logger.logfDebug("Test mode: PWM brightness would be %d%%", currentBrightness);
        return;
    }
    
    // Convert brightness percentage (0-100) to PWM value (0-255)
    // Apply maxBrightness limit
    int effectiveBrightness = (currentBrightness * maxBrightness) / 100;
    int pwmValue = (effectiveBrightness * 255) / 100;
    
    ledcWrite(PWM_CHANNEL, pwmValue);
    
    logger.logfVerbose("PWM updated: Brightness=%d%% (effective=%d%%), PWM=%d/255",
                      currentBrightness, effectiveBrightness, pwmValue);
}

void LightController::setState(LightState newState) {
    if (newState == currentState) return;
    
    LightState oldState = currentState;
    unsigned long currentTime = millis();
    
    // Log state change
    logger.logf("Light state: %s -> %s",
               getStateString().c_str(),
               newState == LightState::OFF ? "OFF" :
               newState == LightState::ON ? "ON" :
               newState == LightState::FADING_IN ? "FADING_IN" :
               newState == LightState::FADING_OUT ? "FADING_OUT" : "FAULT");
    
    // Update state
    currentState = newState;
    stateStartTime = currentTime;
    
    // Handle entry to new state
    switch (newState) {
        case LightState::OFF:
            currentBrightness = 0;
            targetBrightness = 0;
            updatePWM();
            buzzerController.clearAlert(AlertType::LIGHT_FAULT);
            logger.logInfo("Light is now OFF");
            break;
            
        case LightState::ON:
            currentBrightness = maxBrightness;
            targetBrightness = maxBrightness;
            updatePWM();
            buzzerController.clearAlert(AlertType::LIGHT_FAULT);
            if (oldState == LightState::OFF) {
                totalCycles++;
            }
            logger.logfInfo("Light is now ON at %d%% brightness", currentBrightness);
            break;
            
        case LightState::FADING_IN:
            fadeStartTime = currentTime;
            fadeStartBrightness = currentBrightness;
            fadeTargetBrightness = maxBrightness;
            totalCycles++;
            logger.logfInfo("Light fading IN: %d%% -> %d%% over %d minutes",
                          fadeStartBrightness, fadeTargetBrightness, transitionDurationMinutes);
            break;
            
        case LightState::FADING_OUT:
            fadeStartTime = currentTime;
            fadeStartBrightness = currentBrightness;
            fadeTargetBrightness = 0;
            logger.logfInfo("Light fading OUT: %d%% -> %d%% over %d minutes",
                          fadeStartBrightness, fadeTargetBrightness, transitionDurationMinutes);
            break;
            
        case LightState::FAULT:
            currentBrightness = 0;
            targetBrightness = 0;
            updatePWM();
            buzzerController.triggerAlert(AlertType::LIGHT_FAULT);
            logger.logError("Light FAULT state entered");
            break;
    }
}

void LightController::checkSchedule() {
    if (shouldTurnOnBySchedule() && currentState == LightState::OFF) {
        logger.logInfo("Schedule: Fading light in");
        fadeIn();
    } else if (shouldTurnOffBySchedule() && currentState == LightState::ON) {
        logger.logInfo("Schedule: Fading light out");
        fadeOut();
    }
}

bool LightController::shouldTurnOnBySchedule() const {
    time_t now = time(nullptr);
    if (now < 0) return false;
    
    struct tm* timeinfo = localtime(&now);
    int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int onTimeMinutes = onHour * 60 + sunriseOffsetMinutes;
    
    // Handle wrap-around past midnight
    if (onTimeMinutes < 0) onTimeMinutes += 1440;
    if (onTimeMinutes >= 1440) onTimeMinutes -= 1440;
    
    return (currentMinutes >= onTimeMinutes && currentState == LightState::OFF);
}

bool LightController::shouldTurnOffBySchedule() const {
    time_t now = time(nullptr);
    if (now < 0) return false;
    
    struct tm* timeinfo = localtime(&now);
    int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int offTimeMinutes = offHour * 60 + sunsetOffsetMinutes;
    
    // Handle wrap-around past midnight
    if (offTimeMinutes < 0) offTimeMinutes += 1440;
    if (offTimeMinutes >= 1440) offTimeMinutes -= 1440;
    
    return (currentMinutes >= offTimeMinutes && currentState == LightState::ON);
}

time_t LightController::getTodaySunrise() const {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    struct tm sunrise = *timeinfo;
    sunrise.tm_hour = onHour;
    sunrise.tm_min = sunriseOffsetMinutes;
    sunrise.tm_sec = 0;
    return mktime(&sunrise);
}

time_t LightController::getTodaySunset() const {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    struct tm sunset = *timeinfo;
    sunset.tm_hour = offHour;
    sunset.tm_min = sunsetOffsetMinutes;
    sunset.tm_sec = 0;
    return mktime(&sunset);
}

// Manual control methods
void LightController::turnOn() {
    if (currentState == LightState::OFF || currentState == LightState::FADING_OUT) {
        setState(LightState::ON);
    } else {
        logger.logDebug("Cannot turn on light - current state: " + getStateString());
    }
}

void LightController::turnOff() {
    if (currentState == LightState::ON || currentState == LightState::FADING_IN) {
        setState(LightState::OFF);
    } else {
        logger.logDebug("Cannot turn off light - current state: " + getStateString());
    }
}

void LightController::setBrightness(int percent) {
    percent = constrain(percent, 0, 100);
    
    if (percent == 0) {
        turnOff();
    } else {
        currentBrightness = percent;
        targetBrightness = percent;
        if (currentState == LightState::OFF) {
            setState(LightState::ON);
        } else {
            updatePWM();
            logger.logfInfo("Light brightness set to %d%%", percent);
        }
    }
}

void LightController::fadeIn() {
    if (currentState == LightState::OFF || currentState == LightState::FADING_OUT) {
        setState(LightState::FADING_IN);
    } else {
        logger.logDebug("Cannot fade in light - current state: " + getStateString());
    }
}

void LightController::fadeOut() {
    if (currentState == LightState::ON || currentState == LightState::FADING_IN) {
        setState(LightState::FADING_OUT);
    } else {
        logger.logDebug("Cannot fade out light - current state: " + getStateString());
    }
}

// Mode control
void LightController::setAutoMode(bool enabled) {
    autoMode = enabled;
    logger.logf("Light auto mode: %s", enabled ? "ENABLED" : "DISABLED");
}

bool LightController::isAutoMode() const {
    return autoMode;
}

void LightController::setTestMode(bool enabled) {
    testMode = enabled;
    
    if (enabled) {
        logger.logInfo("Light test mode ENABLED - no PWM output");
    } else {
        logger.logInfo("Light test mode DISABLED - PWM active");
        // Reinitialize PWM
        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(OUT_LIGHT_PIN, PWM_CHANNEL);
        updatePWM();
    }
}

bool LightController::isTestMode() const {
    return testMode;
}

// State getters
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
        return 100;  // No fade in progress
    }
    
    unsigned long elapsed = millis() - fadeStartTime;
    unsigned long totalDuration = transitionDurationMinutes * 60000UL;
    
    if (elapsed >= totalDuration) return 100;
    
    return (elapsed * 100) / totalDuration;
}

// Configuration getters/setters
int LightController::getMaxBrightness() const {
    return maxBrightness;
}

void LightController::setMaxBrightness(int percent) {
    maxBrightness = constrain(percent, 0, 100);
    logger.logfInfo("Light max brightness set to %d%%", maxBrightness);
    
    // Update PWM if currently on
    if (currentState == LightState::ON) {
        updatePWM();
    }
}

int LightController::getTransitionDurationMinutes() const {
    return transitionDurationMinutes;
}

void LightController::setTransitionDurationMinutes(int minutes) {
    transitionDurationMinutes = constrain(minutes, 1, 60);
    logger.logfInfo("Light transition duration set to %d minutes", transitionDurationMinutes);
}

int LightController::getOnHour() const {
    return onHour;
}

void LightController::setOnHour(int hour) {
    onHour = constrain(hour, 0, 23);
    logger.logfInfo("Light ON hour set to %d:00", onHour);
}

int LightController::getOffHour() const {
    return offHour;
}

void LightController::setOffHour(int hour) {
    offHour = constrain(hour, 0, 23);
    logger.logfInfo("Light OFF hour set to %d:00", offHour);
}

int LightController::getSunriseOffsetMinutes() const {
    return sunriseOffsetMinutes;
}

void LightController::setSunriseOffsetMinutes(int minutes) {
    sunriseOffsetMinutes = constrain(minutes, -120, 120);
    logger.logfInfo("Light sunrise offset set to %d minutes", sunriseOffsetMinutes);
}

int LightController::getSunsetOffsetMinutes() const {
    return sunsetOffsetMinutes;
}

void LightController::setSunsetOffsetMinutes(int minutes) {
    sunsetOffsetMinutes = constrain(minutes, -120, 120);
    logger.logfInfo("Light sunset offset set to %d minutes", sunsetOffsetMinutes);
}

// Statistics
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

// JSON serialization
void LightController::toJson(JsonObject& json) const {
    json["state"] = getStateString();
    json["current_brightness"] = currentBrightness;
    json["target_brightness"] = targetBrightness;
    json["max_brightness"] = maxBrightness;
    json["fade_progress"] = getFadeProgressPercentage();
    json["auto_mode"] = autoMode;
    json["test_mode"] = testMode;
    json["transition_duration_minutes"] = transitionDurationMinutes;
    json["on_hour"] = onHour;
    json["off_hour"] = offHour;
    json["total_on_time"] = totalOnTime;
    json["total_fade_in_time"] = totalFadeInTime;
    json["total_fade_out_time"] = totalFadeOutTime;
    json["total_cycles"] = totalCycles;
    json["next_scheduled_action"] = getNextScheduledAction();
}

String LightController::getNextScheduledAction() const {
    if (!autoMode) return "Auto mode disabled";
    
    time_t now = time(nullptr);
    if (now < 0) return "Time not available";
    
    struct tm* timeinfo = localtime(&now);
    int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int onTimeMinutes = onHour * 60 + sunriseOffsetMinutes;
    int offTimeMinutes = offHour * 60 + sunsetOffsetMinutes;
    
    // Handle wrap-around
    if (onTimeMinutes < 0) onTimeMinutes += 1440;
    if (onTimeMinutes >= 1440) onTimeMinutes -= 1440;
    if (offTimeMinutes < 0) offTimeMinutes += 1440;
    if (offTimeMinutes >= 1440) offTimeMinutes -= 1440;
    
    if (currentState == LightState::OFF && currentMinutes < onTimeMinutes) {
        int hoursUntil = (onTimeMinutes - currentMinutes) / 60;
        int minutesUntil = (onTimeMinutes - currentMinutes) % 60;
        return "Turn ON in " + String(hoursUntil) + "h " + String(minutesUntil) + "m";
    } else if (currentState == LightState::ON && currentMinutes < offTimeMinutes) {
        int hoursUntil = (offTimeMinutes - currentMinutes) / 60;
        int minutesUntil = (offTimeMinutes - currentMinutes) % 60;
        return "Turn OFF in " + String(hoursUntil) + "h " + String(minutesUntil) + "m";
    }
    
    return "No scheduled action";
}

// Fault handling
bool LightController::hasFault() const {
    return currentState == LightState::FAULT;
}

void LightController::clearFault() {
    if (currentState == LightState::FAULT) {
        setState(LightState::OFF);
        logger.logInfo("Light fault cleared");
    }
}