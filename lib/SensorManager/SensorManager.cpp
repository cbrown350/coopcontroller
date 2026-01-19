#include "SensorManager.h"
#include "Arduino.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <FunctionalInterrupt.h>
#include <memory>
#include <stdint.h>


void IRAM_ATTR SensorManager::sensor1PulseISR() {
    unsigned long currentTime = millis();
    unsigned long prevPulseTime = sensor1.previous_pulse_time.load();
    
    // Calculate per-pulse flow rate if enabled and we have a previous pulse
    if (settingsManager.getWaterMeterPerPulseCalculationEnabled() && prevPulseTime != 0) {
        // Handle millis() rollover
        unsigned long timeDiff;
        if (currentTime >= prevPulseTime) {
            timeDiff = currentTime - prevPulseTime;
        } else {
            timeDiff = (ULONG_MAX - prevPulseTime) + currentTime;
        }
        
        // Noise filtering: ignore very short pulses (< 10ms)
        if (timeDiff >= 10) {
            // Calculate instantaneous flow rate: GPM = (pulses_per_gallon * 60000) / time_between_pulses_ms
            float pulsesPerGallon = settingsManager.getPulsesPerGallon();
            sensor1.flow_rate = (pulsesPerGallon * 60000.0f) / static_cast<float>(timeDiff);
        }
    }
    
    sensor1.pulse_count++;
    sensor1.previous_pulse_time.store(currentTime);
    sensor1.last_pulse_time = currentTime;
}

void IRAM_ATTR SensorManager::sensor2PulseISR() {
    unsigned long currentTime = millis();
    unsigned long prevPulseTime = sensor2.previous_pulse_time.load();
    
    // Calculate per-pulse flow rate if enabled and we have a previous pulse
    if (settingsManager.getWaterMeterPerPulseCalculationEnabled() && prevPulseTime != 0) {
        // Handle millis() rollover
        unsigned long timeDiff;
        if (currentTime >= prevPulseTime) {
            timeDiff = currentTime - prevPulseTime;
        } else {
            timeDiff = (ULONG_MAX - prevPulseTime) + currentTime;
        }
        
        // Noise filtering: ignore very short pulses (< 10ms)
        if (timeDiff >= 10) {
            // Calculate instantaneous flow rate: GPM = (pulses_per_gallon * 60000) / time_between_pulses_ms
            float pulsesPerGallon = settingsManager.getPulsesPerGallon();
            sensor2.flow_rate = (pulsesPerGallon * 60000.0f) / static_cast<float>(timeDiff);
        }
    }
    
    sensor2.pulse_count++;
    sensor2.previous_pulse_time.store(currentTime);
    sensor2.last_pulse_time = currentTime;
}

SensorManager::SensorManager()
    : oneWire1(nullptr),
        oneWire2(nullptr),
        dallasTemp1(nullptr),
        dallasTemp2(nullptr),
        // Initialize sensor data in the initializer list to avoid assignment issues
        sensor1{SensorType::NONE, 0.0f, false, false, 0, 0, 0.0f, 0, 0},
        sensor2{SensorType::NONE, 0.0f, false, false, 0, 0, 0.0f, 0, 0},
        pulsesPerGallon(settingsManager.getPulsesPerGallon())
{}

