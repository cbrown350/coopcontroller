#ifndef __SENSOR_MANAGER_H__
#define __SENSOR_MANAGER_H__

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <atomic>
#include <string>
#include <memory>

/**
 * @brief Supported sensor types
 *
 * Enumeration of sensor types that can be connected to each pin.
 * Auto-detection determines which type is attached.
 */
enum class SensorType {
    NONE = 0,          ///< No sensor detected
    DALLAS_TEMP,       ///< DS18B20 temperature sensor
    WATER_METER        ///< Pulse-based water flow meter
};

/**
 * @brief Sensor data container
 *
 * Stores all data for a single sensor including type, readings,
 * connection status, and statistics. Uses atomic types for
 * thread-safe access from ISRs.
 *
 * Thread Safety:
 * - pulse_count, last_pulse_time are atomic for ISR access
 * - Other fields updated only in main loop context
 */
struct SensorData {  // NOSONAR - shouldn't warn about destructor since it's defined below
    SensorType type;                              ///< Type of sensor detected
    float temperature_f;                          ///< Temperature reading in Fahrenheit
    bool is_connected;                            ///< Currently connected (recent communication)
    bool was_detected;                            ///< Was ever detected (since boot)
    unsigned long last_reading_time;              ///< Timestamp of last successful read
    std::atomic<unsigned long> pulse_count;       ///< Total pulse count (water meter)
    float flow_rate;                              ///< Calculated flow rate (GPM)
    std::atomic<unsigned long> last_pulse_time;   ///< Timestamp of most recent pulse
    std::atomic<unsigned long> previous_pulse_time; ///< Timestamp of previous pulse (for per-pulse calc)
    unsigned long last_flow_calculation_time;     ///< Last time flow rate was calculated

    /**
     * @brief Constructor with default values
     */
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

/**
 * @brief Sensor manager for temperature and flow sensors
 *
 * Manages up to two sensor pins with auto-detection of sensor type.
 * Supports DS18B20 temperature sensors and pulse-based water meters.
 *
 * Features:
 * - Automatic sensor type detection
 * - Temperature reading in Fahrenheit
 * - Water meter pulse counting with ISR
 * - Flow rate calculation (GPM)
 * - Connection status monitoring
 * - Per-pulse and interval-based flow calculation
 * - Atomic operations for ISR safety
 *
 * Sensor Auto-Detection:
 * - Attempts OneWire detection first (Dallas temp)
 * - Falls back to pulse counting mode (water meter)
 * - Updates connection status based on communication
 *
 * Water Meter Operation:
 * - Counts pulses via interrupt handler
 * - Calculates flow rate based on pulses-per-gallon
 * - Supports both per-pulse and interval calculation methods
 */
class SensorManager {
public:
    /**
     * @brief Virtual destructor
     *
     * Allows mocking in unit tests.
     */
    virtual ~SensorManager() = default;

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
    bool perPulseCalcEnabled_ = false;  // Cached setting for ISR-safe access
    
    // Private methods
    void sensor1PulseISR();
    void sensor2PulseISR();
    void detectSensorType(uint8_t pin, SensorData& sensor);
    void readDallasTemperature(DallasTemperature* dallas, SensorData& sensor);
    void logWaterMeterPulse(const SensorData& sensor) const;
    void calculateFlowRate(SensorData& sensor) const;
    void calculatePerPulseFlowRate(SensorData& sensor) const;
    
public:
    SensorManager();

    // Initialization
    void begin(uint8_t sensorPin1, uint8_t sensorPin2 = 0);
    
    // Main update function - call this in loop()
    void update();
    
    // Get sensor data
    virtual SensorData getSensor1Data() const { return sensor1; }
    virtual SensorData getSensor2Data() const { return sensor2; }

    // Get specific values
    virtual float getTemperature1F() const {
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
    virtual float getTemperature2F() const {
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
    virtual bool isSensor1Connected() const { return sensor1.is_connected; }
    virtual bool isSensor2Connected() const { return sensor2.is_connected; }
    virtual SensorType getSensor1Type() const { return sensor1.type; }
    virtual SensorType getSensor2Type() const { return sensor2.type; }

    // Detection state methods
    virtual bool isSensor1Detected() const { return sensor1.was_detected; }
    virtual bool isSensor2Detected() const { return sensor2.was_detected; }

    // Water meter specific
    virtual float getFlowRate1() const { return sensor1.flow_rate; }
    virtual float getFlowRate2() const { return sensor2.flow_rate; }
    virtual unsigned long getPulseCount1() const { return sensor1.pulse_count; }
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