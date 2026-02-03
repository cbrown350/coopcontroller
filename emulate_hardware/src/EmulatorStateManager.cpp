#include "EmulatorStateManager.h"

void EmulatorStateManager::begin() {
    // Configure input pins (reading main controller outputs)
    pinMode(EMU_READ_PUMP_PIN, INPUT);
    pinMode(EMU_READ_LIGHT_PIN, INPUT);
    pinMode(EMU_READ_DOOR_POS_PIN, INPUT);
    pinMode(EMU_READ_DOOR_NEG_PIN, INPUT);
    pinMode(EMU_READ_BUZZER_PIN, INPUT);
    pinMode(EMU_READ_LED_PIN, INPUT);

    // Configure output pins (driving main controller inputs)
    pinMode(EMU_WATER_PULSE1_PIN, OUTPUT);
    pinMode(EMU_WATER_PULSE2_PIN, OUTPUT);
    pinMode(EMU_HALL_OPEN_PIN, OUTPUT);
    pinMode(EMU_HALL_CLOSE_PIN, OUTPUT);
    pinMode(EMU_MANUAL_SW_PIN, OUTPUT);
    pinMode(EMU_DOOR_FAULT_PIN, OUTPUT);

    // Configure status LEDs
    pinMode(EMU_STATUS_LED_PIN, OUTPUT);
    pinMode(EMU_WIFI_LED_PIN, OUTPUT);

    // Initialize outputs to inactive state
    // Water pulses: HIGH = no pulse (main controller uses pullup)
    digitalWrite(EMU_WATER_PULSE1_PIN, HIGH);
    digitalWrite(EMU_WATER_PULSE2_PIN, HIGH);

    // Hall sensors: HIGH = not triggered (active low)
    digitalWrite(EMU_HALL_OPEN_PIN, HIGH);
    digitalWrite(EMU_HALL_CLOSE_PIN, HIGH);

    // Manual switch: HIGH = not pressed (active low with pullup on main)
    digitalWrite(EMU_MANUAL_SW_PIN, HIGH);

    // Door fault: HIGH = no fault (active low)
    digitalWrite(EMU_DOOR_FAULT_PIN, HIGH);

    // Status LED on to indicate boot
    digitalWrite(EMU_STATUS_LED_PIN, HIGH);

    // Initialize door to closed position
    _emulated.doorPosition = DOOR_POSITION_CLOSED;
    _emulated.doorState = DoorState::CLOSED;
    _emulated.hallCloseActive = true;

    Serial.println("[EmulatorState] Initialized");
}

void EmulatorStateManager::update() {
    uint32_t now = millis();

    // Sample input signals at configured interval
    if (now - _lastSampleTime >= SIGNAL_SAMPLE_INTERVAL_MS) {
        sampleInputs();
        _lastSampleTime = now;
    }

    // Update door simulation
    updateDoorSimulation();

    // Update water pulse generation
    updateWaterPulses();

    // Update manual switch release
    updateManualSwitch();

    // Update hall sensor outputs based on door position
    updateHallSensors();

    // Write emulated outputs to pins
    outputEmulatedSignals();
}

