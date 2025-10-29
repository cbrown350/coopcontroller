#ifndef __PUMP_CONTROLLER_H__
#define __PUMP_CONTROLLER_H__

#include <Arduino.h>

// Pump states
enum PumpState {
    PUMP_OFF = 0,
    PUMP_ON,
    PUMP_AUTO,
    PUMP_ERROR
};

// Pump status structure
struct PumpStatus {
    PumpState state;
    bool is_active;
    unsigned long last_switch_time;
    unsigned long current_cycle_start;
    int current_cycle_duration;
    float temperature_f;
    bool temperature_below_threshold;
    bool flow_error;
    unsigned long total_on_time;
    unsigned long total_off_time;
    unsigned long total_cycles;
};

class PumpController {
private:
    int pumpPin;
    PumpStatus status;
    
    // Timing variables
    unsigned long lastUpdateTime;
    unsigned long cycleStartTime;
    bool currentlyInOnPhase;
    unsigned long offPhaseStartTime;
    
    // Error detection
    unsigned long lastFlowCheckTime;
    bool flowErrorDetected;
    
    // Private methods
    void setPumpState(bool isOn);
    void checkFlowError();
    void updateStatistics();
    void handleAutoMode(unsigned long currentTime);
    
public:
    PumpController(int pin = OUT_PUMP_PIN);
    
    // Initialization
    void begin();
    
    // Main update function - call this in loop()
    void update(float temperature_f, bool temperature_below_threshold, bool has_flow_error);
    
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
    
    // Statistics
    unsigned long getTotalOnTime() const { return status.total_on_time; }
    unsigned long getTotalOffTime() const { return status.total_off_time; }
    unsigned long getTotalCycles() const { return status.total_cycles; }
    unsigned long getCurrentCycleTime() const;
    unsigned long getTimeUntilNextSwitch() const;
    
    // Status strings
    String getStateString() const;
    String getStatusJson() const;
    
    // Reset methods
    void resetStatistics();
    void clearFlowError();
};

#endif // __PUMP_CONTROLLER_H__