void SensorManager::begin(uint8_t sensor_pin1, uint8_t sensor_pin2) {
    this->sensorPin1 = sensor_pin1;
    this->sensorPin2 = sensor_pin2;
    logger.logInfo("Initializing temperature sensors...");
    logger.logInfo(String("Water meter calibration: ") + String(pulsesPerGallon, 1) + " pulses per gallon");

    // put your setup code here, to run once:
    pinMode(sensorPin1, INPUT_PULLUP);
    pinMode(sensorPin2, INPUT_PULLUP);

    
    logger.logDebug(String("Pin configuration - sensor pin 1: ") + String(sensorPin1) + ", sensor pin 2: " + String(sensorPin2));
    
    // Initialize OneWire instances
    oneWire1 = std::make_unique<OneWire>(sensorPin1);
    oneWire2 = std::make_unique<OneWire>(sensorPin2);
    
    // Initialize DallasTemperature instances
    dallasTemp1 = std::make_unique<DallasTemperature>(oneWire1.get());
    dallasTemp2 = std::make_unique<DallasTemperature>(oneWire2.get());
    
    
    // Detect sensor types for each pin
    detectSensorType(sensorPin1, sensor1);
    detectSensorType(sensorPin2, sensor2);
    
    // Set up interrupts for water meteer sensors ONLY if no Dallas sensor found
    if (sensor1.type == SensorType::WATER_METER) {
        // Configure pin for water meter input first
        pinMode(sensorPin1, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Only attach interrupt if we're sure it's a water meter
        attachInterrupt(digitalPinToInterrupt(sensorPin1), std::bind(&SensorManager::sensor1PulseISR, this), FALLING);
        logger.logInfo(String("Sensor 1 (Pin ") + String(sensorPin1) + String("): Water meter interrupt attached (FALLING mode)"));
        logger.logDebug(String("Sensor 1 interrupt attached to pin ") + String(sensorPin1));
    }
    
    if (sensor2.type == SensorType::WATER_METER) {
        // Configure pin for water meter input first
        pinMode(sensorPin2, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Attach interrupt for water meter pulse detection
        attachInterrupt(digitalPinToInterrupt(sensorPin2), std::bind(&SensorManager::sensor2PulseISR, this), FALLING);
        logger.logInfo(String("Sensor 2 (Pin ") + String(sensorPin2) + String("): Water meter interrupt attached (FALLING mode)"));
    }
}

void SensorManager::update() {
    // Update sensor 1
    if (sensor1.type == SensorType::DALLAS_TEMP) {
        readDallasTemperature(dallasTemp1.get(), sensor1);
    } else if (sensor1.type == SensorType::WATER_METER) {
        logWaterMeterPulse(sensor1);
        
        // Use per-pulse calculation if enabled, otherwise use interval-based calculation
        if (settingsManager.getWaterMeterPerPulseCalculationEnabled()) {
            // Check for no-flow timeout (5 seconds)
            unsigned long currentTime = millis();
            unsigned long lastPulseTime = sensor1.last_pulse_time.load();
            
            if (lastPulseTime != 0) {
                // Handle millis() rollover
                unsigned long timeSinceLastPulse;
                if (currentTime >= lastPulseTime) {
                    timeSinceLastPulse = currentTime - lastPulseTime;
                } else {
                    timeSinceLastPulse = (ULONG_MAX - lastPulseTime) + currentTime;
                }
                
                // Set flow rate to 0 if no pulse for 5 seconds
                if (timeSinceLastPulse > 5000) {
                    sensor1.flow_rate = 0.0f;
                }
            }
        } else {
            calculateFlowRate(sensor1);
        }
    }
    
    // Update sensor 2
    if (sensor2.type == SensorType::DALLAS_TEMP) {
        readDallasTemperature(dallasTemp2.get(), sensor2);
    } else if (sensor2.type == SensorType::WATER_METER) {
        logWaterMeterPulse(sensor2);
        
        // Use per-pulse calculation if enabled, otherwise use interval-based calculation
        if (settingsManager.getWaterMeterPerPulseCalculationEnabled()) {
            // Check for no-flow timeout (5 seconds)
            unsigned long currentTime = millis();
            unsigned long lastPulseTime = sensor2.last_pulse_time.load();
            
            if (lastPulseTime != 0) {
                // Handle millis() rollover
                unsigned long timeSinceLastPulse;
                if (currentTime >= lastPulseTime) {
                    timeSinceLastPulse = currentTime - lastPulseTime;
                } else {
                    timeSinceLastPulse = (ULONG_MAX - lastPulseTime) + currentTime;
                }
                
                // Set flow rate to 0 if no pulse for 5 seconds
                if (timeSinceLastPulse > 5000) {
                    sensor2.flow_rate = 0.0f;
                }
            }
        } else {
            calculateFlowRate(sensor2);
        }
    }
}

void SensorManager::logWaterMeterPulse(const SensorData& sensor) const {
    
    // Move logging outside interrupt protection - it's not ISR-safe
    if (sensor.type == SensorType::WATER_METER) {
        // This method is called for logging purposes only
        // The actual pulse logging is done in the update() method
    }
}

void SensorManager::detectSensorType(uint8_t pin, SensorData& sensor) { // NOSONAR - not const due to change to sensor
    // Try to detect Dallas temperature sensor first
    if (DallasTemperature* dallas = (pin == sensorPin1) ? dallasTemp1.get() : dallasTemp2.get()) {
        dallas->begin();
        int deviceCount = dallas->getDeviceCount();
        
        if (deviceCount > 0) {
            sensor.type = SensorType::DALLAS_TEMP;
            sensor.is_connected = true;
            sensor.was_detected = true;
            logger.logInfo(String("Sensor on pin ") + String(pin) + String(": Detected Dallas temperature sensor (") + 
                            String(deviceCount) + String(" devices)"));
            return;
        }
    }
    
    // If no Dallas sensor found, configure as water meter
    sensor.type = SensorType::WATER_METER;
    sensor.last_flow_calculation_time = 0; // Initialize flow calculation time
    sensor.was_detected = true;   // But they are "detected" once configured
    sensor.last_pulse_time.store(0); // Initialize pulse time
    sensor.is_connected = false;  // Water meters are "connected" only when pulses are detected
    logger.logInfo(String("Sensor on pin ") + String(pin) + String(": No Dallas sensor found, configured as water meter"));
}

void SensorManager::readDallasTemperature(DallasTemperature* dallas, SensorData& sensor) { // NOSONAR - not const due to change to sensor
    if (!dallas || sensor.type != SensorType::DALLAS_TEMP) {
        return;
    }
    
    // If sensor was never detected, don't attempt to read
    if (!sensor.was_detected) {
        sensor.is_connected = false;
        sensor.temperature_f = NAN;
        return;
    }
    
    dallas->requestTemperatures();
    float tempC = dallas->getTempCByIndex(0);
    
    if (tempC != DEVICE_DISCONNECTED_C) {
        sensor.temperature_f = celsiusToFahrenheit(tempC);
        sensor.is_connected = true;
        sensor.last_reading_time = millis();
        
        logger.logDebug(String("Dallas temp reading: ") + String(tempC) + "°C (" + String(sensor.temperature_f) + "°F)");
    } else {
        sensor.is_connected = false;
        sensor.temperature_f = NAN;
        logger.logWarning(String("WARNING: Dallas temperature sensor disconnected on pin ") + (dallas == dallasTemp1.get() ? "Sensor 1" : "Sensor 2"));
    }
}

void SensorManager::calculateFlowRate(SensorData& sensor) const {
    if (sensor.type != SensorType::WATER_METER) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Initialize last calculation time if this is first call
    if (sensor.last_flow_calculation_time == 0) {
        sensor.last_flow_calculation_time = currentTime;
        return;
    }
    
    if (currentTime - sensor.last_flow_calculation_time >= FLOW_CALCULATION_INTERVAL) {
        unsigned long pulses = sensor.pulse_count.load();
        unsigned long timeDiff = currentTime - sensor.last_flow_calculation_time;
        
        // Calculate flow rate in gallons per minute
        // pulses / pulsesPerGallon gives gallons in the interval
        // Multiply by (60000.0 / timeDiff) to get gallons per minute
        float gallonsInInterval = static_cast<float>(pulses) / pulsesPerGallon;
        sensor.flow_rate = gallonsInInterval * (60000.0f / static_cast<float>(timeDiff));
        
        // Reset pulse count for next interval
        sensor.pulse_count.store(0);
        sensor.last_flow_calculation_time = currentTime;
        
        logger.logDebug(String("Flow rate calculation - Pulses: ") + String(pulses) + ", Time: " + String(timeDiff) + " ms, Rate: " + String(sensor.flow_rate) + " GPM");
    }
}

void SensorManager::resetPulseCount(int sensor) {
    if (sensor == 1) {
        sensor1.pulse_count.store(0);
        sensor1.last_pulse_time.store(0);
        sensor1.last_flow_calculation_time = 0;
        logger.logInfo("Sensor 1 pulse count reset");
    } else if (sensor == 2) {
        sensor2.pulse_count.store(0);
        sensor2.last_pulse_time.store(0);
        sensor2.last_flow_calculation_time = 0;
        logger.logInfo("Sensor 2 pulse count reset");
    }
}

// Water meter calibration
void SensorManager::setPulsesPerGallon(float pulses_per_gallon) {
    this->pulsesPerGallon = pulses_per_gallon;
    logger.logInfo(String("Water meter calibration updated: ") + String(pulsesPerGallon, 1) + " pulses per gallon");
}

float SensorManager::celsiusToFahrenheit(float celsius) const {
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

bool SensorManager::isTemperatureBelowThreshold() const {
    float onThreshold = settingsManager.getTempThresholdOnF();
    float offThreshold = settingsManager.getTempThresholdOffF();
    
    // Check if any temperature sensor is below the ON threshold
    bool anySensorBelowOnThreshold = false;
    if (sensor1.type == SensorType::DALLAS_TEMP && 
        sensor1.is_connected && 
        sensor1.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
    }
    
    if (sensor2.type == SensorType::DALLAS_TEMP && 
        sensor2.is_connected && 
        sensor2.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
    }
    
    // Check if any temperature sensor is above the OFF threshold
    bool anySensorAboveOffThreshold = false;
        if (sensor1.type == SensorType::DALLAS_TEMP && 
            sensor1.is_connected && 
            sensor1.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
    }
    
    if (sensor2.type == SensorType::DALLAS_TEMP && 
        sensor2.is_connected && 
        sensor2.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
    }
    
    // Return true if any sensor is below ON threshold AND no sensor is above OFF threshold
    return anySensorBelowOnThreshold && !anySensorAboveOffThreshold;
}

String SensorManager::getSensorStatusString(const SensorData& sensor) const {
    if (!sensor.was_detected) {
        switch (sensor.type) {
            case SensorType::DALLAS_TEMP:
                return "Dallas Temperature Sensor - Not Detected";
            case SensorType::WATER_METER:
                return "Water Meter - Not Connected";
            default:
                return "Sensor - Not Detected";
        }
    }
    
    if (sensor.type == SensorType::WATER_METER) {
        if (sensor.pulse_count.load() == 0) {
            return "Water Meter - Configured (No Pulses Detected)";
        }
        if (isActivelyConnected(sensor)) {
            return String("Water Meter - Connected (Active: ") + String(sensor.flow_rate, 2) + " GPM)";
        } else {
            return String("Water Meter - Connected (Idle - Last pulse ") + String(getTimeSinceLastPulse(sensor)) + "s ago)";
        }
    }
    
    // For Dallas sensors and other types
    if (!sensor.is_connected) {
        switch (sensor.type) { // NOSONAR - may add more sensor types later
            case SensorType::DALLAS_TEMP:
                return "Dallas Temperature Sensor - Disconnected";
            default:
                return "Sensor - Disconnected";
        }
    }
    
    switch (sensor.type) { // NOSONAR - may add more sensor types later
        case SensorType::DALLAS_TEMP:
            if (isnan(sensor.temperature_f)) {
                return "Dallas Temperature Sensor - Reading Failed";
            }
            return String("Dallas Temperature Sensor - Connected (") + String(sensor.temperature_f, 1) + "°F)";
        default:
            return "Unknown Sensor Type";
    }
}

bool SensorManager::hasActiveWaterMeter() const {
    return (sensor1.type == SensorType::WATER_METER || sensor2.type == SensorType::WATER_METER);
}

unsigned long SensorManager::getMostRecentPulseTime() const {
    unsigned long max_time = 0;
    if (sensor1.type == SensorType::WATER_METER) {
        max_time = max(max_time, sensor1.last_pulse_time.load());
        logger.logDebug(String("Sensor 1 last pulse time: ") + String(sensor1.last_pulse_time.load()));
    }
    if (sensor2.type == SensorType::WATER_METER) {
        max_time = max(max_time, sensor2.last_pulse_time.load());
        logger.logDebug(String("Sensor 2 last pulse time: ") + String(sensor2.last_pulse_time.load()));
    }
    return max_time;
}

bool SensorManager::isActivelyConnected(const SensorData& sensor) const {
    if (sensor.type == SensorType::WATER_METER) {
        if (sensor.pulse_count.load() == 0) return false;  // Never detected any pulses
        unsigned long currentTime = millis();
        unsigned long lastPulseTime = sensor.last_pulse_time.load();
        
        // Handle millis() rollover
        unsigned long timeSinceLastPulse;
        if (currentTime >= lastPulseTime) {
            timeSinceLastPulse = (currentTime - lastPulseTime) / 1000;
        } else {
            // Rollover occurred
            timeSinceLastPulse = ((ULONG_MAX - lastPulseTime) + currentTime) / 1000;
        }
        return timeSinceLastPulse < settingsManager.getWaterMeterTimeoutSeconds();
    }
    return sensor.is_connected;  // For Dallas sensors, use existing logic
}

unsigned long SensorManager::getTimeSinceLastPulse(const SensorData& sensor) const {
    if (sensor.type != SensorType::WATER_METER || sensor.pulse_count.load() == 0) return 0;
    unsigned long currentTime = millis();
    unsigned long lastPulseTime = sensor.last_pulse_time.load();
    
    // Handle millis() rollover
    if (currentTime >= lastPulseTime) {
        return (currentTime - lastPulseTime) / 1000;
    } else {
        // Rollover occurred
        return ((ULONG_MAX - lastPulseTime) + currentTime) / 1000;
    }
}