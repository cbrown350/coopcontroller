#include "PumpController.h"
#include "SettingsManager.h"
#include "Logger.h"

// Constructor with single sensor (backward compatibility)
PumpController::PumpController(SensorManager* sensor, int pin) 
    : pumpPin(pin), primarySensor_(sensor), flowSensor_(sensor) {
    // Initialize status
    status.state = PUMP_OFF;
    status.is_active = false;
    status.last_switch_time = 0;
    status.current_cycle_start = 0;
    status.current_cycle_duration = 0;
    status.temperature_f = 0.0f;
    status.temperature_below_threshold = false;
    status.flow_error = false;
    status.total_on_time = 0;
    status.total_off_time = 0;
    status.total_cycles = 0;
    
    // Initialize timing variables
    lastUpdateTime = 0;
    cycleStartTime = 0;
    currentlyInOnPhase = false;
    offPhaseStartTime = 0;
    
    // Initialize error detection
    lastFlowCheckTime = 0;
    errorStartTime = 0;
    waitingForRetry = false;
}

// Constructor with separate sensors for temperature and flow
PumpController::PumpController(SensorManager* primarySensor, SensorManager* flowSensor, int pin) 
    : pumpPin(pin), primarySensor_(primarySensor), flowSensor_(flowSensor) {
    // Initialize status
    status.state = PUMP_OFF;
    status.is_active = false;
    status.last_switch_time = 0;
    status.current_cycle_start = 0;
    status.current_cycle_duration = 0;
    status.temperature_f = 0.0f;
    status.temperature_below_threshold = false;
    status.flow_error = false;
    status.total_on_time = 0;
    status.total_off_time = 0;
    status.total_cycles = 0;
    
    // Initialize timing variables
    lastUpdateTime = 0;
    cycleStartTime = 0;
    currentlyInOnPhase = false;
    offPhaseStartTime = 0;
    
    // Initialize error detection
    lastFlowCheckTime = 0;
    errorStartTime = 0;
    waitingForRetry = false;
}

void PumpController::begin() {
    pinMode(pumpPin, OUTPUT);
    digitalWrite(pumpPin, LOW);
    
    logger.logDebug(String("OUT_PUMP_PIN: ") + String(OUT_PUMP_PIN));
    logger.logDebug(String("PumpController initialized with pin ") + String(pumpPin));
    logger.logDebug(String("PumpController begin - pin ") + String(pumpPin) + String(" set as OUTPUT, initial state LOW"));
    
    logger.log("Pump controller initialized");
    logger.logf("Pump pin: %d", pumpPin);
    
    // Start with pump off
    setPumpState(false);
    if (settingsManager.getPumpAutoMode()) {
        status.state = PUMP_AUTO;
    } else {
        status.state = PUMP_OFF;
    }

    logger.logDebug(String("PumpController begin - mode set to ") + (settingsManager.getPumpAutoMode() ? "AUTO" : "MANUAL"));
    logger.logDebug("Pump controller initialization complete");
}

void PumpController::update() {
    unsigned long currentTime = millis();
    
    // Get temperature from primary sensor
    if (primarySensor_) {
        // Try to get temperature from sensor 1 first, then sensor 2
        status.temperature_f = primarySensor_->getTemperature1F();
        if (isnan(status.temperature_f)) {
            status.temperature_f = primarySensor_->getTemperature2F();
        }
    } else {
        status.temperature_f = NAN;
    }
    
    // Check for flow errors
    status.flow_error = checkFlowError() || status.flow_error;
    
    // Update statistics
    updateStatistics();
    
    // Handle different states
    switch (status.state) {
        case PUMP_OFF:
            // Manual off state - do nothing
            break;
            
        case PUMP_ON:
            // Manual on state - keep pump on
            if (!status.is_active) {
                setPumpState(true);
                clearFlowError();
            }
            break;
            
        case PUMP_AUTO:
            // Automatic mode - control based on temperature and cycling
            handleAutoMode(currentTime);
            break;
            
        case PUMP_ERROR:
            // Error state - turn off pump
            if (status.is_active) {
                setPumpState(false);
                logger.log("Pump turned off due to error condition");
            }
            break;
    }
    
    lastUpdateTime = currentTime;
}

