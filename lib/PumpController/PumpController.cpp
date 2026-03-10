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
    status.time_until_retry = 0;
    status.total_on_time = 0;
    status.total_off_time = 0;
    status.total_cycles = 0;
    
    // Initialize timing variables
    lastUpdateTime = 0;
    cycleStartTime = 0;
    cyclingActive = false;
    currentlyInOnPhase = false;
    offPhaseStartTime = 0;
    
    // Initialize error detection
    lastFlowCheckTime = 0;
    errorStartTime = 0;
    waitingForRetry = false;

    // Initialize pump off flow monitoring
    pump_off_flow_monitoring_enabled = settingsManager.getPumpOffFlowMonitoringEnabled();
    pump_off_flow_grace_period_seconds = settingsManager.getPumpOffFlowGracePeriodSeconds();
    // Set pump_turned_off_time to now since pump starts in OFF state
    // This allows pump-off flow monitoring to work from initialization
    pump_turned_off_time = millis();
    pump_has_been_off = true; // Pump starts in OFF state
    pump_off_flow_detected = false;
    status.pump_off_flow_detected = false;
    pump_off_pulse_count_at_start = 0;

    // Initialize scheduled maintenance cycle tracking
    lastCompletedCycleTime_ = millis();
    scheduledCycleActive_ = false;
    scheduledCycleStartTime_ = 0;
    status.scheduled_cycle_active = false;
    status.time_until_next_scheduled = 0;

    // Initialize trigger source tracking
    lastTriggerSource_ = TriggerSource::STARTUP;

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
            handleScheduledCycles(currentTime);
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

    // Update time until retry
    if (status.flow_error && waitingForRetry && errorStartTime > 0) {
        unsigned long retryDelayMs = (unsigned long)settingsManager.getWaterFlowErrorTimeoutSeconds() * 1000UL;
        unsigned long timeSinceError = currentTime - errorStartTime;
        if (timeSinceError < retryDelayMs) {
            status.time_until_retry = retryDelayMs - timeSinceError;
        } else {
            status.time_until_retry = 0;
        }
    } else {
        status.time_until_retry = 0;
    }

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

    unsigned long pumpRunStart = status.current_cycle_start;
    unsigned long currentTimeMs = millis();
    // Calculate pump run time - we know pump is active from check above
    // Handle the case where pump started at time 0
    unsigned long pumpRunTime = currentTimeMs - pumpRunStart;
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
            cyclingActive = false;
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
        if (!cyclingActive) {
            // Start new cycle
            cycleStartTime = currentTime;
            cyclingActive = true;
            currentlyInOnPhase = true;
            lastTriggerSource_ = TriggerSource::TEMP_THRESHOLD;
            setPumpState(true);
            clearFlowError();
            status.total_cycles++;

            logger.logfInfo("Temperature below ON threshold (%.1f°F < %.1f°F), starting pump cycle - ON phase", status.temperature_f, onThreshold);
        } else {
            // Check if we need to switch phases
            unsigned long cycleElapsed = currentTime - cycleStartTime;
            unsigned long onTime = settingsManager.getPumpOnTimeSeconds() * 1000UL;
            unsigned long offTime = settingsManager.getPumpOffTimeSeconds() * 1000UL;

            if (currentlyInOnPhase) {
                // In ON phase - check if it's time to switch to OFF
                unsigned long timeUntilOff = onTime - cycleElapsed;
                if (cycleElapsed >= onTime) { // NOSONAR - nested ok
                    // Switch to off phase
                    currentlyInOnPhase = false;
                    offPhaseStartTime = currentTime;
                    lastTriggerSource_ = TriggerSource::TEMP_CYCLE;
                    setPumpState(false);
                    logger.logfInfo("Pump cycle: switching to OFF phase for %u seconds", settingsManager.getPumpOffTimeSeconds());
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
                    lastTriggerSource_ = TriggerSource::TEMP_CYCLE;
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
        // Don't interfere with scheduled maintenance cycles (handled separately)
        if ((status.is_active && !scheduledCycleActive_) || cyclingActive) {
            lastTriggerSource_ = TriggerSource::TEMP_THRESHOLD;
            setPumpState(false);
            // no clearFlowError(); // don't clear, keep flow error flag so it's known it happened
            cycleStartTime = 0; // Reset cycle
            cyclingActive = false;
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
        // Reset cycling state so it can restart fresh after error is cleared
        cyclingActive = false;
        currentlyInOnPhase = false;
        cycleStartTime = 0;
        logger.logWarning("Flow error detected during pump operation, pump turned off but staying in AUTO state");
    }
}

void PumpController::setPumpState(bool isOn) {
    digitalWrite(pumpPin, isOn ? HIGH : LOW);

    if (status.is_active != isOn) {
        status.is_active = isOn;
        status.last_switch_time = millis();

        if (isOn) {
            status.current_cycle_start = millis();
            logger.logInfo("Pump turned ON");
            // Clear pump off flow detection when pump turns on
            pump_off_flow_detected = false;
            status.pump_off_flow_detected = false;
            pump_off_pulse_count_at_start = 0;
        } else {
            if (status.current_cycle_start > 0) {
                status.current_cycle_duration = millis() - status.current_cycle_start;
            }
            logger.logfInfo("Pump turned OFF (cycle duration: %lu ms)", (unsigned long)status.current_cycle_duration);
            // Record when pump turned off for flow monitoring
            pump_turned_off_time = millis();
            pump_has_been_off = true;
            pump_off_pulse_count_at_start = 0;
            // Update last completed cycle time for scheduled maintenance tracking
            lastCompletedCycleTime_ = millis();
        }
    }
}

void PumpController::updateStatistics() {
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - lastUpdateTime;

    // Accumulate time based on current pump state
    if (status.is_active) {
        status.total_on_time += elapsed;
    } else {
        status.total_off_time += elapsed;
    }
}

void PumpController::turnOn(TriggerSource trigger) {
    status.state = PumpState::PUMP_ON;
    scheduledCycleActive_ = false;
    status.scheduled_cycle_active = false;
    lastTriggerSource_ = trigger;
    logger.logfInfo("Pump set to manual ON mode (trigger: %s)", triggerSourceToString(trigger).c_str());
}

void PumpController::turnOff(TriggerSource trigger) {
    status.state = PumpState::PUMP_OFF;
    cycleStartTime = 0; // Reset any auto cycle
    cyclingActive = false;
    currentlyInOnPhase = false;
    scheduledCycleActive_ = false;
    status.scheduled_cycle_active = false;
    setPumpState(false); // Force pump off immediately
    lastTriggerSource_ = trigger;
    logger.logfInfo("Pump set to manual OFF mode (trigger: %s)", triggerSourceToString(trigger).c_str());
}

void PumpController::setAutoMode(bool enabled, TriggerSource trigger) {
    lastTriggerSource_ = trigger;
    if (enabled) {
        status.state = PumpState::PUMP_AUTO;
        cycleStartTime = 0; // Reset cycle to start fresh
        cyclingActive = false;
        scheduledCycleActive_ = false;
        status.scheduled_cycle_active = false;
        logger.logfInfo("Pump set to AUTO mode (trigger: %s)", triggerSourceToString(trigger).c_str());
    } else {
        status.state = PumpState::PUMP_OFF;
        cyclingActive = false;
        scheduledCycleActive_ = false;
        status.scheduled_cycle_active = false;
        setPumpState(false);
        logger.logfInfo("Pump AUTO mode disabled (trigger: %s)", triggerSourceToString(trigger).c_str());
    }
    if (enabled != settingsManager.getPumpAutoMode()) {
        settingsManager.setPumpAutoMode(enabled);
        settingsManager.save();
    }
}

void PumpController::forceCycle(TriggerSource trigger) {
    if (status.state == PumpState::PUMP_AUTO) {
        cycleStartTime = 0; // Reset to start new cycle immediately
        cyclingActive = false;
        scheduledCycleActive_ = false;
        status.scheduled_cycle_active = false;
        lastTriggerSource_ = trigger;
        logger.logfInfo("Pump cycle forced to restart (trigger: %s)", triggerSourceToString(trigger).c_str());
    }
}

unsigned long PumpController::getCurrentRunStartTime() const {
    if (status.is_active) {
        return status.current_cycle_start;
    }
    return 0;
}

unsigned long PumpController::getCurrentCycleTime() const {
    if (status.is_active) {
        return millis() - status.current_cycle_start;
    }
    return 0;
}

unsigned long PumpController::getTimeUntilNextSwitch() const {
    if (status.state != PumpState::PUMP_AUTO || !cyclingActive) {
        return 0;
    }
    
    unsigned long currentTime = millis();
    unsigned long cycleElapsed = currentTime - cycleStartTime;
    unsigned long onTime = settingsManager.getPumpOnTimeSeconds() * 1000UL;
    unsigned long offTime = settingsManager.getPumpOffTimeSeconds() * 1000UL;
    
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
    json += R"("total_cycles":)" + String(status.total_cycles) + ",";
    json += R"("pump_off_flow_detected":)" + String(status.pump_off_flow_detected ? "true" : "false") + ",";
    json += R"("scheduled_cycle_active":)" + String(status.scheduled_cycle_active ? "true" : "false") + ",";
    json += R"("time_until_next_scheduled":)" + String(status.time_until_next_scheduled / 1000);
    json += "}";
    return json;
}

void PumpController::resetStatistics() {
    status.total_on_time = 0;
    status.total_off_time = 0;
    status.total_cycles = 0;
    status.current_cycle_start = 0;
    status.current_cycle_duration = 0;
    lastCompletedCycleTime_ = millis();
    scheduledCycleActive_ = false;
    status.scheduled_cycle_active = false;
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
    
    // Check if pump has been off (handles time=0 case)
    if (!pump_has_been_off) {
        return; // Pump hasn't turned off yet
    }
    
    unsigned long gracePeriodMs = (unsigned long)pump_off_flow_grace_period_seconds * 1000UL;
    
    // Handle millis() rollover and same-cycle timing
    unsigned long timeSinceOff = 0;
    if (currentTime >= pump_turned_off_time) {
        timeSinceOff = currentTime - pump_turned_off_time;
    } else {
        // Check if this is a real rollover or just same-cycle timing
        // (pump turned off during current update() after currentTime was captured)
        unsigned long diff = pump_turned_off_time - currentTime;
        if (diff < 1000) {
            // Small difference means pump just turned off in this cycle
            timeSinceOff = 0;
        } else {
            // Large difference means real millis() rollover occurred (~49 days)
            timeSinceOff = (ULONG_MAX - pump_turned_off_time) + currentTime;
        }
    }
    
    // Only check after grace period has elapsed
    if (timeSinceOff < gracePeriodMs) {
        return; // Still in grace period
    }
    
    // Check for flow from flow sensor using pulse count threshold
    if (flowSensor_) {
        // Get current total pulse count across all water meter sensors
        unsigned long currentPulses = 0;
        if (flowSensor_->getSensor1Type() == SensorType::WATER_METER) {
            currentPulses += flowSensor_->getPulseCount1();
        }
        if (flowSensor_->getSensor2Type() == SensorType::WATER_METER) {
            currentPulses += flowSensor_->getPulseCount2();
        }

        // On first check after grace period, record baseline pulse count
        if (pump_off_pulse_count_at_start == 0) {
            pump_off_pulse_count_at_start = currentPulses;
            return;
        }

        // Check if accumulated pulses exceed threshold
        unsigned long pulseDelta = currentPulses - pump_off_pulse_count_at_start;
        unsigned int threshold = settingsManager.getPumpOffFlowPulseThreshold();
        if (pulseDelta >= threshold && !pump_off_flow_detected) {
            pump_off_flow_detected = true;
            status.pump_off_flow_detected = true;
            logger.logfWarning("Leak detected: %lu pulses while pump OFF (threshold: %u)", pulseDelta, threshold);
        }
    }
}

void PumpController::handleScheduledCycles(unsigned long currentTime) {
    // Only run in AUTO mode with feature enabled
    if (status.state != PumpState::PUMP_AUTO || !settingsManager.getPumpMinDailyCyclesEnabled()) {
        status.time_until_next_scheduled = 0;
        return;
    }

    unsigned int cyclesPerDay = settingsManager.getPumpMinDailyCycles();
    unsigned long intervalMs = (24UL * 3600UL * 1000UL) / cyclesPerDay;
    unsigned long runDurationMs = (unsigned long)settingsManager.getPumpMinCycleRunSeconds() * 1000UL;
    unsigned long timeSinceLastCycle = currentTime - lastCompletedCycleTime_;

    // If a scheduled cycle is currently active
    if (scheduledCycleActive_) {
        // Check if temperature cycling has taken over (handleAutoMode started cycling)
        if (cyclingActive) {
            // Temperature cycling took over - hand off
            scheduledCycleActive_ = false;
            status.scheduled_cycle_active = false;
            status.time_until_next_scheduled = 0;
            return;
        }

        // Check if scheduled cycle duration has elapsed
        unsigned long cycleElapsed = currentTime - scheduledCycleStartTime_;
        if (cycleElapsed >= runDurationMs) {
            // Scheduled cycle complete
            lastTriggerSource_ = TriggerSource::MAINTENANCE_CYCLE;
            setPumpState(false);
            scheduledCycleActive_ = false;
            status.scheduled_cycle_active = false;
            status.time_until_next_scheduled = intervalMs;
            logger.logInfo("Scheduled maintenance pump cycle complete");
            return;
        }

        status.time_until_next_scheduled = 0;
        return;
    }

    // Not in a scheduled cycle - check if we should start one
    // Don't start if temperature cycling is active (pump already running from temp)
    if (cyclingActive || status.is_active) {
        status.time_until_next_scheduled = 0;
        return;
    }

    // Don't start if there's a flow error
    if (status.flow_error) {
        status.time_until_next_scheduled = 0;
        return;
    }

    // Check if interval has elapsed since last completed cycle
    if (timeSinceLastCycle >= intervalMs) {
        // Start a scheduled maintenance cycle
        scheduledCycleActive_ = true;
        scheduledCycleStartTime_ = currentTime;
        status.scheduled_cycle_active = true;
        status.time_until_next_scheduled = 0;
        lastTriggerSource_ = TriggerSource::MAINTENANCE_CYCLE;
        setPumpState(true);
        logger.logInfo("Starting scheduled maintenance pump cycle");
    } else {
        // Update countdown
        status.time_until_next_scheduled = intervalMs - timeSinceLastCycle;
    }
}