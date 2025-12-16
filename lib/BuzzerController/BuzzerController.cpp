#include "BuzzerController.h"
#include "SettingsManager.h"
#include "Logger.h"
#include "esp32-hal-ledc.h"
#include <stdint.h>

// Default alert patterns for each alert type
const AlertPattern BuzzerController::DEFAULT_PATTERNS[] = { // NOSONAR - intentional array
    // PUMP_ERROR: 3 long beeps, repeat every 10 seconds, max 30 cycles
    {1000, 500, 3, 10000, 30},
    
    // SENSOR_ERROR: 2 short beeps, repeat every 5 seconds, max 20 cycles
    {200, 300, 2, 5000, 20},
    
    // WIFI_DISCONNECTED: 1 short beep, repeat every 30 seconds, infinite
    {200, 0, 1, 30000, 0},
    
    // LOW_MEMORY: 5 rapid short beeps, repeat every 15 seconds, max 10 cycles
    {100, 100, 5, 15000, 10},
    
    // DOOR_FAULT: 4 medium beeps, repeat every 8 seconds, max 15 cycles
    {500, 200, 4, 8000, 15},
    
    // LIGHT_FAULT: 3 short beeps, repeat every 10 seconds, max 20 cycles
    {300, 200, 3, 10000, 20},
    
    // SYSTEM_ERROR: 2 long beeps, repeat every 12 seconds, max 25 cycles
    {1000, 1000, 2, 12000, 25},
    
    // TEST_ALERT: 1 medium beep, no repeat
    {500, 0, 1, 0, 1}
};

void BuzzerController::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH); // Active LOW - turn off initially
    
    // Load settings
    loadFromSettings();
    
    logger.logInfo(String("Buzzer controller initialized on pin ") + String(_pin) + String(", type: ") + String(_buzzerType == BuzzerType::ACTIVE ? "Active" : "Passive") + String(", enabled: ") + String(_enabled ? "true" : "false"));
}

void BuzzerController::update() {
    if (!_enabled) {
        if (_isBeeping) {
            logger.logDebug("Buzzer update - disabled, stopping beep");
            stopBeep();
        }
        return;
    }
    
    // Check if silence period has expired
    if (isSilenced() && millis() > _silenceUntil) {
        _silenceUntil = 0;
        logger.logInfo("Buzzer silence period expired");
    }
    
    // Execute current pattern if active and not silenced
    if (_hasActiveAlert && !isSilenced()) {
        executePattern();
    } else if (_isBeeping) {
        logger.logDebug(String("Buzzer update - no active alert or silenced, stopping beep (has_active: ") + 
                       String(_hasActiveAlert ? "true" : "false") + String(", silenced: ") + 
                       String(isSilenced() ? "true" : "false") + ")");
        stopBeep();
    }
}

void BuzzerController::triggerAlert(AlertType alertType) {
    if (!_enabled) {
        String msg = "Buzzer alert: " + getAlertTypeString(alertType) + " - blocked (disabled)";
        logger.logDebug(msg);
        return;
    }
    
    // Don't override higher priority alerts
    if (_hasActiveAlert && alertType > _currentAlertType) {
        String msg = "Buzzer alert: " + getAlertTypeString(alertType) + 
                    " - blocked (higher priority alert active: " + getAlertTypeString(_currentAlertType) + ")";
        logger.logDebug(msg);
        return;
    }
    
    String msg = "Buzzer alert triggered: " + getAlertTypeString(alertType);
    logger.logInfo(msg);
    
    _currentAlertType = alertType;
    _hasActiveAlert = true;
    _currentPattern = DEFAULT_PATTERNS[static_cast<uint8_t>(alertType)];
    _currentCycle = 0;
    _currentBeep = 0;
    _lastStateChange = millis();
    _lastAlertTime = millis();
    
    logAlert(alertType, "triggered");
}