bool PumpController::checkFlowError() {
    // Use flowSensor if provided, otherwise fall back to primarySensor
    if (!flowSensor_) {
        return false; // No flow sensor available
    }
    
    // Check if we have a water meter connected to the flow sensor
    bool hasWaterMeter = false;
    unsigned long lastPulseTime = 0;
    
    // Check sensor 1
    if (flowSensor_->getSensor1Type() == SensorType::WATER_METER) {
        hasWaterMeter = true;
        SensorData sensor1Data = flowSensor_->getSensor1Data();
        lastPulseTime = max(lastPulseTime, sensor1Data.last_pulse_time.load());
    }
    
    // Check sensor 2
    if (flowSensor_->getSensor2Type() == SensorType::WATER_METER) {
        hasWaterMeter = true;
        SensorData sensor2Data = flowSensor_->getSensor2Data();
        lastPulseTime = max(lastPulseTime, sensor2Data.last_pulse_time.load());
    }
    
    if (!hasWaterMeter) {
        return false; // No water meter available for flow detection
    }
    
    // Only check for flow error when pump is on and running long enough
    if (!status.is_active) {
        return false;
    }
    
    unsigned long pumpRunStart = getCurrentRunStartTime();
    unsigned long currentTimeMs = millis();
    unsigned long pumpRunTime = (pumpRunStart > 0) ? (currentTimeMs - pumpRunStart) : 0;
    int timeoutSeconds = settingsManager.getWaterFlowErrorTimeoutSeconds();
    unsigned long timeoutMs = (unsigned long)timeoutSeconds * 1000UL;
    
    // Check if pump has been running long enough and no flow detected
    if (pumpRunTime >= timeoutMs && (currentTimeMs - lastPulseTime) >= timeoutMs) {
        logger.log("Flow error detected: pump running without flow for timeout period");
        logger.logDebug(String("Pump run time: ") + String(pumpRunTime) + " ms, Current time: " + String(currentTimeMs) + " ms, Last pulse time: " + String(lastPulseTime) + " ms, Timeout: " + String(timeoutMs) + " ms");
        return true;
    }
    
    return false;
}