void EmulatorStateManager::sampleInputs() {
    // Read pump state
    bool newPumpState = digitalRead(EMU_READ_PUMP_PIN) == HIGH;
    if (newPumpState != _monitored.pumpActive) {
        _monitored.pumpActive = newPumpState;
        Serial.printf("[EmulatorState] Pump: %s\n", newPumpState ? "ON" : "OFF");
    }

    // Read light state and estimate brightness via PWM timing
    bool lightState = digitalRead(EMU_READ_LIGHT_PIN) == HIGH;
    uint32_t now = millis();

    if (lightState != _lastLightState) {
        uint32_t duration = now - _lastLightChange;
        if (_lastLightState) {
            _lightPwmHighTime = duration;
        } else {
            _lightPwmLowTime = duration;
        }
        _lastLightChange = now;
        _lastLightState = lightState;

        // Estimate duty cycle
        if (_lightPwmHighTime + _lightPwmLowTime > 0) {
            uint32_t total = _lightPwmHighTime + _lightPwmLowTime;
            _monitored.lightBrightness = (_lightPwmHighTime * 100) / total;
        }
    }

    // If light has been steady for a while, it's either fully on or off
    if (now - _lastLightChange > PWM_SAMPLE_WINDOW_MS) {
        _monitored.lightBrightness = lightState ? 100 : 0;
    }
    _monitored.lightActive = _monitored.lightBrightness > 0;

    // Read door motor signals
    bool newDoorPos = digitalRead(EMU_READ_DOOR_POS_PIN) == HIGH;
    bool newDoorNeg = digitalRead(EMU_READ_DOOR_NEG_PIN) == HIGH;

    if (newDoorPos != _monitored.doorMotorPosActive ||
        newDoorNeg != _monitored.doorMotorNegActive) {

        _monitored.doorMotorPosActive = newDoorPos;
        _monitored.doorMotorNegActive = newDoorNeg;
        _monitored.motorDirection = calculateMotorDirection(newDoorPos, newDoorNeg);

        Serial.printf("[EmulatorState] Motor: POS=%d NEG=%d DIR=%s\n",
                      newDoorPos, newDoorNeg,
                      motorDirectionToString(_monitored.motorDirection));
    }

    // Read buzzer state (active low) with pattern tracking
    bool newBuzzerState = digitalRead(EMU_READ_BUZZER_PIN) == LOW;
    if (newBuzzerState != _monitored.buzzerActive) {
        _monitored.buzzerActive = newBuzzerState;
        if (newBuzzerState) {
            _buzzerOnTime = now;
        }
        Serial.printf("[EmulatorState] Buzzer: %s\n", newBuzzerState ? "ON" : "OFF");
    }
    if (_monitored.buzzerActive) {
        _monitored.buzzerOnDuration = now - _buzzerOnTime;
    }
    updateBuzzerPattern(newBuzzerState);

    // Read WiFi LED state (active low) with pattern tracking
    bool newLedState = digitalRead(EMU_READ_LED_PIN) == LOW;
    if (newLedState != _monitored.wifiLedActive) {
        _monitored.wifiLedActive = newLedState;
    }
    updateLedPattern(newLedState);
}