void BuzzerController::silenceAlerts(unsigned long silenceDurationMs) {
    if (!_hasActiveAlert) {
        return;
    }
    
    _silenceUntil = millis() + silenceDurationMs;
    logAlert(_currentAlertType, ("silenced for " + String(silenceDurationMs / 1000) + " seconds").c_str());
    
    // Stop current beep
    if (_isBeeping) {
        stopBeep();
    }
}

void BuzzerController::testAlert() {
    triggerAlert(AlertType::TEST_ALERT);
    logger.logInfo("Buzzer test alert triggered");
}
void BuzzerController::clearAlert(AlertType alertType) {
    if (_hasActiveAlert && _currentAlertType == alertType) {
        String msg = "Buzzer alert cleared: " + getAlertTypeString(alertType);
        logger.logInfo(msg);
        _hasActiveAlert = false;
        _currentCycle = 0;
        _currentBeep = 0;
        _lastStateChange = millis(); // Reset timing to prevent immediate restart
        _isBeeping = false;
        logAlert(alertType, "cleared");
        
        // Stop any ongoing beep immediately
        stopBeep();
        
        logger.logDebug("Buzzer alert - pattern execution should now stop");
    } else {
        String msg = "Buzzer alert clear ignored for " + getAlertTypeString(alertType) + 
                    " - not active (current: " + getAlertTypeString(_currentAlertType) + ", has_active: " + 
                    String(_hasActiveAlert ? "true" : "false") + ")";
        logger.logDebug(msg);
    }
}

void BuzzerController::setEnabled(bool enabled) {
    if (_enabled != enabled) {
        _enabled = enabled;
        
        if (!enabled && _isBeeping) {
            stopBeep();
        }
        
        logger.logInfo(String("Buzzer ") + String(enabled ? "enabled" : "disabled"));
    }
}

bool BuzzerController::isEnabled() const {
    return _enabled;
}

void BuzzerController::setBuzzerType(BuzzerType type) {
    if (_buzzerType != type) {
        _buzzerType = type;
        
        // Stop any ongoing beep when changing type
        if (_isBeeping) {
            stopBeep();
        }
        
        logger.logInfo(String("Buzzer type set to: ") + String(type == BuzzerType::ACTIVE ? "Active" : "Passive"));
    }
}

BuzzerType BuzzerController::getBuzzerType() const {
    return _buzzerType;
}

bool BuzzerController::hasActiveAlert() const {
    return _hasActiveAlert;
}

AlertType BuzzerController::getCurrentAlertType() const {
    return _currentAlertType;
}

unsigned long BuzzerController::getSilenceRemainingMs() const {
    if (!isSilenced()) {
        return 0;
    }
    
    unsigned long remaining = _silenceUntil - millis();
    return remaining > 0 ? remaining : 0;
}

void BuzzerController::loadFromSettings() {
    _enabled = settingsManager.getBuzzerEnabled();
    
    String buzzerTypeStr = settingsManager.getBuzzerType();
    if (buzzerTypeStr == "PASSIVE") {
        _buzzerType = BuzzerType::PASSIVE;
    } else {
        _buzzerType = BuzzerType::ACTIVE;
    }
}

void BuzzerController::saveToSettings() const {
    settingsManager.setBuzzerEnabled(_enabled);
    settingsManager.setBuzzerType(_buzzerType == BuzzerType::ACTIVE ? "ACTIVE" : "PASSIVE");
    settingsManager.save();
}

String BuzzerController::getAlertTypeString(AlertType type) const {
    switch (type) {
        case AlertType::PUMP_ERROR: return "PUMP_ERROR";
        case AlertType::SENSOR_ERROR: return "SENSOR_ERROR";
        case AlertType::WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
        case AlertType::LOW_MEMORY: return "LOW_MEMORY";
        case AlertType::DOOR_FAULT: return "DOOR_FAULT";
        case AlertType::LIGHT_FAULT: return "LIGHT_FAULT";
        case AlertType::SYSTEM_ERROR: return "SYSTEM_ERROR";
        case AlertType::TEST_ALERT: return "TEST_ALERT";
        default: return "UNKNOWN";
    }
}