void PumpController::handleAutoMode(unsigned long currentTime) {
    // Check if we have a valid temperature reading
    if (isnan(status.temperature_f)) {
        // No temperature sensor available - cannot do temperature-based control
        if (status.state == PUMP_AUTO) {
            logger.logWarning("No temperature sensor available - automatic temperature control disabled");
            // Turn off pump but stay in AUTO mode in case sensor becomes available later
            if (status.is_active) {
                setPumpState(false);
            }
            cycleStartTime = 0; // Reset cycle
            currentlyInOnPhase = false;
        }
        return;
    }
    float onThreshold = settingsManager.getTempThresholdOnF();
    float offThreshold = settingsManager.getTempThresholdOffF();
    
    // If temperature is between ON and OFF thresholds, maintain current state (hysteresis)
    if (status.temperature_f < onThreshold) {
        status.temperature_below_threshold = true;
    }
    
    // Check if temperature is below ON threshold to start cycling
    if (status.temperature_below_threshold) {
        // Temperature is below threshold - run cycling
        if (cycleStartTime == 0) {
            // Start new cycle
            cycleStartTime = currentTime;
            currentlyInOnPhase = true;
            setPumpState(true);
            clearFlowError();
            status.total_cycles++;
            
            logger.logf("Temperature below ON threshold (%.1f°F < %.1f°F), starting pump cycle - ON phase", 
                       status.temperature_f, onThreshold);
        } else {
            // Check if we need to switch phases
            unsigned long cycleElapsed = currentTime - cycleStartTime;
            int onTime = settingsManager.getPumpOnTimeSeconds() * 1000;
            int offTime = settingsManager.getPumpOffTimeSeconds() * 1000;
            
            if (currentlyInOnPhase) {
                // In ON phase - check if it's time to switch to OFF
                unsigned long timeUntilOff = onTime - cycleElapsed;
                if (cycleElapsed >= onTime) {
                    // Switch to off phase
                    currentlyInOnPhase = false;
                    offPhaseStartTime = currentTime;
                    setPumpState(false);
                    logger.logf("Pump cycle: switching to OFF phase for %d seconds", settingsManager.getPumpOffTimeSeconds());
                } else if ((cycleElapsed % 10000 < 1000)) {
                    // Log countdown every 10 seconds in debug mode
                    logger.logDebug(String("Pump ON phase: ") + String((timeUntilOff + 500) / 1000) + " seconds remaining until OFF");
                }
            } else {
                // In OFF phase - check if it's time to switch to ON
                if (cycleElapsed >= (onTime + offTime)) {
                    // Start new cycle
                    cycleStartTime = currentTime;
                    currentlyInOnPhase = true;
                    setPumpState(true);
                    clearFlowError();
                    status.total_cycles++;
                    logger.logf("Starting new pump cycle, temperature: %.1f°F - ON phase", status.temperature_f);
                } else if ((cycleElapsed % 10000 < 1000)) {
                    // Log countdown every 10 seconds in debug mode
                    unsigned long timeUntilOn = (onTime + offTime) - cycleElapsed;
                    logger.logDebug(String("Pump OFF phase: ") + String((timeUntilOn + 500) / 1000) + " seconds remaining until ON");
                }
            }
        }
    } 
    if (status.temperature_f > offThreshold) {
        // Temperature is above OFF threshold - turn off pump and reset cycle
        if (status.is_active || cycleStartTime != 0) {
            setPumpState(false);
            // clearFlowError(); // keep flow error flag so it's known it happened
            cycleStartTime = 0; // Reset cycle
            currentlyInOnPhase = false;
            logger.logf("Temperature above OFF threshold (%.1f°F > %.1f°F), pump turned off and cycle reset", 
                       status.temperature_f, offThreshold);
        }
        status.temperature_below_threshold = false;
    }
    
    // Check for flow error during pump operation - handle error without changing state
    if (status.is_active && status.flow_error && status.state == PUMP_AUTO) {
        // Don't change state to ERROR, keep as AUTO but handle error logic
        errorStartTime = currentTime;
        waitingForRetry = true;
        setPumpState(false); // Turn off pump but keep AUTO state
        logger.log("Flow error detected during pump operation, pump turned off but staying in AUTO state");
    }
}

void PumpController::setPumpState(bool isOn) {
    logger.logDebug(String("Setting pump pin ") + String(pumpPin) + " to " + (isOn ? "HIGH (ON)" : "LOW (OFF)"));
    
    digitalWrite(pumpPin, isOn ? HIGH : LOW);
    
    if (status.is_active != isOn) {
        status.is_active = isOn;
        status.last_switch_time = millis();
        
        if (isOn) {
            status.current_cycle_start = millis();
            logger.log("Pump turned ON");
            logger.logDebug("Pump turned ON - cycle started");
        } else {
            if (status.current_cycle_start > 0) {
                status.current_cycle_duration = millis() - status.current_cycle_start;
            }
            logger.log("Pump turned OFF");
            logger.logDebug(String("Pump turned OFF - cycle duration: ") + String((unsigned long)status.current_cycle_duration) + " ms");
        }
    }
}

void PumpController::updateStatistics() {
    if (status.is_active && status.current_cycle_start > 0) {
        // Update total on time
        status.total_on_time = millis() - status.current_cycle_start;
    } else if (!status.is_active && offPhaseStartTime > 0) {
        // Update total off time when in off phase
        status.total_off_time = millis() - offPhaseStartTime;
    }
}

void PumpController::turnOn() {
    status.state = PUMP_ON;
    logger.log("Pump set to manual ON mode");
}

