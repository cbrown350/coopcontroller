#include "DoorController.h"
#include "SunriseSunset.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "BuzzerController.h"
#include <time.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FunctionalInterrupt.h>


// Static instance for ISR access
DoorController* DoorController::instance = nullptr;

// Static constant definition (required for ODR-use in std::min)
const int DoorController::MAX_TIMING_HISTORY;

DoorController::DoorController() {
    instance = this;
    currentState = DoorState::IDLE;
    currentPosition = DoorPosition::UNKNOWN;
    stateStartTime = 0;
    autoOpenEnabled = false;
    autoCloseEnabled = false;
    testMode = false;
    lastMovementDirection = DoorState::IDLE;
    lastSwitchCheck = 0;
    lastSwitchState = HIGH;

    // Default configuration
    openTimeoutSeconds = 30;
    closeTimeoutSeconds = 30;
    autoOpenOffsetMinutes = 0;
    autoCloseOffsetMinutes = 0;
    autoOpenDays.fill(true);
    autoCloseDays.fill(true);
    lockoutEnabled = false;

    // Weather-gated auto-open
    weatherManager = nullptr;
    weatherPostponeUntilMs = 0;
    weatherPostponedOpen = false;

    // Timeout auto-calculation
    openTimingHistory.fill(0);
    closeTimingHistory.fill(0);
    openTimingIndex = 0;
    closeTimingIndex = 0;
    openTimingCount = 0;
    closeTimingCount = 0;
    autoCalcTimeoutEnabled = false;

    // Statistics
    totalOpenTime = 0;
    totalCloseTime = 0;
    totalCycles = 0;

    // Trigger source tracking
    lastTriggerSource_ = TriggerSource::STARTUP;
}

void DoorController::begin(BuzzerController* _buzzerController, SunriseSunsetCalculator* _sunriseSunset) {
    this->buzzerController = _buzzerController;
    this->sunriseSunset = _sunriseSunset;    

    // Initialize motor control pins
    pinMode(OUT_DOOR_A_OPEN_POS_PIN, OUTPUT);
    pinMode(OUT_DOOR_A_OPEN_NEG_PIN, OUTPUT);
    
    // Initialize Hall sensor pins (INPUT_ONLY pins - no pullup available on 36, 39)
    // Pin 36 and 39 are INPUT_ONLY on ESP32, cannot use INPUT_PULLUP
    pinMode(DOOR_A_HALL_SENSOR_OPEN_B_PIN, INPUT);
    pinMode(DOOR_A_HALL_SENSOR_CLOSED_B_PIN, INPUT);
    
    // Initialize fault pin (Pin 34 is also INPUT_ONLY - no pullup)
    pinMode(DOOR_A_FAULT_B_PIN, INPUT);
    
    // Initialize manual switch pin (active LOW input with pullup)
    pinMode(DOOR_MANUAL_SWITCH_B_PIN, INPUT_PULLUP);
    
    // Set motor outputs to safe state (stopped)
    setMotorOutputs(false, false);
    
    // // Attach interrupts for Hall sensors (active LOW, trigger on FALLING edge)
    attachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_OPEN_B_PIN), std::bind(&DoorController::handleHallOpenISR, this), FALLING);
    attachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_CLOSED_B_PIN), std::bind(&DoorController::handleHallClosedISR, this), FALLING);

    // Read initial position from Hall sensors
    updatePosition();
    
    logger.logInfo("Door controller initialized with Hall sensor interrupts");
    logger.logfInfo("Initial door position: %s", getPositionString().c_str());
    
    // Debug: Read and log all pin states
    logger.logDebug("Pin states at startup:");
    logger.logfDebug("  Hall Open (pin %d): %d (active=%s)", DOOR_A_HALL_SENSOR_OPEN_B_PIN, 
            digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN),
            digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW ? "YES" : "no");
    logger.logfDebug("  Hall Closed (pin %d): %d (active=%s)", DOOR_A_HALL_SENSOR_CLOSED_B_PIN,
            digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN),
            digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW ? "YES" : "no");
    logger.logfDebug("  Fault (pin %d): %d (fault=%s)", DOOR_A_FAULT_B_PIN,
            digitalRead(DOOR_A_FAULT_B_PIN),
            digitalRead(DOOR_A_FAULT_B_PIN) == LOW ? "YES" : "no");
    logger.logfDebug("  Manual Switch (pin %d): %d (pressed=%s)", DOOR_MANUAL_SWITCH_B_PIN,
            digitalRead(DOOR_MANUAL_SWITCH_B_PIN),
            digitalRead(DOOR_MANUAL_SWITCH_B_PIN) == LOW ? "YES" : "no");
}