void BuzzerController::toJson(JsonObject& json) const { // NOSONAR - json is being written
    json["enabled"] = _enabled;
    json["buzzer_type"] = _buzzerType == BuzzerType::ACTIVE ? "ACTIVE" : "PASSIVE";
    json["has_active_alert"] = _hasActiveAlert;
    
    if (_hasActiveAlert) {
        json["current_alert_type"] = getAlertTypeString(_currentAlertType);
        json["silence_remaining_ms"] = getSilenceRemainingMs();
    }
}

void BuzzerController::executePattern() {
    unsigned long currentTime = millis();
    
    // Check if we've exceeded max cycles
    if (_currentPattern.max_cycles > 0 && _currentCycle >= _currentPattern.max_cycles) {
        logger.logDebug(String("Buzzer pattern - exceeded max cycles (") + String(_currentCycle) + 
                       String(" >= ") + String(_currentPattern.max_cycles) + ")");
        _hasActiveAlert = false;
        logAlert(_currentAlertType, "completed max cycles");
        return;
    }
    
    // State machine for pattern execution
    if (!_isBeeping) {
        // Should we start a beep?
        unsigned long timeSinceLastState = currentTime - _lastStateChange;
        if (timeSinceLastState >= _currentPattern.pause_duration_ms) {
            logger.logDebug(String("Buzzer pattern - starting beep ") + String(_currentBeep + 1) + 
                           String(" of ") + String(_currentPattern.repeat_count) + 
                           String(" in cycle ") + String(_currentCycle + 1));
            startBeep();
            _lastStateChange = currentTime;
        }
    } else {
        // Should we stop the beep?
        unsigned long timeSinceLastState = currentTime - _lastStateChange;
        if (timeSinceLastState >= _currentPattern.beep_duration_ms) {
            logger.logDebug(String("Buzzer pattern - stopping beep ") + String(_currentBeep + 1));
            stopBeep();
            _lastStateChange = currentTime;
            _currentBeep++;
            
            // Check if we've completed all beeps in this cycle
            if (_currentBeep >= _currentPattern.repeat_count) {
                _currentBeep = 0;
                _currentCycle++;
                
                logger.logDebug(String("Buzzer pattern - completed cycle ") + String(_currentCycle) + 
                               String(" (max: ") + String(_currentPattern.max_cycles) + ")");
                
                // If this was the last beep and no pattern pause, we're done
                if (_currentPattern.pattern_pause_ms == 0) { // NOSONAR - nesting ok
                    _hasActiveAlert = false;
                    logger.logDebug("Buzzer pattern - no pattern pause, clearing alert");
                    logAlert(_currentAlertType, "pattern completed");
                } else {
                    logger.logDebug(String("Buzzer pattern - waiting for pattern pause ") + 
                                   String(_currentPattern.pattern_pause_ms) + "ms");
                }
            }
        }
    }
}

void BuzzerController::startBeep() {
    if (_buzzerType == BuzzerType::ACTIVE) {
        digitalWrite(_pin, LOW); // Active LOW - turn on
    } else {
        // Passive buzzer - use tone generation
        tone(_pin, 1000, _currentPattern.beep_duration_ms); // 1kHz tone
    }
    logger.logDebug(String("Buzzer beep started - pin LOW, duration: ") + String(_currentPattern.beep_duration_ms) + "ms");
    _isBeeping = true;
}

void BuzzerController::stopBeep() {
    if (_buzzerType == BuzzerType::ACTIVE) {
        digitalWrite(_pin, HIGH); // Active LOW - turn off
    } else {
        noTone(_pin);
    }
    logger.logDebug("Buzzer beep stopped - pin HIGH");
    _isBeeping = false;
}

bool BuzzerController::isSilenced() const {
    return _silenceUntil > 0 && millis() < _silenceUntil;
}

void BuzzerController::logAlert(AlertType type, const char* action) const {
    logger.logInfo(String("Buzzer alert: ") + getAlertTypeString(type) + String(" - ") + action);
}