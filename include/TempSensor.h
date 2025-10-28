#ifndef __TEMP_SENSOR_H__
#define __TEMP_SENSOR_H__

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Sensor types for each pin
enum SensorType {
    SENSOR_TYPE_NONE = 0,
    SENSOR_TYPE_DALLAS_TEMP,
    SENSOR_TYPE_WATER_METER
};

// Sensor data structure
struct SensorData {
    SensorType type;
    float temperature_f;
    bool is_connected;
    unsigned long last_reading_time;
    unsigned long pulse_count;
    float flow_rate;  // Calculated flow rate for water meter
    unsigned long last_pulse_time;
};

class TempSensor {
private:
    // OneWire and DallasTemperature instances
    OneWire* oneWire1;
    OneWire* oneWire2;
    DallasTemperature* dallasTemp1;
    DallasTemperature* dallasTemp2;
    
    // Sensor data for each pin
    SensorData sensor1;
    SensorData sensor2;
    
    // Water meter calculation variables
    static const unsigned long FLOW_CALCULATION_INTERVAL = 60000; // 1 minute
    float pulseToGallons;  // Conversion factor for pulses to gallons
    
    // Private methods
    void detectSensorType(int pin, SensorData& sensor);
    void readDallasTemperature(DallasTemperature* dallas, SensorData& sensor);
    void handleWaterMeterPulse(SensorData& sensor);
    void calculateFlowRate(SensorData& sensor);
    
public:
    TempSensor();
    ~TempSensor();
    
    // Initialization
    void begin();
    
    // Main update function - call this in loop()
    void update();
    
    // Get sensor data
    SensorData getSensor1Data() const { return sensor1; }
    SensorData getSensor2Data() const { return sensor2; }
    
    // Get specific values
    float getTemperature1F() const { return sensor1.temperature_f; }
    float getTemperature2F() const { return sensor2.temperature_f; }
    bool isSensor1Connected() const { return sensor1.is_connected; }
    bool isSensor2Connected() const { return sensor2.is_connected; }
    SensorType getSensor1Type() const { return sensor1.type; }
    SensorType getSensor2Type() const { return sensor2.type; }
    
    // Water meter specific
    float getFlowRate1() const { return sensor1.flow_rate; }
    float getFlowRate2() const { return sensor2.flow_rate; }
    unsigned long getPulseCount1() const { return sensor1.pulse_count; }
    unsigned long getPulseCount2() const { return sensor2.pulse_count; }
    
    // Utility methods
    float celsiusToFahrenheit(float celsius) const;
    bool isTemperatureBelowThreshold(float threshold_f) const;
    void resetPulseCount(int sensor);
    
    // Status methods
    String getSensorStatusString(const SensorData& sensor) const;
    bool hasWaterFlowError(int sensor) const;
};

#endif // __TEMP_SENSOR_H__