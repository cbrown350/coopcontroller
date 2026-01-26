#ifndef DOORCONTROLLER_H
#define DOORCONTROLLER_H

#include "BuzzerController.h"
#include "SunriseSunset.h"

#include <Arduino.h>
#include <ArduinoJson.h>

// Door state enumeration
enum class DoorState {
    IDLE,
    OPENING,
    OPEN,
    CLOSING,
    CLOSED,
    FAULT
};

// Door position enumeration
enum class DoorPosition {
    UNKNOWN,
    OPEN,
    CLOSED,
    PARTIAL
};

class DoorController { // NOSONAR - complexity ok
private:
    BuzzerController* buzzerController;
    SunriseSunsetCalculator* sunriseSunset;

    // State variables
    DoorState currentState;
    DoorPosition currentPosition;
    unsigned long stateStartTime;
    bool autoMode;
    bool testMode;
    
    // Last movement direction for manual switch reversal
    DoorState lastMovementDirection;
    
    // Manual switch debouncing
    unsigned long lastSwitchCheck;
    bool lastSwitchState;
    static const int switchDebounceMs = 50;
    
    // Configuration
    int openTimeoutSeconds;
    int closeTimeoutSeconds;
    int sunriseOffsetMinutes;
    int sunsetOffsetMinutes;
    
    // Statistics
    unsigned long totalOpenTime;
    unsigned long totalCloseTime;
    unsigned long totalCycles;
    
    // Static instance for ISR access
    static DoorController* instance;
    
    // Internal methods
    void setState(DoorState newState);
    void setMotorOutputs(bool openPositive, bool openNegative);
    void updatePosition();
    void checkManualSwitch();
    void checkTimeout();
    void checkSchedule();
    bool shouldOpenBySchedule() const;
    bool shouldCloseBySchedule() const;
    time_t getTodaySunrise() const;
    time_t getTodaySunset() const;
    
    // ISR-safe methods called from interrupt context
    void handleHallOpenISR();
    void handleHallClosedISR();

public:
    DoorController();
    
    // Initialization
    void begin(BuzzerController* buzzerController, SunriseSunsetCalculator* sunriseSunset);
    
    // Main update loop - call frequently (100ms recommended)
    void update();
    
    // Manual control
    void open();
    void close();
    void stop();
    
    // Automatic mode control
    void setAutoMode(bool enabled);
    bool isAutoMode() const;
    
    // Test mode for UI testing without hardware
    void setTestMode(bool enabled);
    bool isTestMode() const;
    
    // State getters
    DoorState getState() const;
    DoorPosition getPosition() const;
    String getStateString() const;
    String getPositionString() const;
    
    // Progress calculation for UI
    int getProgressPercentage() const;
    
    // Configuration getters/setters
    int getOpenTimeoutSeconds() const;
    void setOpenTimeoutSeconds(int seconds);
    int getCloseTimeoutSeconds() const;
    void setCloseTimeoutSeconds(int seconds);
    int getSunriseOffsetMinutes() const;
    void setSunriseOffsetMinutes(int minutes);
    int getSunsetOffsetMinutes() const;
    void setSunsetOffsetMinutes(int minutes);
    
    // Statistics
    unsigned long getTotalOpenTime() const;
    unsigned long getTotalCloseTime() const;
    unsigned long getTotalCycles() const;
    void resetStatistics();
    
    // Status for API
    void toJson(JsonObject& json) const;
    String getNextScheduledAction() const;
    
    // Fault handling
    bool hasFault() const;
    void clearFault();
    bool isHardwareFault() const;
    
    // Position memory
    void notifyPosition() const;
    void restorePosition();
};

#endif // DOORCONTROLLER_H