void DoorController::update() {
    static unsigned long lastDebugLog = 0;
    unsigned long currentTime = millis();
    
    // Note: Motor pin maintenance removed - pins should stay in state set by setState()
    // If pins are being reset externally, we need to identify the source, not fight it every loop
    
    // Update position from Hall sensors (only when NOT moving - ISR handles movement)
    if (currentState != DoorState::OPENING && currentState != DoorState::CLOSING) {
        updatePosition();
    }
    
    // Periodic debug logging every 5 seconds when moving
    if ((currentState == DoorState::OPENING || currentState == DoorState::CLOSING) &&
        (currentTime - lastDebugLog >= 5000)) {
        lastDebugLog = currentTime;
        logger.logfDebug("Door moving: state=%s, position=%s, elapsed=%lu ms",
                getStateString().c_str(), getPositionString().c_str(),
                currentTime - stateStartTime);
        logger.logfDebug("  Hall sensors: Open=%d, Closed=%d, Fault=%d",
                digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN),
                digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN),
                digitalRead(DOOR_A_FAULT_B_PIN));
        logger.logfDebug("  Motor pins: POS=%d, NEG=%d",
                digitalRead(OUT_DOOR_A_OPEN_POS_PIN),
                digitalRead(OUT_DOOR_A_OPEN_NEG_PIN));
    }
    
    // Note: Hall sensor position detection removed from main loop
    // ISRs handle immediate motor stop and set a flag
    // We just need to check if ISR stopped the motor and complete the state transition
    // Check for ISR-triggered stops by seeing if motor is off but we're in movement state
    if ((currentState == DoorState::OPENING || currentState == DoorState::CLOSING) && !testMode) {
        // If motor outputs are both LOW, ISR must have stopped it
        if (digitalRead(OUT_DOOR_A_OPEN_POS_PIN) == LOW && digitalRead(OUT_DOOR_A_OPEN_NEG_PIN) == LOW) { // NOSONAR - descriptive
            // Complete the state transition based on which direction we were going
            if (currentState == DoorState::OPENING) {
                setState(DoorState::OPEN);
                logger.logInfo("Hall sensor ISR stopped motor - Door reached OPEN position");
            } else if (currentState == DoorState::CLOSING) {
                setState(DoorState::CLOSED);
                logger.logInfo("Hall sensor ISR stopped motor - Door reached CLOSED position");
            }
        }
    }
    
    // Check manual switch
    checkManualSwitch();
    
    // Check for timeouts in movement states
    checkTimeout();
    
    // Check for hardware faults
    if (isHardwareFault() && currentState != DoorState::FAULT) {
        logger.logfWarning("Hardware fault detected on DRV8833 (pin %d is LOW)", DOOR_A_FAULT_B_PIN);
        setState(DoorState::FAULT);
    }
    
    // Check automatic schedule. Auto-open and auto-close are independent:
    // each is gated by its own enable flag and day-of-week list.
    if (currentState == DoorState::IDLE || currentState == DoorState::OPEN || currentState == DoorState::CLOSED) {
        if (autoOpenEnabled || autoCloseEnabled) {
            if (autoOpenEnabled) checkAutoOpenSchedule();
            if (autoCloseEnabled) checkAutoCloseSchedule();
        }
    }
}

void DoorController::updatePosition() {
    bool hallOpen = (digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW);
    bool hallClosed = (digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW);
    
    if (testMode) {
        // In test mode, position follows state
        if (currentState == DoorState::OPEN) {
            currentPosition = DoorPosition::OPEN;
        } else if (currentState == DoorState::CLOSED) {
            currentPosition = DoorPosition::CLOSED;
        } else if (currentState == DoorState::OPENING || currentState == DoorState::CLOSING) {
            currentPosition = DoorPosition::PARTIAL;
        }
        return;
    }
    
    // Update position based on Hall sensors (active LOW)
    if (hallOpen && !hallClosed) {
        currentPosition = DoorPosition::OPEN;
    } else if (!hallOpen && hallClosed) {
        currentPosition = DoorPosition::CLOSED;
    } else if (!hallOpen && !hallClosed) {
        currentPosition = DoorPosition::PARTIAL;
    } else {
        // Both sensors active - error condition
        logger.logWarning("Both Hall sensors active - wiring error");
        currentPosition = DoorPosition::UNKNOWN;
    }
}

