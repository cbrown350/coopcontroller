#include "PumpController.h"
#include "SettingsManager.h"
#include "Logger.h"
#include <Arduino.h>
#include <stdint.h>
#include <algorithm>
#include <climits>


void PumpController::begin(SensorManager* primarySensor, SensorManager* flowSensor, uint8_t pin) {
    primarySensor_ = primarySensor;
    flowSensor_ = flowSensor;
    pumpPin = pin;
    
    // Initialize status
    status.state = PumpState::PUMP_OFF;
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

    // Initialize pump off flow monitoring
    pump_off_flow_monitoring_enabled = settingsManager.getPumpOffFlowMonitoringEnabled();
    pump_off_flow_grace_period_seconds = settingsManager.getPumpOffFlowGracePeriodSeconds();
    pump_turned_off_time = 0;
    pump_off_flow_detected = false;
    status.pump_off_flow_detected = false;

    pinMode(pumpPin, OUTPUT);
    digitalWrite(pumpPin, LOW);
    
    logger.logfDebug("OUT_PUMP_PIN: %d", OUT_PUMP_PIN);
    logger.logfDebug("PumpController initialized with pin %d", pumpPin);
    logger.logfDebug("PumpController begin - pin %d set as OUTPUT, initial state LOW", pumpPin);
    
    logger.logInfo("Pump controller initialized");
    logger.logfInfo("Pump pin: %d", pumpPin);
    
    // Start with pump off
    setPumpState(false);
    if (settingsManager.getPumpAutoMode()) {
        status.state = PumpState::PUMP_AUTO;
    } else {
        status.state = PumpState::PUMP_OFF;
    }

    logger.logfDebug("PumpController begin - mode set to %s", settingsManager.getPumpAutoMode() ? "AUTO" : "MANUAL");
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
        case PumpState::PUMP_OFF:
            // Manual off state - do nothing
            break;
            
        case PumpState::PUMP_ON:
            // Manual on state - keep pump on
            if (!status.is_active) {
                setPumpState(true);
                clearFlowError();
            }
            break;
            
        case PumpState::PUMP_AUTO:
            // Automatic mode - control based on temperature and cycling
            handleAutoMode(currentTime);
            break;
            
        case PumpState::PUMP_ERROR:
            // Error state - turn off pump
            if (status.is_active) {
                setPumpState(false);
                logger.logError("Pump turned off due to error condition");
            }
            break;
    }
    
    // Check for pump off flow monitoring
    checkPumpOffFlow(currentTime);
    
    lastUpdateTime = currentTime;
}

bool PumpController::checkFlowError() const {
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
        lastPulseTime = std::max(lastPulseTime, sensor1Data.last_pulse_time.load());
    }
    
    // Check sensor 2
    if (flowSensor_->getSensor2Type() == SensorType::WATER_METER) {
        hasWaterMeter = true;
        SensorData sensor2Data = flowSensor_->getSensor2Data();
        lastPulseTime = std::max(lastPulseTime, sensor2Data.last_pulse_time.load());
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
    if (pumpRunTime >= timeoutMs && (currentTimeMs - lastPulseTime) >= timeoutMs) { // NOSONAR - clearer decleared above
        logger.logWarning("Flow error detected: pump running without flow for timeout period");
        logger.logfDebug("Pump run time: %lu ms, Current time: %lu ms, Last pulse time: %lu ms, Timeout: %lu ms", pumpRunTime, currentTimeMs, lastPulseTime, timeoutMs);
        return true;
    }
    
    return false;
}