void PumpController::turnOff() {
    status.state = PUMP_OFF;
    cycleStartTime = 0; // Reset any auto cycle
    currentlyInOnPhase = false;
    setPumpState(false); // Force pump off immediately
    logger.log("Pump set to manual OFF mode");
}

void PumpController::setAutoMode(bool enabled) {
    if (enabled) {
        status.state = PUMP_AUTO;
        cycleStartTime = 0; // Reset cycle to start fresh
        logger.log("Pump set to AUTO mode");
    } else {
        status.state = PUMP_OFF;
        setPumpState(false);
        logger.log("Pump AUTO mode disabled");
    }
    if (enabled != settingsManager.getPumpAutoMode()) {
        settingsManager.setPumpAutoMode(enabled);
        settingsManager.save();
    }
}

void PumpController::forceCycle() {
    if (status.state == PUMP_AUTO) {
        cycleStartTime = 0; // Reset to start new cycle immediately
        logger.log("Pump cycle forced to restart");
    }
}

unsigned long PumpController::getCurrentRunStartTime() const {
    if (status.is_active && status.current_cycle_start > 0) {
        return status.current_cycle_start;
    }
    return 0;
}

unsigned long PumpController::getCurrentCycleTime() const {
    if (status.current_cycle_start > 0) {
        return millis() - status.current_cycle_start;
    }
    return 0;
}

unsigned long PumpController::getTimeUntilNextSwitch() const {
    if (status.state != PUMP_AUTO || cycleStartTime == 0) {
        return 0;
    }
    
    unsigned long currentTime = millis();
    unsigned long cycleElapsed = currentTime - cycleStartTime;
    int onTime = settingsManager.getPumpOnTimeSeconds() * 1000;
    int offTime = settingsManager.getPumpOffTimeSeconds() * 1000;
    
    if (currentlyInOnPhase) {
        return (onTime - cycleElapsed) > 0 ? (onTime - cycleElapsed) : 0;
    } else {
        return (onTime + offTime) > cycleElapsed ? ((onTime + offTime) - cycleElapsed) : 0;
    }
}

String PumpController::getStateString() const {
    // Show actual pump state including error condition
    switch (status.state) {
        case PUMP_ERROR:
            return "ERROR";
        case PUMP_AUTO:
            return "AUTO";
        case PUMP_ON:
            return "ON";
        case PUMP_OFF:
        default:
            return "OFF";
    }
}

String PumpController::getStatusJson() const {
    String json = "{";
    json += "\"state\":\"" + getStateString() + "\",";
    json += "\"is_active\":" + String(status.is_active ? "true" : "false") + ",";
    if (isnan(status.temperature_f)) {
        json += "\"temperature_f\":null,";
    } else {
        json += "\"temperature_f\":" + String(status.temperature_f, 1) + ",";
    }
    json += "\"temperature_below_threshold\":" + String(status.temperature_below_threshold ? "true" : "false") + ",";
    json += "\"flow_error\":" + String(status.flow_error ? "true" : "false") + ",";
    json += "\"current_cycle_time\":" + String(getCurrentCycleTime() / 1000) + ",";
    json += "\"time_until_next_switch\":" + String(getTimeUntilNextSwitch() / 1000) + ",";
    json += "\"time_until_retry\":" + String(status.time_until_retry / 1000) + ",";
    json += "\"total_on_time\":" + String(status.total_on_time / 1000) + ",";
    json += "\"total_off_time\":" + String(status.total_off_time / 1000) + ",";
    json += "\"total_cycles\":" + String(status.total_cycles);
    json += "}";
    return json;
}

void PumpController::resetStatistics() {
    status.total_on_time = 0;
    status.total_cycles = 0;
    status.current_cycle_start = 0;
    status.current_cycle_duration = 0;
    logger.log("Pump statistics reset");
}

void PumpController::clearFlowError() {
    status.flow_error = false; // Also clear the status flow_error
    errorStartTime = 0;
    waitingForRetry = false;
    logger.log("Flow error cleared");
}