void DoorController::setState(DoorState newState) {
    if (newState == currentState) return;
    
    DoorState oldState = currentState;
    unsigned long currentTime = millis();
    
    // Log state change
    String stateText;
    switch(newState) {
        case DoorState::IDLE: stateText = "IDLE"; break;
        case DoorState::OPENING: stateText = "OPENING"; break;
        case DoorState::OPEN: stateText = "OPEN"; break;
        case DoorState::CLOSING: stateText = "CLOSING"; break;
        case DoorState::CLOSED: stateText = "CLOSED"; break;
        case DoorState::FAULT: stateText = "FAULT"; break;
    }
    logger.logfInfo("Door state: %s -> %s", getStateString().c_str(), stateText.c_str());
    
    // Handle exit from old state
    unsigned long elapsed = currentTime - stateStartTime;
    if (oldState == DoorState::OPENING) {
        totalOpenTime += elapsed;
        // Record successful open timing (not fault/stop transitions)
        if (newState == DoorState::OPEN) {
            openTimingHistory[openTimingIndex] = elapsed;
            openTimingIndex = (openTimingIndex + 1) % MAX_TIMING_HISTORY;
            if (openTimingCount < MAX_TIMING_HISTORY) openTimingCount++;
            if (autoCalcTimeoutEnabled) {
                unsigned int recommended = getRecommendedOpenTimeout();
                if (recommended > 0) {
                    openTimeoutSeconds = recommended;
                    logger.logfInfo("Auto-calc open timeout updated to %u seconds", openTimeoutSeconds);
                }
            }
        }
    } else if (oldState == DoorState::CLOSING) {
        totalCloseTime += elapsed;
        // Record successful close timing (not fault/stop transitions)
        if (newState == DoorState::CLOSED) {
            closeTimingHistory[closeTimingIndex] = elapsed;
            closeTimingIndex = (closeTimingIndex + 1) % MAX_TIMING_HISTORY;
            if (closeTimingCount < MAX_TIMING_HISTORY) closeTimingCount++;
            if (autoCalcTimeoutEnabled) {
                unsigned int recommended = getRecommendedCloseTimeout();
                if (recommended > 0) {
                    closeTimeoutSeconds = recommended;
                    logger.logfInfo("Auto-calc close timeout updated to %u seconds", closeTimeoutSeconds);
                }
            }
        }
    }
    
    // Update state BEFORE handling motor outputs
    currentState = newState;
    stateStartTime = currentTime;
    
    // Handle entry to new state - set motor outputs based on NEW state
    switch (newState) {
        case DoorState::OPENING:
            setMotorOutputs(true, false); // Forward to open - KEEP RUNNING
            totalCycles++;
            logger.logInfo("Motor started: OPENING");
            break;
            
        case DoorState::CLOSING:
            setMotorOutputs(false, true); // Reverse to close - KEEP RUNNING
            logger.logInfo("Motor started: CLOSING");
            break;
            
        case DoorState::OPEN:
            setMotorOutputs(false, false); // Stop motor
            currentPosition = DoorPosition::OPEN;
            notifyPosition();
            buzzerController->clearAlert(AlertType::DOOR_FAULT);
            logger.logInfo("Door is now OPEN");
            break;
            
        case DoorState::CLOSED:
            setMotorOutputs(false, false); // Stop motor
            currentPosition = DoorPosition::CLOSED;
            notifyPosition();
            buzzerController->clearAlert(AlertType::DOOR_FAULT);
            logger.logInfo("Door is now CLOSED");
            break;
            
        case DoorState::FAULT:
            setMotorOutputs(false, false); // Stop motor
            buzzerController->triggerAlert(AlertType::DOOR_FAULT);
            logger.logError("Door FAULT state entered");
            break;
            
        case DoorState::IDLE:
            setMotorOutputs(false, false); // Stop motor
            buzzerController->clearAlert(AlertType::DOOR_FAULT);
            logger.logInfo("Door is now IDLE");
            break;
    }
}

void DoorController::setMotorOutputs(bool openPositive, bool openNegative) { // NOSONAR - not const, writes to pins
    if (testMode) {
        logger.logfDebug("Test: Motor %s", openPositive ? "OPEN" : openNegative ? "CLOSE" : "STOP"); // NOSONAR - clearer inline
    } else {
        logger.logfDebug("Motor pins: OPEN_POS=%d, OPEN_NEG=%d", openPositive, openNegative);
    }
    
    digitalWrite(OUT_DOOR_A_OPEN_POS_PIN, openPositive ? HIGH : LOW);
    digitalWrite(OUT_DOOR_A_OPEN_NEG_PIN, openNegative ? HIGH : LOW);
}

