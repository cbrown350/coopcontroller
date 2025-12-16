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

DoorController::DoorController() {
    instance = this;
    currentState = DoorState::IDLE;
    currentPosition = DoorPosition::UNKNOWN;
    stateStartTime = 0;
    autoMode = false;
    testMode = false;
    lastMovementDirection = DoorState::IDLE;
    lastSwitchCheck = 0;
    lastSwitchState = HIGH;
    
    // Default configuration
    openTimeoutSeconds = 30;
    closeTimeoutSeconds = 30;
    sunriseOffsetMinutes = 0;
    sunsetOffsetMinutes = 0;
    
    // Statistics
    totalOpenTime = 0;
    totalCloseTime = 0;
    totalCycles = 0;
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
    
    // Check automatic schedule
    if (autoMode && (currentState == DoorState::IDLE || currentState == DoorState::OPEN || currentState == DoorState::CLOSED)) {
        checkSchedule();
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
    if (oldState == DoorState::OPENING) {
        totalOpenTime += (currentTime - stateStartTime);
    } else if (oldState == DoorState::CLOSING) {
        totalCloseTime += (currentTime - stateStartTime);
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
    if (lastSwitchState == HIGH && currentSwitchState == LOW && !testMode) {
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
                open();
                logger.logInfo("  Action: Cleared fault, opening door");
            } else {
                close();
                logger.logInfo("  Action: Cleared fault, closing door");
            }
            lastMovementDirection = DoorState::IDLE;
        } else if (currentState == DoorState::IDLE || currentState == DoorState::OPEN || currentState == DoorState::CLOSED) {
            // In stopped states (IDLE/OPEN/CLOSED), handle button press
            if (lastMovementDirection == DoorState::OPENING) {
                // Was opening, now stopped - reverse to close
                close();
                lastMovementDirection = DoorState::IDLE;
                logger.logInfo("  Action: Reversing to CLOSE");
            } else if (lastMovementDirection == DoorState::CLOSING) {
                // Was closing, now stopped - reverse to open
                open();
                lastMovementDirection = DoorState::IDLE;
                logger.logInfo("  Action: Reversing to OPEN");
            } else {
                // Normal toggle based on position
                if (currentPosition == DoorPosition::CLOSED || currentPosition == DoorPosition::UNKNOWN) { // NOSONAR - complexity ok
                    open();
                    logger.logInfo("  Action: Opening door (normal toggle)");
                } else {
                    close();
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

void DoorController::checkSchedule() {
    if (shouldOpenBySchedule() && currentPosition != DoorPosition::OPEN) {
        logger.logInfo("Schedule: Opening door");
        open();
    } else if (shouldCloseBySchedule() && currentPosition != DoorPosition::CLOSED) {
        logger.logInfo("Schedule: Closing door");
        close();
    }
}

// Manual control methods
void DoorController::open() {
    if (currentState == DoorState::IDLE || currentState == DoorState::CLOSED) {
        setState(DoorState::OPENING);
    } else {
        logger.logfWarning("Cannot open door - current state: %s", getStateString().c_str());
    }
}

void DoorController::close() {
    if (currentState == DoorState::IDLE || currentState == DoorState::OPEN) {
        setState(DoorState::CLOSING);
    } else {
        logger.logfWarning("Cannot close door - current state: %s", getStateString().c_str());
    }
}

void DoorController::stop() {
    if (currentState == DoorState::OPENING || currentState == DoorState::CLOSING) {
        setState(DoorState::IDLE);
    }
}

// Mode control
void DoorController::setAutoMode(bool enabled) {
    autoMode = enabled;
    logger.logfInfo("Door auto mode: %s", enabled ? "ENABLED" : "DISABLED");
}

bool DoorController::isAutoMode() const {
    return autoMode;
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

String DoorController::getStateString() const {
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

String DoorController::getPositionString() const {
    switch (currentPosition) {
        case DoorPosition::OPEN: return "OPEN";
        case DoorPosition::CLOSED: return "CLOSED";
        case DoorPosition::PARTIAL: return "PARTIAL";
        case DoorPosition::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

int DoorController::getProgressPercentage() const {
    if (currentPosition == DoorPosition::OPEN) return 100;
    if (currentPosition == DoorPosition::CLOSED) return 0;
    if (currentPosition == DoorPosition::UNKNOWN) return 50;
    
    // Estimate progress during movement
    if (currentState == DoorState::OPENING) {
        unsigned long elapsed = millis() - stateStartTime;
        int progress = (elapsed * 100) / (openTimeoutSeconds * 1000);
        return min(progress, 95);
    } else if (currentState == DoorState::CLOSING) {
        unsigned long elapsed = millis() - stateStartTime;
        int progress = 100 - ((elapsed * 100) / (closeTimeoutSeconds * 1000));
        return max(progress, 5);
    }
    
    return 50;
}

// Configuration
int DoorController::getOpenTimeoutSeconds() const {
    return openTimeoutSeconds;
}

void DoorController::setOpenTimeoutSeconds(int seconds) {
    openTimeoutSeconds = max(5, min(120, seconds));
    logger.logfDebug("Door open timeout: %d seconds", openTimeoutSeconds);
}

int DoorController::getCloseTimeoutSeconds() const {
    return closeTimeoutSeconds;
}

void DoorController::setCloseTimeoutSeconds(int seconds) {
    closeTimeoutSeconds = max(5, min(120, seconds));
    logger.logfDebug("Door close timeout: %d seconds", closeTimeoutSeconds);
}

int DoorController::getSunriseOffsetMinutes() const {
    return sunriseOffsetMinutes;
}

void DoorController::setSunriseOffsetMinutes(int minutes) {
    sunriseOffsetMinutes = max(-60, min(60, minutes));
    logger.logfDebug("Door sunrise offset: %d minutes", sunriseOffsetMinutes);
}

int DoorController::getSunsetOffsetMinutes() const {
    return sunsetOffsetMinutes;
}

void DoorController::setSunsetOffsetMinutes(int minutes) {
    sunsetOffsetMinutes = max(-60, min(60, minutes));
    logger.logfDebug("Door sunset offset: %d minutes", sunsetOffsetMinutes);
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
    json["auto_mode"] = autoMode;
    json["test_mode"] = testMode;
    json["hall_open"] = (digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW);
    json["hall_closed"] = (digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW);
    json["total_open_time"] = totalOpenTime / 1000;
    json["total_close_time"] = totalCloseTime / 1000;
    json["total_cycles"] = totalCycles;
    json["next_scheduled_action"] = getNextScheduledAction();
}

String DoorController::getNextScheduledAction() const {
    if (!autoMode) return "Auto mode disabled";
    if (shouldOpenBySchedule()) return "Scheduled to open";
    if (shouldCloseBySchedule()) return "Scheduled to close";
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

// Schedule helpers using sunrise/sunset calculations
bool DoorController::shouldOpenBySchedule() const {
    time_t now = time(nullptr);
    if (now < 0) return false;

    struct tm timeinfo {};                          // storage for localtime_r
    if (localtime_r(&now, &timeinfo) == nullptr) {  // reentrant, thread-safe
        return false;
    }
    const struct tm* ti = &timeinfo;                // pointer-to-const

    int currentMinutes = ti->tm_hour * 60 + ti->tm_min;
    int openTime = sunriseSunset->getSunriseMinutes() + sunriseOffsetMinutes;

    return (currentMinutes >= openTime && currentPosition != DoorPosition::OPEN);
}

bool DoorController::shouldCloseBySchedule() const {
    time_t now = time(nullptr);
    if (now < 0) return false;

    struct tm timeinfo{};
    if (localtime_r(&now, &timeinfo) == nullptr) return false;
    const struct tm* ti = &timeinfo;

    int currentMinutes = ti->tm_hour * 60 + ti->tm_min;

    int closeTime = sunriseSunset->getSunsetMinutes() + sunsetOffsetMinutes;
    if (settingsManager.getDoorAutoCloseAfterSunsetEnabled()) {
        int autoCloseTime = sunriseSunset->getSunsetMinutes() + settingsManager.getDoorAutoCloseAfterSunsetMinutes();
        closeTime = max(closeTime, autoCloseTime);
    }
    return (currentMinutes >= closeTime && currentPosition != DoorPosition::CLOSED);
}
time_t DoorController::getTodaySunrise() const {
    time_t now = time(nullptr);
    struct tm timeinfo{};
    if (localtime_r(&now, &timeinfo) == nullptr) return (time_t)-1;

    struct tm sunrise = timeinfo;
    sunrise.tm_hour = 6;
    sunrise.tm_min = 0;
    sunrise.tm_sec = 0;
    return mktime(&sunrise);
}

time_t DoorController::getTodaySunset() const {
    time_t now = time(nullptr);
    struct tm timeinfo{};
    if (localtime_r(&now, &timeinfo) == nullptr) return (time_t)-1;

    struct tm sunset = timeinfo;
    sunset.tm_hour = 20;
    sunset.tm_min = 0;
    sunset.tm_sec = 0;
    return mktime(&sunset);
}

// ISR-safe methods - minimal processing in interrupt context
void IRAM_ATTR DoorController::handleHallOpenISR() { // NOSONAR - writes pins
    // Only act if we're currently opening AND the sensor is actually active (LOW)
    // This prevents false triggers from floating pin noise
    if (currentState == DoorState::OPENING && digitalRead(DOOR_A_HALL_SENSOR_OPEN_B_PIN) == LOW) {
        // Stop motor immediately
        digitalWrite(OUT_DOOR_A_OPEN_POS_PIN, LOW);
        digitalWrite(OUT_DOOR_A_OPEN_NEG_PIN, LOW);
        // Note: Full state transition will happen in main loop
    }
}

void IRAM_ATTR DoorController::handleHallClosedISR() { // NOSONAR - writes pins
    // Only act if we're currently closing AND the sensor is actually active (LOW)
    // This prevents false triggers from floating pin noise
    if (currentState == DoorState::CLOSING && digitalRead(DOOR_A_HALL_SENSOR_CLOSED_B_PIN) == LOW) {
        // Stop motor immediately
        digitalWrite(OUT_DOOR_A_OPEN_POS_PIN, LOW);
        digitalWrite(OUT_DOOR_A_OPEN_NEG_PIN, LOW);
        // Note: Full state transition will happen in main loop
    }
}