void PumpController::handleAutoMode(unsigned long currentTime) { // NOSONAR - complexity ok
    // Check if we have a valid temperature reading
    if (isnan(status.temperature_f)) {
        // No temperature sensor available - cannot do temperature-based control
        if (status.state == PumpState::PUMP_AUTO) {
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
            
            logger.logfInfo("Temperature below ON threshold (%.1f°F < %.1f°F), starting pump cycle - ON phase", status.temperature_f, onThreshold);
        } else {
            // Check if we need to switch phases
            unsigned long cycleElapsed = currentTime - cycleStartTime;
            int onTime = settingsManager.getPumpOnTimeSeconds() * 1000;
            int offTime = settingsManager.getPumpOffTimeSeconds() * 1000;
            
            if (currentlyInOnPhase) {
                // In ON phase - check if it's time to switch to OFF
                unsigned long timeUntilOff = onTime - cycleElapsed;
                if (cycleElapsed >= onTime) { // NOSONAR - nested ok
                    // Switch to off phase
                    currentlyInOnPhase = false;
                    offPhaseStartTime = currentTime;
                    setPumpState(false);
                    logger.logfInfo("Pump cycle: switching to OFF phase for %d seconds", settingsManager.getPumpOffTimeSeconds());
                } else if (cycleElapsed % 10000 < 1000) {
                    // Log countdown every 10 seconds in debug mode
                    logger.logfDebug("Pump ON phase: %d seconds remaining until OFF", (timeUntilOff + 500) / 1000);
                }
            } else {
                // In OFF phase - check if it's time to switch to ON
                if (cycleElapsed >= (onTime + offTime)) { // NOSONAR - nested ok
                    // Start new cycle
                    cycleStartTime = currentTime;
                    currentlyInOnPhase = true;
                    setPumpState(true);
                    clearFlowError();
                    status.total_cycles++;
                    logger.logfInfo("Starting new pump cycle, temperature: %.1f°F - ON phase", status.temperature_f);
                } else if (cycleElapsed % 10000 < 1000) {
                    // Log countdown every 10 seconds in debug mode
                    unsigned long timeUntilOn = (onTime + offTime) - cycleElapsed;
                    logger.logfDebug("Pump OFF phase: %d seconds remaining until ON", (timeUntilOn + 500) / 1000);
                }
            }
        }
    } 
    if (status.temperature_f > offThreshold) {
        // Temperature is above OFF threshold - turn off pump and reset cycle
        if (status.is_active || cycleStartTime != 0) {
            setPumpState(false);
            // no clearFlowError(); // don't clear, keep flow error flag so it's known it happened
            cycleStartTime = 0; // Reset cycle
            currentlyInOnPhase = false;
            logger.logfInfo("Temperature above OFF threshold (%.1f°F > %.1f°F), pump turned off and cycle reset", status.temperature_f, offThreshold);
        }
        status.temperature_below_threshold = false;
    }
    
    // Check for flow error during pump operation - handle error without changing state
    if (status.is_active && status.flow_error && status.state == PumpState::PUMP_AUTO) {
        // Don't change state to ERROR, keep as AUTO but handle error logic
        errorStartTime = currentTime;
        waitingForRetry = true;
        setPumpState(false); // Turn off pump but keep AUTO state
        logger.logWarning("Flow error detected during pump operation, pump turned off but staying in AUTO state");
    }
}