void DoorController::checkManualSwitch() { // NOSONAR - complexity ok
    unsigned long currentTime = millis();
    bool currentSwitchState = digitalRead(DOOR_MANUAL_SWITCH_B_PIN);
    
    // Debug: Log switch state changes
    static bool lastLoggedState = HIGH;
    if (currentSwitchState != lastLoggedState) { // NOSONAR - clearer declared above
        logger.logfDebug("Manual switch state changed: %d -> %d", lastLoggedState, currentSwitchState);
        lastLoggedState = currentSwitchState;
    }
    
    // Detect button press (HIGH to LOW transition) with debounce
    if (lastSwitchState == HIGH && currentSwitchState == LOW && !testMode && !lockoutEnabled) {
        // Check debounce timing
        if (currentTime - lastSwitchCheck < switchDebounceMs) {
            logger.logfDebug("Manual switch press ignored - debounce (too soon: %lu ms)", currentTime - lastSwitchCheck);
            return;
        }
        
        lastSwitchCheck = currentTime;
        
        logger.logInfo("Manual switch PRESSED detected");
        logger.logfInfo("  Current state: %s, position: %s, lastMovement: %s",
               getStateString().c_str(), getPositionString().c_str(),
               lastMovementDirection == DoorState::OPENING ? "OPENING" :
               lastMovementDirection == DoorState::CLOSING ? "CLOSING" : "NONE"); // NOSONAR - clearer inline
        
        if (currentState == DoorState::OPENING || currentState == DoorState::CLOSING) {
            // Stop and remember direction
            lastMovementDirection = currentState;
            setState(DoorState::IDLE);
            logger.logInfo("  Action: Stopped door");
        } else if (currentState == DoorState::FAULT) {
            // Clear fault and toggle
            clearFault();
            if (currentPosition == DoorPosition::CLOSED || currentPosition == DoorPosition::UNKNOWN) {
                open(TriggerSource::MANUAL_BUTTON);
                logger.logInfo("  Action: Cleared fault, opening door");
            } else {
                close(TriggerSource::MANUAL_BUTTON);
                logger.logInfo("  Action: Cleared fault, closing door");
            }
            lastMovementDirection = DoorState::IDLE;
        } else if (currentState == DoorState::IDLE || currentState == DoorState::OPEN || currentState == DoorState::CLOSED) {
            // In stopped states (IDLE/OPEN/CLOSED), handle button press
            if (lastMovementDirection == DoorState::OPENING) {
                // Was opening, now stopped - reverse to close
                close(TriggerSource::MANUAL_BUTTON);
                lastMovementDirection = DoorState::IDLE;
                logger.logInfo("  Action: Reversing to CLOSE");
            } else if (lastMovementDirection == DoorState::CLOSING) {
                // Was closing, now stopped - reverse to open
                open(TriggerSource::MANUAL_BUTTON);
                lastMovementDirection = DoorState::IDLE;
                logger.logInfo("  Action: Reversing to OPEN");
            } else {
                // Normal toggle based on position
                if (currentPosition == DoorPosition::CLOSED || currentPosition == DoorPosition::UNKNOWN) { // NOSONAR - complexity ok
                    open(TriggerSource::MANUAL_BUTTON);
                    logger.logInfo("  Action: Opening door (normal toggle)");
                } else {
                    close(TriggerSource::MANUAL_BUTTON);
                    logger.logInfo("  Action: Closing door (normal toggle)");
                }
            }
        }
    }
    
    lastSwitchState = currentSwitchState;
}

void DoorController::checkTimeout() {
    unsigned long elapsed = millis() - stateStartTime;
    
    if (currentState == DoorState::OPENING && elapsed > (openTimeoutSeconds * 1000)) {
        logger.logWarning("Door open timeout - possible obstruction");
        setState(DoorState::FAULT);
    } else if (currentState == DoorState::CLOSING && elapsed > (closeTimeoutSeconds * 1000)) {
        logger.logWarning("Door close timeout - possible obstruction");
        setState(DoorState::FAULT);
    }
}

void DoorController::checkAutoOpenSchedule() {
    if (lockoutEnabled) return;
    int dayIdx = getTodayDayOfWeek();
    if (dayIdx < 0 || !autoOpenDays[dayIdx]) return;
    if (shouldOpenBySchedule() && currentPosition != DoorPosition::OPEN) {
        // Weather gate: when a WeatherManager is attached and its gate is
        // active, hold back opening during inclement weather. The schedule
        // condition (shouldOpenBySchedule) already bounds this to the daytime
        // window, so once the door has opened or the close time arrives, this
        // path stops firing on its own.
        if (weatherManager != nullptr && weatherManager->isWeatherGateActive() &&
            !weatherManager->isWeatherGoodForOpening()) {
            unsigned long now = millis();
            // Recheck about once an hour rather than every loop, to avoid log
            // spam. millis() only moves forward here (no rollover concern for
            // the ~49-day wrap within a single daytime window).
            if (weatherPostponeUntilMs == 0 || now >= weatherPostponeUntilMs) {
                logger.logInfo("Schedule: auto-open postponed due to inclement weather; will recheck in ~1 hour");
                weatherPostponeUntilMs = now + 3600000UL; // 1 hour
            }
            weatherPostponedOpen = true;
            return;
        }

        // Weather is good (or not gating) — clear any postpone state and open.
        weatherPostponedOpen = false;
        weatherPostponeUntilMs = 0;
        logger.logInfo("Schedule: Opening door (sunrise)");
        open(TriggerSource::SUNRISE);
    }
}

void DoorController::checkAutoCloseSchedule() {
    if (lockoutEnabled) return;
    int dayIdx = getTodayDayOfWeek();
    if (dayIdx < 0 || !autoCloseDays[dayIdx]) return;
    if (shouldCloseBySchedule() && currentPosition != DoorPosition::CLOSED) {
        logger.logInfo("Schedule: Closing door (sunset)");
        close(TriggerSource::SUNSET);
    }
}

