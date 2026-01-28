#ifndef __PUMP_CONTROLLER_H__
#define __PUMP_CONTROLLER_H__

#include <Arduino.h>
#include "SensorManager.h"
#include <stdint.h>

// Pump states
enum class PumpState {
    PUMP_OFF = 0,
    PUMP_ON,
    PUMP_AUTO,
    PUMP_ERROR
};

// Pump status structure
struct PumpStatus {
    PumpState state = PumpState::PUMP_AUTO;
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
    bool pump_off_flow_detected; // Flow detected when pump should be off
};

class PumpController {
private:
    uint8_t pumpPin;
    PumpStatus status;
    
    // Sensor references
    SensorManager* primarySensor_;    // Primary sensor for temperature
    SensorManager* flowSensor_;       // Sensor for flow detection (can be same or different)
    
    // Timing variables
    unsigned long lastUpdateTime;
    unsigned long cycleStartTime;
    bool cyclingActive;  // Flag to track if cycling has started (handles time=0 case)
    bool currentlyInOnPhase;
    unsigned long offPhaseStartTime;
    
    // Error detection
    unsigned long lastFlowCheckTime;
    unsigned long errorStartTime; // When error state started
    bool waitingForRetry; // Flag to indicate we're waiting to retry after error
    
    // Pump off flow monitoring
    bool pump_off_flow_monitoring_enabled; // Enable pump OFF flow monitoring
    int pump_off_flow_grace_period_seconds; // Grace period after pump turns off before monitoring starts
    unsigned long pump_turned_off_time; // Track when pump last turned off
    bool pump_has_been_off; // Flag to track if pump was ever off (handles time=0 case)
    bool pump_off_flow_detected; // Flag for flow detected when pump should be off
    
    // Private methods
    void setPumpState(bool isOn);
    void updateStatistics();
    void handleAutoMode(unsigned long currentTime);
    bool checkFlowError() const;
    void checkPumpOffFlow(unsigned long currentTime); // Check for flow when pump is OFF
    
public:
    PumpController() = default;
    
    // Initialization
    void begin(SensorManager* primarySensor, SensorManager* flowSensor, uint8_t pin);
    
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
    bool getPumpOffFlowDetected() const { return status.pump_off_flow_detected; }
    
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
    void clearPumpOffFlowDetected(); // Clear pump off flow detection flag
};

#endif // __PUMP_CONTROLLER_H__