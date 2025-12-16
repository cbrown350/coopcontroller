#ifndef __SENSOR_MANAGER_H__
#define __SENSOR_MANAGER_H__

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <atomic>
#include <string>
#include <memory>

// Sensor types for each pin
enum class SensorType {
    NONE = 0,
    DALLAS_TEMP,
    WATER_METER
};

// Sensor data structure
struct SensorData {  // NOSONAR - shouldn't warn about destructor since it's defined below
    SensorType type;
    float temperature_f;
    bool is_connected;
    bool was_detected;
    unsigned long last_reading_time;
    std::atomic<unsigned long> pulse_count;
    float flow_rate;  // Calculated flow rate for water meter
    std::atomic<unsigned long> last_pulse_time;
    unsigned long last_flow_calculation_time;  // Track last calculation time per sensor
    
    // Constructor
    explicit SensorData(SensorType t = SensorType::NONE,    // NOSONAR
               float temp = 0.0f,
               bool connected = false,
               bool detected = false,
               unsigned long lastRead = 0,
               unsigned long pulses = 0,
               float flow = 0.0f,
               unsigned long lastPulse = 0,
               unsigned long lastFlowCalc = 0)
      : type(t)
      , temperature_f(temp)
      , is_connected(connected)
      , was_detected(detected)
      , last_reading_time(lastRead)
      , pulse_count(pulses)
      , flow_rate(flow)
      , last_pulse_time(lastPulse)
      , last_flow_calculation_time(lastFlowCalc)
    {}

    // Explicit copy constructor - copy atomics using load/store
    SensorData(const SensorData& other)
        : type(other.type),
          temperature_f(other.temperature_f),
          is_connected(other.is_connected),
          was_detected(other.was_detected),
          last_reading_time(other.last_reading_time),
          pulse_count(other.pulse_count.load()),
          flow_rate(other.flow_rate),
          last_pulse_time(other.last_pulse_time.load()),
          last_flow_calculation_time(other.last_flow_calculation_time)
    {
        // all initialization done in initializer list
    }

    // Explicit move constructor
    SensorData(SensorData&& other) noexcept
        : type(other.type),
          temperature_f(other.temperature_f),
          is_connected(other.is_connected),
          was_detected(other.was_detected),
          last_reading_time(other.last_reading_time),
          pulse_count(other.pulse_count.load()),
          flow_rate(other.flow_rate),
          last_pulse_time(other.last_pulse_time.load()),
          last_flow_calculation_time(other.last_flow_calculation_time)
    {
        // move is same as copy for these trivials/atomics
    }

    ~SensorData() = default;

    SensorData& operator=(const SensorData& other) {
        if (this == &other) return *this;
        type = other.type;
        temperature_f = other.temperature_f;
        is_connected = other.is_connected;
        was_detected = other.was_detected;
        last_flow_calculation_time = other.last_flow_calculation_time;
        pulse_count.store(other.pulse_count.load());
        last_pulse_time.store(other.last_pulse_time.load());
        flow_rate = other.flow_rate;
        last_reading_time = other.last_reading_time;
        return *this;
    }
};

class SensorManager {
private:
    // OneWire and DallasTemperature instances
    uint8_t sensorPin1;
    uint8_t sensorPin2;
    std::unique_ptr<OneWire> oneWire1;
    std::unique_ptr<OneWire> oneWire2;
    std::unique_ptr<DallasTemperature> dallasTemp1;
    std::unique_ptr<DallasTemperature> dallasTemp2;
    
    // Sensor data for each pin
    SensorData  sensor1;
    SensorData  sensor2;
    
    // Water meter calculation variables
    static const unsigned long FLOW_CALCULATION_INTERVAL = 60000; // 1 minute
    float pulsesPerGallon;  // Conversion factor for pulses to gallons
    
    // Private methods
    void sensor1PulseISR();
    void sensor2PulseISR();
    void detectSensorType(uint8_t pin, SensorData& sensor);
    void readDallasTemperature(DallasTemperature* dallas, SensorData& sensor);
    void logWaterMeterPulse(const SensorData& sensor) const;
    void calculateFlowRate(SensorData& sensor) const;
    
public:
    SensorManager();
    ~SensorManager() = default;
    
    // Initialization
    void begin(uint8_t sensorPin1, uint8_t sensorPin2 = 0);
    
    // Main update function - call this in loop()
    void update();
    
    // Get sensor data
    SensorData getSensor1Data() const { return sensor1; }
    SensorData getSensor2Data() const { return sensor2; }
    
    // Get specific values
    float getTemperature1F() const { 
        // Only return temperature if this is a Dallas sensor
        if (sensor1.type != SensorType::DALLAS_TEMP) {
            return NAN;  // Water meters don't have temperature
        }
        
        // Check if sensor is actually detected and connected
        if (!sensor1.was_detected || !sensor1.is_connected) {
            return NAN;  // Sensor not available
        }
        
        return sensor1.temperature_f; 
    }
    float getTemperature2F() const { 
        // Only return temperature if this is a Dallas sensor
        if (sensor2.type != SensorType::DALLAS_TEMP) {
            return NAN;  // Water meters don't have temperature
        }
        
        // Check if sensor is actually detected and connected
        if (!sensor2.was_detected || !sensor2.is_connected) {
            return NAN;  // Sensor not available
        }
        
        return sensor2.temperature_f; 
    }
    bool isSensor1Connected() const { return sensor1.is_connected; }
    bool isSensor2Connected() const { return sensor2.is_connected; }
    SensorType getSensor1Type() const { return sensor1.type; }
    SensorType getSensor2Type() const { return sensor2.type; }
    
    // Detection state methods
    bool isSensor1Detected() const { return sensor1.was_detected; }
    bool isSensor2Detected() const { return sensor2.was_detected; }
    
    // Water meter specific
    float getFlowRate1() const { return sensor1.flow_rate; }
    float getFlowRate2() const { return sensor2.flow_rate; }
    unsigned long getPulseCount1() const { return sensor1.pulse_count; }
    unsigned long getPulseCount2() const { return sensor2.pulse_count; }
    
    // Utility methods
    float celsiusToFahrenheit(float celsius) const;
    bool isTemperatureBelowThreshold() const;
    void resetPulseCount(int sensor);
    
    // Water meter calibration
    void setPulsesPerGallon(float pulsesPerGallon);
    
    // Status methods
    String getSensorStatusString(const SensorData& sensor) const;
    
    // Water meter specific
    bool hasActiveWaterMeter() const;
    unsigned long getMostRecentPulseTime() const;
    
    // Connection status methods
    bool isActivelyConnected(const SensorData& sensor) const;
    unsigned long getTimeSinceLastPulse(const SensorData& sensor) const;
};

#endif // __SENSOR_MANAGER_H__