// Manual control methods
void DoorController::open(TriggerSource trigger) {
    if (lockoutEnabled) {
        logger.logWarning("Door open blocked - lockout is enabled");
        return;
    }
    if (currentState == DoorState::IDLE || currentState == DoorState::CLOSED) {
        lastTriggerSource_ = trigger;
        setState(DoorState::OPENING);
        logger.logfInfo("Door opening (trigger: %s)", triggerSourceToString(trigger).c_str());
    } else {
        logger.logfWarning("Cannot open door - current state: %s", getStateString().c_str());
    }
}

void DoorController::close(TriggerSource trigger) {
    if (lockoutEnabled) {
        logger.logWarning("Door close blocked - lockout is enabled");
        return;
    }
    if (currentState == DoorState::IDLE || currentState == DoorState::OPEN) {
        lastTriggerSource_ = trigger;
        setState(DoorState::CLOSING);
        logger.logfInfo("Door closing (trigger: %s)", triggerSourceToString(trigger).c_str());
    } else {
        logger.logfWarning("Cannot close door - current state: %s", getStateString().c_str());
    }
}

void DoorController::stop(TriggerSource trigger) {
    if (currentState == DoorState::OPENING || currentState == DoorState::CLOSING) {
        lastTriggerSource_ = trigger;
        setState(DoorState::IDLE);
        logger.logfInfo("Door stopped (trigger: %s)", triggerSourceToString(trigger).c_str());
    }
}

// Lockout control
void DoorController::setLockoutEnabled(bool enabled) {
    lockoutEnabled = enabled;
    logger.logfInfo("Door lockout: %s", enabled ? "ENABLED" : "DISABLED");
}

bool DoorController::isLockoutEnabled() const {
    return lockoutEnabled;
}

// Timeout auto-calculation
unsigned int DoorController::getRecommendedOpenTimeout() const {
    if (openTimingCount == 0) return 0;
    unsigned long maxTime = 0;
    int count = std::min(openTimingCount, MAX_TIMING_HISTORY);
    for (int i = 0; i < count; i++) {
        if (openTimingHistory[i] > maxTime) {
            maxTime = openTimingHistory[i];
        }
    }
    return static_cast<unsigned int>((maxTime / 1000) + 1); // Convert ms to seconds + 1s buffer
}

unsigned int DoorController::getRecommendedCloseTimeout() const {
    if (closeTimingCount == 0) return 0;
    unsigned long maxTime = 0;
    int count = std::min(closeTimingCount, MAX_TIMING_HISTORY);
    for (int i = 0; i < count; i++) {
        if (closeTimingHistory[i] > maxTime) {
            maxTime = closeTimingHistory[i];
        }
    }
    return static_cast<unsigned int>((maxTime / 1000) + 1); // Convert ms to seconds + 1s buffer
}

void DoorController::setAutoCalcTimeoutEnabled(bool enabled) {
    autoCalcTimeoutEnabled = enabled;
    logger.logfInfo("Door timeout auto-calculation: %s", enabled ? "ENABLED" : "DISABLED");
}

bool DoorController::isAutoCalcTimeoutEnabled() const {
    return autoCalcTimeoutEnabled;
}

int DoorController::getOpenTimingCount() const {
    return openTimingCount;
}

int DoorController::getCloseTimingCount() const {
    return closeTimingCount;
}

// Weather gate
void DoorController::setWeatherManager(WeatherManager* _weatherManager) {
    weatherManager = _weatherManager;
}

bool DoorController::isWeatherPostponed() const {
    // Only meaningful while the door is not already open.
    return weatherPostponedOpen && currentPosition != DoorPosition::OPEN;
}

// Mode control
void DoorController::setAutoOpenEnabled(bool enabled, TriggerSource trigger) {
    autoOpenEnabled = enabled;
    lastTriggerSource_ = trigger;
    // Reset any weather-postpone state when the mode is toggled so a stale
    // recheck timer doesn't carry over.
    weatherPostponedOpen = false;
    weatherPostponeUntilMs = 0;
    logger.logfInfo("Door auto-open: %s (trigger: %s)", enabled ? "ENABLED" : "DISABLED", triggerSourceToString(trigger).c_str());
}

void DoorController::setAutoCloseEnabled(bool enabled, TriggerSource trigger) {
    autoCloseEnabled = enabled;
    lastTriggerSource_ = trigger;
    logger.logfInfo("Door auto-close: %s (trigger: %s)", enabled ? "ENABLED" : "DISABLED", triggerSourceToString(trigger).c_str());
}

bool DoorController::isAutoMode() const {
    return autoOpenEnabled || autoCloseEnabled;
}

bool DoorController::isAutoOpenEnabled() const {
    return autoOpenEnabled;
}

bool DoorController::isAutoCloseEnabled() const {
    return autoCloseEnabled;
}

