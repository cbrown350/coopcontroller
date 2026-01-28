#ifndef DALLAS_TEMPERATURE_MOCK_H
#define DALLAS_TEMPERATURE_MOCK_H

#include <Arduino.h>

// Mock DallasTemperature library for desktop unit testing
// This mock provides minimal interface for SensorManager tests

// Constant for disconnected sensor
#define DEVICE_DISCONNECTED_C -127.0f

class DallasTemperature {
public:
    // Mock constructor
    DallasTemperature(OneWire* wire) {
        // Empty mock implementation
    }
    
    // Mock methods that SensorManager uses
    void begin(void) {
        // Empty mock implementation
    }
    
    uint8_t getResolution(void) {
        return 9;  // Default 9-bit resolution
    }
    
    bool setResolution(uint8_t resolution) {
        return true;  // Always succeed in mock
    }
    
    void setWaitForConversion(bool wait) {
        // Empty mock implementation
    }
    
    void requestTemperatures(void) {
        // Empty mock implementation
    }
    
    float getTempCByIndex(uint8_t deviceIndex, bool forceConversion = false) {
        // Return disconnected temperature for mock
        return DEVICE_DISCONNECTED_C;
    }
    
    int getDeviceCount(void) {
        return 0;  // No devices in mock
    }
    
    bool isParasite(void) {
        return false;
    }
    
    uint8_t getParasiteAddress(void) {
        return 0;
    }
    
    bool isConversionAvailable(void) {
        return false;
    }
    
    bool getAddress(uint8_t* deviceAddress) {
        return false;
    }
    
    bool setLowAlarmTemp(float alarmTemp) {
        return true;
    }
    
    bool setHighAlarmTemp(float alarmTemp) {
        return true;
    }
    
    float getLowAlarmTemp(void) {
        return 0.0;
    }
    
    float getHighAlarmTemp(void) {
        return 0.0;
    }
    
    bool hasAlarm(void) {
        return false;
    }
};

#endif // DALLAS_TEMPERATURE_MOCK_H