void PumpController::setPumpState(bool isOn) {
    logger.logfDebug("Setting pump pin %d to %s", pumpPin, isOn ? "HIGH (ON)" : "LOW (OFF)");
    
    digitalWrite(pumpPin, isOn ? HIGH : LOW);
    
    if (status.is_active != isOn) {
        status.is_active = isOn;
        status.last_switch_time = millis();
        
        if (isOn) {
            status.current_cycle_start = millis();
            logger.logInfo("Pump turned ON");
            logger.logDebug("Pump turned ON - cycle started");
            // Clear pump off flow detection when pump turns on
            pump_off_flow_detected = false;
            status.pump_off_flow_detected = false;
        } else {
            if (status.current_cycle_start > 0) {
                status.current_cycle_duration = millis() - status.current_cycle_start;
            }
            logger.logInfo("Pump turned OFF");
            logger.logfDebug("Pump turned OFF - cycle duration: %lu ms", (unsigned long)status.current_cycle_duration);
            // Record when pump turned off for flow monitoring
            pump_turned_off_time = millis();
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
    status.state = PumpState::PUMP_ON;
    logger.logInfo("Pump set to manual ON mode");
}

void PumpController::turnOff() {
    status.state = PumpState::PUMP_OFF;
    cycleStartTime = 0; // Reset any auto cycle
    currentlyInOnPhase = false;
    setPumpState(false); // Force pump off immediately
    logger.logInfo("Pump set to manual OFF mode");
}

void PumpController::setAutoMode(bool enabled) {
    if (enabled) {
        status.state = PumpState::PUMP_AUTO;
        cycleStartTime = 0; // Reset cycle to start fresh
        logger.logInfo("Pump set to AUTO mode");
    } else {
        status.state = PumpState::PUMP_OFF;
        setPumpState(false);
        logger.logInfo("Pump AUTO mode disabled");
    }
    if (enabled != settingsManager.getPumpAutoMode()) {
        settingsManager.setPumpAutoMode(enabled);
        settingsManager.save();
    }
}

void PumpController::forceCycle() {
    if (status.state == PumpState::PUMP_AUTO) {
        cycleStartTime = 0; // Reset to start new cycle immediately
        logger.logInfo("Pump cycle forced to restart");
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
    if (status.state != PumpState::PUMP_AUTO || cycleStartTime == 0) {
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
        case PumpState::PUMP_ERROR:
            return "ERROR";
        case PumpState::PUMP_AUTO:
            return "AUTO";
        case PumpState::PUMP_ON:
            return "ON";
        case PumpState::PUMP_OFF:
        default:
            return "OFF";
    }
}

String PumpController::getStatusJson() const {
    String json = R"({)";
    json += R"("state":")" + getStateString() + R"(",)";
    json += R"("is_active":)" + String(status.is_active ? "true" : "false") + ",";
    if (isnan(status.temperature_f)) {
        json += R"("temperature_f":null,)";
    } else {
        json += R"("temperature_f":)" + String(status.temperature_f, 1) + ",";
    }
    json += R"("temperature_below_threshold":)" + String(status.temperature_below_threshold ? "true" : "false") + ",";
    json += R"("flow_error":)" + String(status.flow_error ? "true" : "false") + ",";
    json += R"("current_cycle_time":)" + String(getCurrentCycleTime() / 1000) + ",";
    json += R"("time_until_next_switch":)" + String(getTimeUntilNextSwitch() / 1000) + ",";
    json += R"("time_until_retry":)" + String(status.time_until_retry / 1000) + ",";
    json += R"("total_on_time":)" + String(status.total_on_time / 1000) + ",";
    json += R"("total_off_time":)" + String(status.total_off_time / 1000) + ",";
    json += R"("total_cycles":)" + String(status.total_cycles);
    json += R"("pump_off_flow_detected":)" + String(status.pump_off_flow_detected ? "true" : "false");
    json += "}";
    return json;
}

void PumpController::resetStatistics() {
    status.total_on_time = 0;
    status.total_cycles = 0;
    status.current_cycle_start = 0;
    status.current_cycle_duration = 0;
    logger.logInfo("Pump statistics reset");
}

void PumpController::clearFlowError() {
    status.flow_error = false; // Also clear the status flow_error
    errorStartTime = 0;
    waitingForRetry = false;
    logger.logInfo("Flow error cleared");
}

void PumpController::clearPumpOffFlowDetected() {
    pump_off_flow_detected = false;
    status.pump_off_flow_detected = false;
    logger.logInfo("Pump off flow detection cleared");
}

void PumpController::checkPumpOffFlow(unsigned long currentTime) {
    // Only check if monitoring is enabled
    if (!pump_off_flow_monitoring_enabled) {
        return;
    }
    
    // Only check when pump is OFF
    if (status.is_active) {
        return;
    }
    
    // Check if grace period has elapsed
    if (pump_turned_off_time == 0) {
        return; // Pump hasn't turned off yet
    }
    
    unsigned long gracePeriodMs = (unsigned long)pump_off_flow_grace_period_seconds * 1000UL;
    
    // Handle millis() rollover
    unsigned long timeSinceOff = 0;
    if (currentTime >= pump_turned_off_time) {
        timeSinceOff = currentTime - pump_turned_off_time;
    } else {
        // Rollover occurred
        timeSinceOff = (ULONG_MAX - pump_turned_off_time) + currentTime;
    }
    
    // Only check after grace period has elapsed
    if (timeSinceOff < gracePeriodMs) {
        return; // Still in grace period
    }
    
    // Check for flow from flow sensor
    if (flowSensor_) {
        float flowRate = 0.0f;
        
        // Check sensor 1 flow rate
        if (flowSensor_->getSensor1Type() == SensorType::WATER_METER) {
            flowRate = flowSensor_->getFlowRate1();
        }
        
        // Check sensor 2 flow rate if sensor 1 is not a water meter
        if (flowSensor_->getSensor2Type() == SensorType::WATER_METER && flowRate == 0.0f) {
            flowRate = flowSensor_->getFlowRate2();
        }
        
        // If flow detected and not already flagged
        if (flowRate > 0.0f && !pump_off_flow_detected) {
            pump_off_flow_detected = true;
            status.pump_off_flow_detected = true;
            logger.logWarning("WARNING: Water flow detected while pump is OFF - Possible stuck relay or valve leak");
        }
    }
}