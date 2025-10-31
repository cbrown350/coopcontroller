#ifndef __TEMP_SENSOR_H__
#define __TEMP_SENSOR_H__

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <atomic>

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
    std::atomic<unsigned long> pulse_count;
    float flow_rate;  // Calculated flow rate for water meter
    std::atomic<unsigned long> last_pulse_time;
    
    explicit SensorData(SensorType t = SENSOR_TYPE_NONE,
               float temp = 0.0f,
               bool connected = false,
               unsigned long lastRead = 0,
               unsigned long pulses = 0,
               float flow = 0.0f,
               unsigned long lastPulse = 0)
      : type(t)
      , temperature_f(temp)
      , is_connected(connected)
      , last_reading_time(lastRead)
      , pulse_count(pulses)
      , flow_rate(flow)
      , last_pulse_time(lastPulse)
    {}

    // Explicit copy constructor - copy atomics using load/store
    SensorData(const SensorData& other)
        : type(other.type),
          temperature_f(other.temperature_f),
          is_connected(other.is_connected),
          last_reading_time(other.last_reading_time),
          pulse_count(other.pulse_count.load()),
          flow_rate(other.flow_rate),
          last_pulse_time(other.last_pulse_time.load())
    {
        // all initialization done in initializer list
    }

    // Explicit move constructor
    SensorData(SensorData&& other) noexcept
        : type(other.type),
          temperature_f(other.temperature_f),
          is_connected(other.is_connected),
          last_reading_time(other.last_reading_time),
          pulse_count(other.pulse_count.load()),
          flow_rate(other.flow_rate),
          last_pulse_time(other.last_pulse_time.load())
    {
        // move is same as copy for these trivials/atomics
    }

    SensorData& operator=(const SensorData& other) {
        if (this == &other) return *this;
        type = other.type;
        temperature_f = other.temperature_f;
        is_connected = other.is_connected;
        pulse_count.store(other.pulse_count.load());
        last_pulse_time.store(other.last_pulse_time.load());
        flow_rate = other.flow_rate;
        last_reading_time = other.last_reading_time;
        return *this;
    }
};

class TempSensor {
private:
    // OneWire and DallasTemperature instances
    OneWire* oneWire1;
    OneWire* oneWire2;
    DallasTemperature* dallasTemp1;
    DallasTemperature* dallasTemp2;
    
    // Sensor data for each pin
    SensorData  sensor1;
    SensorData  sensor2;
    
    // Water meter calculation variables
    static const unsigned long FLOW_CALCULATION_INTERVAL = 60000; // 1 minute
    float pulseToGallons;  // Conversion factor for pulses to gallons
    
    // Private methods
    void sensor1PulseISR();
    void sensor2PulseISR();
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
    bool isTemperatureBelowThreshold() const;
    void resetPulseCount(int sensor);
    
    // Status methods
    String getSensorStatusString(const SensorData& sensor) const;
    bool hasWaterFlowError(int sensor) const;
    
    // Water meter specific
    bool hasActiveWaterMeter() const;
    unsigned long getMostRecentPulseTime() const;
};

#endif // __TEMP_SENSOR_H__