void DoorController::setTestMode(bool enabled) {
    testMode = enabled;
    
    if (enabled) {
        // Detach interrupts in test mode
        detachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_OPEN_B_PIN));
        detachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_CLOSED_B_PIN));
        logger.logInfo("Door test mode ENABLED - interrupts detached");
    } else {
        // Reattach interrupts
        attachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_OPEN_B_PIN), std::bind(&DoorController::handleHallOpenISR, this), FALLING);
        attachInterrupt(digitalPinToInterrupt(DOOR_A_HALL_SENSOR_CLOSED_B_PIN), std::bind(&DoorController::handleHallClosedISR, this), FALLING);
        logger.logInfo("Door test mode DISABLED - interrupts reattached");
    }
}

bool DoorController::isTestMode() const {
    return testMode;
}

// State getters
DoorState DoorController::getState() const {
    return currentState;
}

DoorPosition DoorController::getPosition() const {
    return currentPosition;
}

const char* DoorController::getStateCStr() const {
    switch (currentState) {
        case DoorState::IDLE: return "IDLE";
        case DoorState::OPENING: return "OPENING";
        case DoorState::OPEN: return "OPEN";
        case DoorState::CLOSING: return "CLOSING";
        case DoorState::CLOSED: return "CLOSED";
        case DoorState::FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

String DoorController::getStateString() const {
    return String(getStateCStr());
}

const char* DoorController::getPositionCStr() const {
    switch (currentPosition) {
        case DoorPosition::OPEN: return "OPEN";
        case DoorPosition::CLOSED: return "CLOSED";
        case DoorPosition::PARTIAL: return "PARTIAL";
        case DoorPosition::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

String DoorController::getPositionString() const {
    return String(getPositionCStr());
}

int DoorController::getProgressPercentage() const {
    if (currentPosition == DoorPosition::OPEN) return 100;
    if (currentPosition == DoorPosition::CLOSED) return 0;
    if (currentPosition == DoorPosition::UNKNOWN) return 50;
    
    // Estimate progress during movement
    if (currentState == DoorState::OPENING) {
        unsigned long elapsed = millis() - stateStartTime;
        int progress = (elapsed * 100) / (openTimeoutSeconds * 1000);
        return std::min(progress, 95);
    } else if (currentState == DoorState::CLOSING) {
        unsigned long elapsed = millis() - stateStartTime;
        int progress = 100 - ((elapsed * 100) / (closeTimeoutSeconds * 1000));
        return std::max(progress, 5);
    }
    
    return 50;
}

// Configuration
unsigned int DoorController::getOpenTimeoutSeconds() const {
    return openTimeoutSeconds;
}

void DoorController::setOpenTimeoutSeconds(unsigned int seconds) {
    openTimeoutSeconds = std::max(5u, std::min(120u, seconds));
    logger.logfDebug("Door open timeout: %u seconds", openTimeoutSeconds);
}

unsigned int DoorController::getCloseTimeoutSeconds() const {
    return closeTimeoutSeconds;
}

void DoorController::setCloseTimeoutSeconds(unsigned int seconds) {
    closeTimeoutSeconds = std::max(5u, std::min(120u, seconds));
    logger.logfDebug("Door close timeout: %u seconds", closeTimeoutSeconds);
}

int DoorController::getAutoOpenOffsetMinutes() const {
    return autoOpenOffsetMinutes;
}

void DoorController::setAutoOpenOffsetMinutes(int minutes) {
    autoOpenOffsetMinutes = std::max(-120, std::min(120, minutes));
    logger.logfDebug("Door auto-open offset: %d minutes", autoOpenOffsetMinutes);
}

int DoorController::getAutoCloseOffsetMinutes() const {
    return autoCloseOffsetMinutes;
}

void DoorController::setAutoCloseOffsetMinutes(int minutes) {
    autoCloseOffsetMinutes = std::max(-120, std::min(120, minutes));
    logger.logfDebug("Door auto-close offset: %d minutes", autoCloseOffsetMinutes);
}

bool DoorController::getAutoOpenDay(int dayIdx) const {
    if (dayIdx < 0 || dayIdx > 6) return false;
    return autoOpenDays[dayIdx];
}

void DoorController::setAutoOpenDay(int dayIdx, bool enabled) {
    if (dayIdx < 0 || dayIdx > 6) return;
    autoOpenDays[dayIdx] = enabled;
    logger.logfDebug("Door auto-open day %d: %s", dayIdx, enabled ? "enabled" : "disabled");
}

bool DoorController::getAutoCloseDay(int dayIdx) const {
    if (dayIdx < 0 || dayIdx > 6) return false;
    return autoCloseDays[dayIdx];
}

void DoorController::setAutoCloseDay(int dayIdx, bool enabled) {
    if (dayIdx < 0 || dayIdx > 6) return;
    autoCloseDays[dayIdx] = enabled;
    logger.logfDebug("Door auto-close day %d: %s", dayIdx, enabled ? "enabled" : "disabled");
}

// Statistics
unsigned long DoorController::getTotalOpenTime() const {
    return totalOpenTime;
}

unsigned long DoorController::getTotalCloseTime() const {
    return totalCloseTime;
}

unsigned long DoorController::getTotalCycles() const {
    return totalCycles;
}

void DoorController::resetStatistics() {
    totalOpenTime = 0;
    totalCloseTime = 0;
    totalCycles = 0;
    logger.logInfo("Door statistics reset");
}

// JSON serialization
void DoorController::toJson(JsonObject& json) const { // NOSONAR - json is written
    json["state"] = getStateString();
    json["position"] = getPositionString();
    json["progress"] = getProgressPercentage();
    json["auto_mode"] = isAutoMode();          // any auto enabled (backward-compat for status/MQTT)
    json["auto_open_enabled"] = autoOpenEnabled;
    json["auto_close_enabled"] = autoCloseEnabled;
    json["test_mode"] = testMode;
    json["lockout_enabled"] = lockoutEnabled;
    json["hall_open"] = (digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW);
    json["hall_closed"] = (digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW);
    json["total_open_time"] = totalOpenTime / 1000;
    json["total_close_time"] = totalCloseTime / 1000;
    json["total_cycles"] = totalCycles;
    json["next_scheduled_action"] = getNextScheduledAction();
    json["weather_postponed"] = isWeatherPostponed();
    json["auto_calc_timeout_enabled"] = autoCalcTimeoutEnabled;
    json["recommended_open_timeout"] = getRecommendedOpenTimeout();
    json["recommended_close_timeout"] = getRecommendedCloseTimeout();
}

String DoorController::getNextScheduledAction() const {
    if (!autoOpenEnabled && !autoCloseEnabled) return "Auto mode disabled";
    // Report whichever direction is enabled and relevant right now.
    if (autoOpenEnabled && shouldOpenBySchedule()) {
        if (isWeatherPostponed()) return "Open postponed (weather)";
        return "Scheduled to open";
    }
    if (autoCloseEnabled && shouldCloseBySchedule()) return "Scheduled to close";
    return "No scheduled action";
}

// Fault handling
bool DoorController::hasFault() const {
    return currentState == DoorState::FAULT || isHardwareFault();
}

void DoorController::clearFault() {
    if (currentState == DoorState::FAULT) {
        setState(DoorState::IDLE);
        logger.logInfo("Door fault cleared");
    }
}

bool DoorController::isHardwareFault() const {
    if (testMode) return false;
    // DRV8833 fault pin is active LOW
    return (digitalRead(DOOR_A_FAULT_B_PIN) == LOW);
}

void DoorController::notifyPosition() const {
    logger.logfInfo("Door position saved: %s", getPositionString().c_str());
}

void DoorController::restorePosition() {
    updatePosition();
    logger.logfInfo("Door position restored: %s", getPositionString().c_str());
}

// Helper: get current local time in minutes since midnight.
// When configTzTime() is used (ESP32), localtime() returns DST-aware local time
// directly via tm_gmtoff. Falls back to manual offset for desktop/test builds.
int DoorController::getCurrentLocalMinutes() const {
    time_t now = time(nullptr);
    if (now < 0) return -1;

    struct tm timeinfo{};
#if defined(_WIN32)
    if (localtime_s(&timeinfo, &now) != 0) return -1;
#elif defined(ARDUINO) && !defined(ESP32)
    struct tm* result = localtime(&now);
    if (result == nullptr) return -1;
    timeinfo = *result;
#else
    if (localtime_r(&now, &timeinfo) == nullptr) return -1;
#endif

    int minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

#ifdef ESP32
    // On ESP32 with configTzTime(), localtime already returns DST-aware local time.
    // Compare with gmtime to detect if timezone is configured.
    struct tm utcTm{};
    gmtime_r(&now, &utcTm);
    int utcMinutes = utcTm.tm_hour * 60 + utcTm.tm_min;
    if (minutes != utcMinutes || timeinfo.tm_mday != utcTm.tm_mday) {
        return minutes; // Already local time
    }
#endif

    // Fallback: manual offset when system time is pure UTC (or desktop)
    int localMinutes = minutes + (settingsManager.getTimezoneOffsetHours() * 60);
    if (localMinutes < 0) localMinutes += 1440;
    if (localMinutes >= 1440) localMinutes -= 1440;
    return localMinutes;
}

// Day-of-week from local time. tm_wday: 0=Sunday..6=Saturday.
// Falls back to -1 if time is unavailable.
int DoorController::getTodayDayOfWeek() const {
    time_t now = time(nullptr);
    if (now < 0) return -1;

    struct tm timeinfo{};
#if defined(_WIN32)
    if (localtime_s(&timeinfo, &now) != 0) return -1;
#elif defined(ARDUINO) && !defined(ESP32)
    struct tm* result = localtime(&now);
    if (result == nullptr) return -1;
    timeinfo = *result;
#else
    if (localtime_r(&now, &timeinfo) == nullptr) return -1;
#endif

#ifdef ESP32
    // If timezone is configured, localtime_r already returns local-time tm_wday.
    // Detect UTC (unconfigured tz) by comparing with gmtime and fall back gracefully.
    struct tm utcTm{};
    gmtime_r(&now, &utcTm);
    if (timeinfo.tm_wday == utcTm.tm_wday && timeinfo.tm_hour == utcTm.tm_hour) {
        // Treat as UTC; shift by timezone offset hours (best-effort day boundary)
        int localWday = utcTm.tm_wday;
        int localHour = utcTm.tm_hour + settingsManager.getTimezoneOffsetHours();
        if (localHour < 0) localWday = (localWday + 6) % 7;
        else if (localHour >= 24) localWday = (localWday + 1) % 7;
        return localWday;
    }
#endif
    return timeinfo.tm_wday;
}

// Schedule helpers using sunrise/sunset calculations
bool DoorController::shouldOpenBySchedule() const {
    int currentMinutes = getCurrentLocalMinutes();
    if (currentMinutes < 0) return false;

    int openTime = sunriseSunset->getSunriseMinutes() + autoOpenOffsetMinutes;

    // Calculate the close time (must match shouldCloseBySchedule logic).
    // Used as the upper bound of the daytime open window so that auto-open
    // does not re-open the door after auto-close fires, which would create
    // an open/close loop. Only apply this guard when auto-close is enabled.
    int closeTime = sunriseSunset->getSunsetMinutes() + autoCloseOffsetMinutes;

    if (autoCloseEnabled) {
        // Only open during the daytime window: after sunrise but before close time
        return (currentMinutes >= openTime && currentMinutes < closeTime && currentPosition != DoorPosition::OPEN);
    }
    // No auto-close enabled: only the lower bound applies
    return (currentMinutes >= openTime && currentPosition != DoorPosition::OPEN);
}

bool DoorController::shouldCloseBySchedule() const {
    int currentMinutes = getCurrentLocalMinutes();
    if (currentMinutes < 0) return false;

    int closeTime = sunriseSunset->getSunsetMinutes() + autoCloseOffsetMinutes;
    return (currentMinutes >= closeTime && currentPosition != DoorPosition::CLOSED);
}
time_t DoorController::getTodaySunrise() const {
    time_t now = time(nullptr);
    struct tm timeinfo{};

#if defined(_WIN32)
    if (localtime_s(&timeinfo, &now) != 0) return (time_t)-1;
#elif defined(ARDUINO) && !defined(ESP32)
    struct tm* result = localtime(&now);
    if (result == nullptr) return (time_t)-1;
    timeinfo = *result;
#else
    if (localtime_r(&now, &timeinfo) == nullptr) return (time_t)-1;
#endif

    struct tm sunrise = timeinfo;
    sunrise.tm_hour = 6;
    sunrise.tm_min = 0;
    sunrise.tm_sec = 0;
    return mktime(&sunrise);
}

time_t DoorController::getTodaySunset() const {
    time_t now = time(nullptr);
    struct tm timeinfo{};

#if defined(_WIN32)
    if (localtime_s(&timeinfo, &now) != 0) return (time_t)-1;
#elif defined(ARDUINO) && !defined(ESP32)
    struct tm* result = localtime(&now);
    if (result == nullptr) return (time_t)-1;
    timeinfo = *result;
#else
    if (localtime_r(&now, &timeinfo) == nullptr) return (time_t)-1;
#endif

    struct tm sunset = timeinfo;
    sunset.tm_hour = 20;
    sunset.tm_min = 0;
    sunset.tm_sec = 0;
    return mktime(&sunset);
}

// ISR-safe methods - minimal processing in interrupt context
#ifdef ESP32
void IRAM_ATTR DoorController::handleHallOpenISR() { // NOSONAR - writes pins
#else
void DoorController::handleHallOpenISR() { // NOSONAR - writes pins
#endif
    // Only act if we're currently opening AND the sensor is actually active (LOW)
    // This prevents false triggers from floating pin noise
    if (currentState == DoorState::OPENING && digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW) {
        // Stop motor immediately
        digitalWrite(OUT_DOOR_A_OPEN_POS_PIN, LOW);
        digitalWrite(OUT_DOOR_A_OPEN_NEG_PIN, LOW);
        // Note: Full state transition will happen in main loop
    }
}

#ifdef ESP32
void IRAM_ATTR DoorController::handleHallClosedISR() { // NOSONAR - writes pins
#else
void DoorController::handleHallClosedISR() { // NOSONAR - writes pins
#endif
    // Only act if we're currently closing AND the sensor is actually active (LOW)
    // This prevents false triggers from floating pin noise
    if (currentState == DoorState::CLOSING && digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW) {
        // Stop motor immediately
        digitalWrite(OUT_DOOR_A_OPEN_POS_PIN, LOW);
        digitalWrite(OUT_DOOR_A_OPEN_NEG_PIN, LOW);
        // Note: Full state transition will happen in main loop
    }
}