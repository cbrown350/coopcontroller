#ifndef __PUMP_CONTROLLER_H__
#define __PUMP_CONTROLLER_H__

#include <Arduino.h>
#include "SensorManager.h"

// Pump states
enum PumpState {
    PUMP_OFF = 0,
    PUMP_ON,
    PUMP_AUTO,
    PUMP_ERROR
};

// Pump status structure
struct PumpStatus {
    PumpState state = PUMP_AUTO;
    bool is_active;
    unsigned long last_switch_time;
    unsigned long current_cycle_start;
    int current_cycle_duration;
    float temperature_f;
    bool temperature_below_threshold;
    bool flow_error;
    unsigned long time_until_retry; // Time remaining until next retry attempt
    unsigned long total_on_time;
    unsigned long total_off_time;
    unsigned long total_cycles;
};

class PumpController {
private:
    int pumpPin;
    PumpStatus status;
    
    // Sensor references
    SensorManager* primarySensor_;    // Primary sensor for temperature
    SensorManager* flowSensor_;       // Sensor for flow detection (can be same or different)
    
    // Timing variables
    unsigned long lastUpdateTime;
    unsigned long cycleStartTime;
    bool currentlyInOnPhase;
    unsigned long offPhaseStartTime;
    
    // Error detection
    unsigned long lastFlowCheckTime;
    unsigned long errorStartTime; // When error state started
    bool waitingForRetry; // Flag to indicate we're waiting to retry after error
    
    // Private methods
    void setPumpState(bool isOn);
    void updateStatistics();
    void handleAutoMode(unsigned long currentTime);
    bool checkFlowError();
    
public:
    // Constructor with single sensor (backward compatibility)
    PumpController(SensorManager* sensor, int pin = OUT_PUMP_PIN);
    
    // Constructor with separate sensors for temperature and flow
    PumpController(SensorManager* primarySensor, SensorManager* flowSensor, int pin = OUT_PUMP_PIN);
    
    // Initialization
    void begin();
    
    // Main update function - call this in loop()
    void update();
    
    // Control methods
    void turnOn();
    void turnOff();
    void setAutoMode(bool enabled);
    void forceCycle();
    
    // Status methods
    PumpStatus getStatus() const { return status; }
    bool isPumpOn() const { return status.is_active; }
    PumpState getState() const { return status.state; }
    float getCurrentTemperature() const { return status.temperature_f; }
    bool hasFlowError() const { return status.flow_error; }
    
    unsigned long getCurrentRunStartTime() const;
    
    // Statistics
    unsigned long getTotalOnTime() const { return status.total_on_time; }
    unsigned long getTotalOffTime() const { return status.total_off_time; }
    unsigned long getTotalCycles() const { return status.total_cycles; }
    unsigned long getCurrentCycleTime() const;
    unsigned long getTimeUntilNextSwitch() const;
    unsigned long getTimeUntilRetry() const { return status.time_until_retry; }
    
    // Status strings
    String getStateString() const;
    String getStatusJson() const;
    
    // Reset methods
    void resetStatistics();
    void clearFlowError();
};

#endif // __PUMP_CONTROLLER_H__