void EmulatorStateManager::updateDoorSimulation() {
    if (!_config.autoSimulateDoor) {
        return;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - _lastDoorUpdateTime;
    _lastDoorUpdateTime = now;

    // Calculate position change based on motor direction
    MotorDirection dir = _monitored.motorDirection;

    if (dir == MotorDirection::OPENING && _emulated.doorPosition < DOOR_POSITION_OPEN) {
        // Calculate how much to move based on travel time
        float positionPerMs = 100.0f / _config.doorTravelTimeMs;
        float delta = positionPerMs * elapsed;
        _emulated.doorPosition = min(DOOR_POSITION_OPEN,
                                     (uint8_t)(_emulated.doorPosition + delta));
        _emulated.doorState = DoorState::OPENING;

        if (_emulated.doorPosition >= DOOR_POSITION_OPEN) {
            _emulated.doorState = DoorState::OPEN;
            Serial.println("[EmulatorState] Door reached OPEN position");
        }
    }
    else if (dir == MotorDirection::CLOSING && _emulated.doorPosition > DOOR_POSITION_CLOSED) {
        float positionPerMs = 100.0f / _config.doorTravelTimeMs;
        float delta = positionPerMs * elapsed;
        int newPos = static_cast<int>(_emulated.doorPosition) - static_cast<int>(delta);
        _emulated.doorPosition = static_cast<uint8_t>(max(0, newPos));
        _emulated.doorState = DoorState::CLOSING;

        if (_emulated.doorPosition <= DOOR_POSITION_CLOSED) {
            _emulated.doorState = DoorState::CLOSED;
            Serial.println("[EmulatorState] Door reached CLOSED position");
        }
    }
    else if (dir == MotorDirection::STOPPED || dir == MotorDirection::BRAKE) {
        // Motor stopped - update state based on position
        if (_emulated.doorPosition >= DOOR_POSITION_OPEN - DOOR_HALL_TRIGGER_THRESHOLD) {
            _emulated.doorState = DoorState::OPEN;
        }
        else if (_emulated.doorPosition <= DOOR_POSITION_CLOSED + DOOR_HALL_TRIGGER_THRESHOLD) {
            _emulated.doorState = DoorState::CLOSED;
        }
        else if (_emulated.doorState == DoorState::OPENING ||
                 _emulated.doorState == DoorState::CLOSING) {
            _emulated.doorState = DoorState::STOPPED;
        }
    }
}

void EmulatorStateManager::updateWaterPulses() {
    uint32_t now = millis();

    // Handle pulse in progress
    if (_pulseInProgress) {
        if (now - _pulseStartTime >= PULSE_DURATION_MS) {
            // End the pulse
            if (_pulseChannel == 1) {
                digitalWrite(EMU_WATER_PULSE1_PIN, HIGH);
            } else if (_pulseChannel == 2) {
                digitalWrite(EMU_WATER_PULSE2_PIN, HIGH);
            }
            _pulseInProgress = false;
        }
        return;  // Don't start new pulse while one is in progress
    }

    // Auto-generate pulses when pump is running
    if (!_config.autoGeneratePulses || !_monitored.pumpActive) {
        return;
    }

    // Check if flow is simulated as blocked
    if (_config.simulateFrozenLine) {
        return;
    }

    // Calculate pulse interval based on flow rate
    // Flow rate in GPM, pulses per gallon
    // Pulses per second = (GPM * pulsesPerGallon) / 60
    float pulsesPerSecond = (_config.flowRateGPM * _config.pulsesPerGallon) / 60.0f;
    uint32_t pulseIntervalMs = (pulsesPerSecond > 0) ? (1000.0f / pulsesPerSecond) : 10000;

    if (now - _lastPulseTime >= pulseIntervalMs) {
        // Generate pulse on channel 1 (primary water meter)
        triggerSinglePulse(1);
        _lastPulseTime = now;
    }
}

void EmulatorStateManager::updateHallSensors() {
    if (_config.simulateDoorStuck) {
        // Hall sensors never trigger
        _emulated.hallOpenActive = false;
        _emulated.hallCloseActive = false;
        return;
    }

    // Hall open triggers when door is at open position (within threshold)
    _emulated.hallOpenActive =
        _emulated.doorPosition >= (DOOR_POSITION_OPEN - DOOR_HALL_TRIGGER_THRESHOLD);

    // Hall close triggers when door is at closed position (within threshold)
    _emulated.hallCloseActive =
        _emulated.doorPosition <= (DOOR_POSITION_CLOSED + DOOR_HALL_TRIGGER_THRESHOLD);
}

void EmulatorStateManager::updateManualSwitch() {
    uint32_t now = millis();

    // Update press duration while pressed
    if (_emulated.manualSwitch.isPressed && _emulated.manualSwitch.pressStartTime > 0) {
        _emulated.manualSwitch.pressDuration = now - _emulated.manualSwitch.pressStartTime;

        // Determine press type based on duration
        if (_emulated.manualSwitch.pressDuration >= _emulated.manualSwitch.longPressThresholdMs) {
            _emulated.manualSwitch.lastPressType = SwitchPressType::LONG;
        } else if (_emulated.manualSwitch.pressDuration >= _emulated.manualSwitch.shortPressThresholdMs) {
            _emulated.manualSwitch.lastPressType = SwitchPressType::SHORT;
        }
    }

    // Handle auto-release
    if (_emulated.manualSwitch.isPressed && _emulated.manualSwitch.autoReleaseTime > 0) {
        if (now >= _emulated.manualSwitch.autoReleaseTime) {
            _emulated.manualSwitch.isPressed = false;
            _emulated.manualSwitch.autoReleaseTime = 0;
            Serial.printf("[EmulatorState] Manual switch auto-released (type: %s, duration: %lu ms)\n",
                          _emulated.manualSwitch.lastPressType == SwitchPressType::LONG ? "LONG" : "SHORT",
                          _emulated.manualSwitch.pressDuration);
        }
    }
}

void EmulatorStateManager::outputEmulatedSignals() {
    // Check if manual override mode is enabled
    if (_config.manualOverrideEnabled) {
        // In manual override mode, use override states directly
        digitalWrite(EMU_HALL_OPEN_PIN, _config.overrideHallOpen ? LOW : HIGH);
        digitalWrite(EMU_HALL_CLOSE_PIN, _config.overrideHallClose ? LOW : HIGH);
        digitalWrite(EMU_MANUAL_SW_PIN, _config.overrideManualSwitch ? LOW : HIGH);
        digitalWrite(EMU_DOOR_FAULT_PIN, _config.overrideDoorFault ? LOW : HIGH);
        digitalWrite(EMU_WATER_PULSE1_PIN, _config.overrideWaterPulse1 ? LOW : HIGH);
        digitalWrite(EMU_WATER_PULSE2_PIN, _config.overrideWaterPulse2 ? LOW : HIGH);
        return;
    }

    // Normal mode - use emulated states
    // Hall sensors (active low - LOW means magnet detected)
    digitalWrite(EMU_HALL_OPEN_PIN, _emulated.hallOpenActive ? LOW : HIGH);
    digitalWrite(EMU_HALL_CLOSE_PIN, _emulated.hallCloseActive ? LOW : HIGH);

    // Manual switch (active low)
    digitalWrite(EMU_MANUAL_SW_PIN, _emulated.manualSwitch.isPressed ? LOW : HIGH);

    // Door fault (active low)
    bool faultOutput = _emulated.doorFaultActive || _config.injectDoorFault;
    digitalWrite(EMU_DOOR_FAULT_PIN, faultOutput ? LOW : HIGH);
}

// ============================================================================
// CONTROL METHODS
// ============================================================================

void EmulatorStateManager::setDoorPosition(uint8_t position) {
    _emulated.doorPosition = constrain(position, DOOR_POSITION_CLOSED, DOOR_POSITION_OPEN);
    updateHallSensors();

    Serial.printf("[EmulatorState] Door position set to %d%%\n", _emulated.doorPosition);
}

void EmulatorStateManager::setDoorState(DoorState state) {
    _emulated.doorState = state;

    // If setting to OPEN or CLOSED, also set position
    if (state == DoorState::OPEN) {
        _emulated.doorPosition = DOOR_POSITION_OPEN;
    } else if (state == DoorState::CLOSED) {
        _emulated.doorPosition = DOOR_POSITION_CLOSED;
    }

    updateHallSensors();
    Serial.printf("[EmulatorState] Door state set to %s\n", doorStateToString(state));
}

void EmulatorStateManager::setWaterFlowEnabled(bool enabled) {
    _emulated.waterFlowEnabled = enabled;
    Serial.printf("[EmulatorState] Water flow: %s\n", enabled ? "ENABLED" : "DISABLED");
}

void EmulatorStateManager::setFlowRate(float gpm) {
    _config.flowRateGPM = gpm;
    _emulated.flowRateGPM = gpm;
    Serial.printf("[EmulatorState] Flow rate set to %.2f GPM\n", gpm);
}

void EmulatorStateManager::triggerSinglePulse(uint8_t channel) {
    if (_pulseInProgress) {
        return;  // Pulse already in progress
    }

    if (channel == 1) {
        digitalWrite(EMU_WATER_PULSE1_PIN, LOW);
        _emulated.channel1PulseCount++;
    } else if (channel == 2) {
        digitalWrite(EMU_WATER_PULSE2_PIN, LOW);
        _emulated.channel2PulseCount++;
    } else {
        return;
    }

    _pulseInProgress = true;
    _pulseStartTime = millis();
    _pulseChannel = channel;
}

void EmulatorStateManager::resetPulseCounters() {
    _emulated.channel1PulseCount = 0;
    _emulated.channel2PulseCount = 0;
    Serial.println("[EmulatorState] Pulse counters reset");
}

void EmulatorStateManager::pressManualSwitch() {
    _emulated.manualSwitch.isPressed = true;
    _emulated.manualSwitch.pressStartTime = millis();
    _emulated.manualSwitch.pressDuration = 0;
    _emulated.manualSwitch.autoReleaseTime = 0;  // Stay pressed until released
    _emulated.manualSwitch.lastPressType = SwitchPressType::NONE;
    Serial.println("[EmulatorState] Manual switch pressed");
}

void EmulatorStateManager::releaseManualSwitch() {
    uint32_t duration = _emulated.manualSwitch.pressDuration;

    // Determine final press type
    if (duration >= _emulated.manualSwitch.longPressThresholdMs) {
        _emulated.manualSwitch.lastPressType = SwitchPressType::LONG;
    } else if (duration >= _emulated.manualSwitch.shortPressThresholdMs) {
        _emulated.manualSwitch.lastPressType = SwitchPressType::SHORT;
    }

    _emulated.manualSwitch.isPressed = false;
    _emulated.manualSwitch.autoReleaseTime = 0;
    Serial.printf("[EmulatorState] Manual switch released (type: %s, duration: %lu ms)\n",
                  _emulated.manualSwitch.lastPressType == SwitchPressType::LONG ? "LONG" :
                  (_emulated.manualSwitch.lastPressType == SwitchPressType::SHORT ? "SHORT" : "NONE"),
                  duration);
}

void EmulatorStateManager::pulseManualSwitch(uint32_t durationMs) {
    _emulated.manualSwitch.isPressed = true;
    _emulated.manualSwitch.pressStartTime = millis();
    _emulated.manualSwitch.pressDuration = 0;
    _emulated.manualSwitch.autoReleaseTime = millis() + durationMs;
    _emulated.manualSwitch.lastPressType = SwitchPressType::SHORT;
    Serial.printf("[EmulatorState] Manual switch pulsed for %lu ms (short press)\n", durationMs);
}

void EmulatorStateManager::longPressManualSwitch(uint32_t durationMs) {
    _emulated.manualSwitch.isPressed = true;
    _emulated.manualSwitch.pressStartTime = millis();
    _emulated.manualSwitch.pressDuration = 0;
    _emulated.manualSwitch.autoReleaseTime = millis() + durationMs;
    _emulated.manualSwitch.lastPressType = SwitchPressType::LONG;
    Serial.printf("[EmulatorState] Manual switch long-pressed for %lu ms\n", durationMs);
}

uint32_t EmulatorStateManager::getCurrentPressDuration() const {
    if (!_emulated.manualSwitch.isPressed) {
        return _emulated.manualSwitch.pressDuration;  // Return last duration
    }
    return millis() - _emulated.manualSwitch.pressStartTime;
}

void EmulatorStateManager::setManualSwitchThresholds(uint32_t shortMs, uint32_t longMs) {
    _emulated.manualSwitch.shortPressThresholdMs = shortMs;
    _emulated.manualSwitch.longPressThresholdMs = longMs;
    _config.shortPressMs = shortMs;
    _config.longPressMs = longMs;
    Serial.printf("[EmulatorState] Manual switch thresholds: short=%lu ms, long=%lu ms\n", shortMs, longMs);
}

void EmulatorStateManager::setDoorFault(bool fault) {
    _emulated.doorFaultActive = fault;
    Serial.printf("[EmulatorState] Door fault: %s\n", fault ? "ACTIVE" : "CLEARED");
}

// ============================================================================
// MANUAL OVERRIDE METHODS
// ============================================================================

void EmulatorStateManager::setManualOverrideEnabled(bool enabled) {
    _config.manualOverrideEnabled = enabled;
    Serial.printf("[EmulatorState] Manual override mode: %s\n", enabled ? "ENABLED" : "DISABLED");

    if (!enabled) {
        // Clear all overrides when disabling
        clearAllOverrides();
    }
}

void EmulatorStateManager::setOverrideHallOpen(bool state) {
    _config.overrideHallOpen = state;
    if (_config.manualOverrideEnabled) {
        Serial.printf("[EmulatorState] Override hall open: %s\n", state ? "ACTIVE" : "INACTIVE");
    }
}

void EmulatorStateManager::setOverrideHallClose(bool state) {
    _config.overrideHallClose = state;
    if (_config.manualOverrideEnabled) {
        Serial.printf("[EmulatorState] Override hall close: %s\n", state ? "ACTIVE" : "INACTIVE");
    }
}

void EmulatorStateManager::setOverrideDoorFault(bool state) {
    _config.overrideDoorFault = state;
    if (_config.manualOverrideEnabled) {
        Serial.printf("[EmulatorState] Override door fault: %s\n", state ? "ACTIVE" : "INACTIVE");
    }
}

void EmulatorStateManager::setOverrideManualSwitch(bool state) {
    _config.overrideManualSwitch = state;
    if (_config.manualOverrideEnabled) {
        Serial.printf("[EmulatorState] Override manual switch: %s\n", state ? "PRESSED" : "RELEASED");
    }
}

void EmulatorStateManager::setOverrideWaterPulse(uint8_t channel, bool state) {
    if (channel == 1) {
        _config.overrideWaterPulse1 = state;
    } else if (channel == 2) {
        _config.overrideWaterPulse2 = state;
    }
    if (_config.manualOverrideEnabled) {
        Serial.printf("[EmulatorState] Override water pulse %d: %s\n", channel, state ? "LOW" : "HIGH");
    }
}

void EmulatorStateManager::clearAllOverrides() {
    _config.overrideHallOpen = false;
    _config.overrideHallClose = false;
    _config.overrideDoorFault = false;
    _config.overrideManualSwitch = false;
    _config.overrideWaterPulse1 = false;
    _config.overrideWaterPulse2 = false;
    Serial.println("[EmulatorState] All overrides cleared");
}

// ============================================================================
// PATTERN TRACKING METHODS
// ============================================================================

void EmulatorStateManager::updateBuzzerPattern(bool currentState) {
    uint32_t now = millis();

    // Check for pattern timeout (reset if idle too long)
    if (now - _buzzerLastTransition > PATTERN_TIMEOUT_MS) {
        _monitored.buzzerPattern.isBlinking = false;
        _monitored.buzzerPattern.cycleCount = 0;
        _buzzerPatternIndex = 0;
    }

    if (currentState != _lastBuzzerState) {
        uint32_t duration = now - _buzzerLastTransition;
        _buzzerLastTransition = now;
        _lastBuzzerState = currentState;

        if (currentState) {
            // Buzzer turned ON - record previous OFF duration
            if (_buzzerPatternIndex < PATTERN_HISTORY_SIZE) {
                _buzzerOffDurations[_buzzerPatternIndex] = duration;
            }
            _monitored.buzzerPattern.totalOffTime += duration;
        } else {
            // Buzzer turned OFF - record previous ON duration
            if (_buzzerPatternIndex < PATTERN_HISTORY_SIZE) {
                _buzzerOnDurations[_buzzerPatternIndex] = duration;
                _buzzerPatternIndex++;
                if (_buzzerPatternIndex >= PATTERN_HISTORY_SIZE) {
                    _buzzerPatternIndex = 0;  // Wrap around
                }
            }
            _monitored.buzzerPattern.totalOnTime += duration;
            _monitored.buzzerPattern.cycleCount++;

            // Calculate pattern after a few cycles
            if (_monitored.buzzerPattern.cycleCount >= 2) {
                calculatePattern(_monitored.buzzerPattern, _buzzerOnDurations, _buzzerOffDurations,
                                 min((uint8_t)_monitored.buzzerPattern.cycleCount, (uint8_t)PATTERN_HISTORY_SIZE));
            }
        }
    }
}

void EmulatorStateManager::updateLedPattern(bool currentState) {
    uint32_t now = millis();

    // Check for pattern timeout (reset if idle too long)
    if (now - _ledLastTransition > PATTERN_TIMEOUT_MS) {
        _monitored.ledPattern.isBlinking = false;
        _monitored.ledPattern.cycleCount = 0;
        _ledPatternIndex = 0;
    }

    if (currentState != _lastLedState) {
        uint32_t duration = now - _ledLastTransition;
        _ledLastTransition = now;
        _lastLedState = currentState;

        if (currentState) {
            // LED turned ON - record previous OFF duration
            if (_ledPatternIndex < PATTERN_HISTORY_SIZE) {
                _ledOffDurations[_ledPatternIndex] = duration;
            }
            _monitored.ledPattern.totalOffTime += duration;
        } else {
            // LED turned OFF - record previous ON duration
            if (_ledPatternIndex < PATTERN_HISTORY_SIZE) {
                _ledOnDurations[_ledPatternIndex] = duration;
                _ledPatternIndex++;
                if (_ledPatternIndex >= PATTERN_HISTORY_SIZE) {
                    _ledPatternIndex = 0;  // Wrap around
                }
            }
            _monitored.ledPattern.totalOnTime += duration;
            _monitored.ledPattern.cycleCount++;

            // Calculate pattern after a few cycles
            if (_monitored.ledPattern.cycleCount >= 2) {
                calculatePattern(_monitored.ledPattern, _ledOnDurations, _ledOffDurations,
                                 min((uint8_t)_monitored.ledPattern.cycleCount, (uint8_t)PATTERN_HISTORY_SIZE));
            }
        }
    }
}

void EmulatorStateManager::calculatePattern(SignalPattern& pattern, uint32_t* onDurations, uint32_t* offDurations, uint8_t count) {
    if (count < 2) {
        pattern.isBlinking = false;
        return;
    }

    // Calculate averages
    uint32_t totalOn = 0;
    uint32_t totalOff = 0;
    for (uint8_t i = 0; i < count; i++) {
        totalOn += onDurations[i];
        totalOff += offDurations[i];
    }

    pattern.onTimeMs = totalOn / count;
    pattern.offTimeMs = totalOff / count;
    pattern.periodMs = pattern.onTimeMs + pattern.offTimeMs;

    // Calculate frequency and duty cycle
    if (pattern.periodMs > 0) {
        pattern.frequencyHz = 1000.0f / pattern.periodMs;
        pattern.dutyCycle = (pattern.onTimeMs * 100) / pattern.periodMs;
        pattern.isBlinking = (pattern.periodMs >= MIN_BLINK_PERIOD_MS * 2);
    } else {
        pattern.frequencyHz = 0;
        pattern.dutyCycle = pattern.onTimeMs > 0 ? 100 : 0;
        pattern.isBlinking = false;
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

MotorDirection EmulatorStateManager::calculateMotorDirection(bool pos, bool neg) {
    if (pos && !neg) {
        return MotorDirection::OPENING;
    } else if (!pos && neg) {
        return MotorDirection::CLOSING;
    } else if (pos && neg) {
        return MotorDirection::BRAKE;
    } else {
        return MotorDirection::STOPPED;
    }
}

const char* EmulatorStateManager::doorStateToString(DoorState state) {
    switch (state) {
        case DoorState::OPEN: return "OPEN";
        case DoorState::CLOSED: return "CLOSED";
        case DoorState::OPENING: return "OPENING";
        case DoorState::CLOSING: return "CLOSING";
        case DoorState::STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}

const char* EmulatorStateManager::motorDirectionToString(MotorDirection dir) {
    switch (dir) {
        case MotorDirection::OPENING: return "OPENING";
        case MotorDirection::CLOSING: return "CLOSING";
        case MotorDirection::BRAKE: return "BRAKE";
        case MotorDirection::STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

void EmulatorStateManager::toJson(JsonObject& obj) const {
    JsonObject monitored = obj["monitored"].to<JsonObject>();
    monitoredToJson(monitored);

    JsonObject emulated = obj["emulated"].to<JsonObject>();
    emulatedToJson(emulated);

    JsonObject config = obj["config"].to<JsonObject>();
    config["door_travel_time_ms"] = _config.doorTravelTimeMs;
    config["auto_simulate_door"] = _config.autoSimulateDoor;
    config["pulses_per_gallon"] = _config.pulsesPerGallon;
    config["flow_rate_gpm"] = _config.flowRateGPM;
    config["auto_generate_pulses"] = _config.autoGeneratePulses;
    config["inject_door_fault"] = _config.injectDoorFault;
    config["simulate_frozen_line"] = _config.simulateFrozenLine;
    config["simulate_door_stuck"] = _config.simulateDoorStuck;
    config["short_press_ms"] = _config.shortPressMs;
    config["long_press_ms"] = _config.longPressMs;

    // Manual override configuration
    JsonObject override = obj["override"].to<JsonObject>();
    override["enabled"] = _config.manualOverrideEnabled;
    override["hall_open"] = _config.overrideHallOpen;
    override["hall_close"] = _config.overrideHallClose;
    override["door_fault"] = _config.overrideDoorFault;
    override["manual_switch"] = _config.overrideManualSwitch;
    override["water_pulse_1"] = _config.overrideWaterPulse1;
    override["water_pulse_2"] = _config.overrideWaterPulse2;
}

void EmulatorStateManager::monitoredToJson(JsonObject& obj) const {
    obj["pump_active"] = _monitored.pumpActive;
    obj["light_active"] = _monitored.lightActive;
    obj["light_brightness"] = _monitored.lightBrightness;
    obj["motor_pos_active"] = _monitored.doorMotorPosActive;
    obj["motor_neg_active"] = _monitored.doorMotorNegActive;
    obj["motor_direction"] = motorDirectionToString(_monitored.motorDirection);
    obj["buzzer_active"] = _monitored.buzzerActive;
    obj["buzzer_duration_ms"] = _monitored.buzzerOnDuration;
    obj["wifi_led_active"] = _monitored.wifiLedActive;

    // Buzzer pattern
    JsonObject buzzerPattern = obj["buzzer_pattern"].to<JsonObject>();
    buzzerPattern["is_blinking"] = _monitored.buzzerPattern.isBlinking;
    buzzerPattern["frequency_hz"] = _monitored.buzzerPattern.frequencyHz;
    buzzerPattern["period_ms"] = _monitored.buzzerPattern.periodMs;
    buzzerPattern["on_time_ms"] = _monitored.buzzerPattern.onTimeMs;
    buzzerPattern["off_time_ms"] = _monitored.buzzerPattern.offTimeMs;
    buzzerPattern["duty_cycle"] = _monitored.buzzerPattern.dutyCycle;
    buzzerPattern["cycle_count"] = _monitored.buzzerPattern.cycleCount;

    // LED pattern
    JsonObject ledPattern = obj["led_pattern"].to<JsonObject>();
    ledPattern["is_blinking"] = _monitored.ledPattern.isBlinking;
    ledPattern["frequency_hz"] = _monitored.ledPattern.frequencyHz;
    ledPattern["period_ms"] = _monitored.ledPattern.periodMs;
    ledPattern["on_time_ms"] = _monitored.ledPattern.onTimeMs;
    ledPattern["off_time_ms"] = _monitored.ledPattern.offTimeMs;
    ledPattern["duty_cycle"] = _monitored.ledPattern.dutyCycle;
    ledPattern["cycle_count"] = _monitored.ledPattern.cycleCount;
}

static const char* switchPressTypeToString(SwitchPressType type) {
    switch (type) {
        case SwitchPressType::SHORT: return "SHORT";
        case SwitchPressType::LONG: return "LONG";
        default: return "NONE";
    }
}

void EmulatorStateManager::emulatedToJson(JsonObject& obj) const {
    obj["water_flow_enabled"] = _emulated.waterFlowEnabled;
    obj["flow_rate_gpm"] = _emulated.flowRateGPM;
    obj["channel1_pulses"] = _emulated.channel1PulseCount;
    obj["channel2_pulses"] = _emulated.channel2PulseCount;
    obj["door_state"] = doorStateToString(_emulated.doorState);
    obj["door_position"] = _emulated.doorPosition;
    obj["hall_open_active"] = _emulated.hallOpenActive;
    obj["hall_close_active"] = _emulated.hallCloseActive;
    obj["door_fault_active"] = _emulated.doorFaultActive;

    // Enhanced manual switch state
    JsonObject manualSwitch = obj["manual_switch"].to<JsonObject>();
    manualSwitch["is_pressed"] = _emulated.manualSwitch.isPressed;
    manualSwitch["press_type"] = switchPressTypeToString(_emulated.manualSwitch.lastPressType);
    manualSwitch["press_duration_ms"] = _emulated.manualSwitch.pressDuration;
    manualSwitch["short_threshold_ms"] = _emulated.manualSwitch.shortPressThresholdMs;
    manualSwitch["long_threshold_ms"] = _emulated.manualSwitch.longPressThresholdMs;

    // Keep backward compatibility
    obj["manual_switch_pressed"] = _emulated.manualSwitch.isPressed;
}
