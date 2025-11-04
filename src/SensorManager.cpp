#include "SensorManager.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <FunctionalInterrupt.h>


void IRAM_ATTR SensorManager::sensor1PulseISR() {
    sensor1.pulse_count++;
    sensor1.last_pulse_time = millis();
}

void IRAM_ATTR SensorManager::sensor2PulseISR() {
    sensor2.pulse_count++;
    sensor2.last_pulse_time = millis();
}

SensorManager::SensorManager()
    : oneWire1(nullptr),
        oneWire2(nullptr),
        dallasTemp1(nullptr),
        dallasTemp2(nullptr),
        // Initialize sensor data in the initializer list to avoid assignment issues
        sensor1{SensorType::NONE, 0.0f, false, false, 0, 0, 0.0f, 0, 0},
        sensor2{SensorType::NONE, 0.0f, false, false, 0, 0, 0.0f, 0, 0},
        pulsesPerGallon(450.0f)
  {}

SensorManager::~SensorManager() {}

void SensorManager::begin() {
    logger.log("Initializing temperature sensors...");
    
    logger.logDebug(String("Pin configuration - TEMP_METER_PIN: ") + String(TEMP_METER_PIN) + ", TEMP_METER_2_PIN: " + String(TEMP_METER_2_PIN));
    
    // Initialize OneWire instances
    oneWire1 = new OneWire(TEMP_METER_PIN);
    oneWire2 = new OneWire(TEMP_METER_2_PIN);
    
    // Initialize DallasTemperature instances
    dallasTemp1 = new DallasTemperature(oneWire1);
    dallasTemp2 = new DallasTemperature(oneWire2);
    
    // Detect sensor types for each pin
    detectSensorType(TEMP_METER_PIN, sensor1);
    detectSensorType(TEMP_METER_2_PIN, sensor2);
    
    // Set up interrupts for water meteer sensors ONLY if no Dallas sensor found
    if (sensor1.type == SensorType::WATER_METER) {
        // Configure pin for water meter input first
        pinMode(TEMP_METER_PIN, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Only attach interrupt if we're sure it's a water meter
        attachInterrupt(digitalPinToInterrupt(TEMP_METER_PIN), std::bind(&SensorManager::sensor1PulseISR, this), FALLING);
        logger.logf("Sensor 1 (Pin %d): Water meter interrupt attached (FALLING mode)", TEMP_METER_PIN);
        logger.logDebug(String("Sensor 1 interrupt attached to pin ") + String(TEMP_METER_PIN));
    }
    
    if (sensor2.type == SensorType::WATER_METER) {
        // Configure pin for water meter input first
        pinMode(TEMP_METER_2_PIN, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Attach interrupt for water meter pulse detection
        attachInterrupt(digitalPinToInterrupt(TEMP_METER_2_PIN), std::bind(&SensorManager::sensor2PulseISR, this), FALLING);
        logger.logf("Sensor 2 (Pin %d): Water meter interrupt attached (FALLING mode)", TEMP_METER_2_PIN);
        logger.logDebug(String("Sensor 2 interrupt attached to pin ") + String(TEMP_METER_2_PIN));
    }
}

void SensorManager::update() {
    // Update sensor 1
    if (sensor1.type == SensorType::DALLAS_TEMP) {
        readDallasTemperature(dallasTemp1, sensor1);
    } else if (sensor1.type == SensorType::WATER_METER) {
        logWaterMeterPulse(sensor1);
        calculateFlowRate(sensor1);
    }
    
    // Update sensor 2
    if (sensor2.type == SensorType::DALLAS_TEMP) {
        readDallasTemperature(dallasTemp2, sensor2);
    } else if (sensor2.type == SensorType::WATER_METER) {
        logWaterMeterPulse(sensor2);
        calculateFlowRate(sensor2);
    }
}

void SensorManager::logWaterMeterPulse(const SensorData& sensor) const {
    
    // Move logging outside interrupt protection - it's not ISR-safe
    if (sensor.type == SensorType::WATER_METER) {
        // This method is called for logging purposes only
        // The actual pulse logging is done in the update() method
    }
}

void SensorManager::detectSensorType(int pin, SensorData& sensor) {
    // Try to detect Dallas temperature sensor first
    DallasTemperature* dallas = (pin == TEMP_METER_PIN) ? dallasTemp1 : dallasTemp2;
    
    if (dallas) {
        dallas->begin();
        int deviceCount = dallas->getDeviceCount();
        
        if (deviceCount > 0) {
            sensor.type = SensorType::DALLAS_TEMP;
            sensor.is_connected = true;
            sensor.was_detected = true;
            logger.logf("Sensor on pin %d: Detected Dallas temperature sensor (%d devices)", pin, deviceCount);
            return;
        }
    }
    
    // If no Dallas sensor found, configure as water meter
    sensor.type = SensorType::WATER_METER;
    sensor.last_flow_calculation_time = 0; // Initialize flow calculation time
    sensor.was_detected = true;   // But they are "detected" once configured
    sensor.last_pulse_time.store(0); // Initialize pulse time
    sensor.is_connected = false;  // Water meters are "connected" only when pulses are detected
    logger.logf("Sensor on pin %d: No Dallas sensor found, configured as water meter", pin);
}

void SensorManager::readDallasTemperature(DallasTemperature* dallas, SensorData& sensor) {
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
        
        logger.logDebug(String("Dallas temp reading: ") + String(tempC) + "°C (" + String(sensor.temperature_f) + "°F");
    } else {
        sensor.is_connected = false;
        sensor.temperature_f = NAN;
        logger.logf("WARNING: Dallas temperature sensor disconnected on pin %d", 
                   (dallas == dallasTemp1) ? TEMP_METER_PIN : TEMP_METER_2_PIN);
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
        float gallonsInInterval = pulses / pulsesPerGallon;
        sensor.flow_rate = gallonsInInterval * (60000.0f / timeDiff);
        
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
        logger.log("Sensor 1 pulse count reset");
    } else if (sensor == 2) {
        sensor2.pulse_count.store(0);
        sensor2.last_pulse_time.store(0);
        sensor2.last_flow_calculation_time = 0;
        logger.log("Sensor 2 pulse count reset");
    }
}

// Water meter calibration
void SensorManager::setPulsesPerGallon(float pulsesPerGallon) {
    this->pulsesPerGallon = pulsesPerGallon;
    logger.logf("Water meter calibration updated: %.1f pulses per gallon", pulsesPerGallon);
}

bool SensorManager::hasWaterFlowError(int sensor) const {
    // Implementation for flow error detection
    // This could check if pump is running but no flow detected
    return false;  // Placeholder - implement based on requirements
}

float SensorManager::celsiusToFahrenheit(float celsius) const {
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

bool SensorManager::isTemperatureBelowThreshold() const {
    float onThreshold = settingsManager.getTempThresholdOnF();
    float offThreshold = settingsManager.getTempThresholdOffF();
    
    // Check if any temperature sensor is below the ON threshold
    bool anySensorBelowOnThreshold = false;
    if (sensor1.type == SensorType::DALLAS_TEMP && sensor1.is_connected) {
        if (sensor1.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
        }
    }
    
    if (sensor2.type == SensorType::DALLAS_TEMP && sensor2.is_connected) {
        if (sensor2.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
        }
    }
    
    // Check if any temperature sensor is above the OFF threshold
    bool anySensorAboveOffThreshold = false;
    if (sensor1.type == SensorType::DALLAS_TEMP && sensor1.is_connected) {
        if (sensor1.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
        }
    }
    
    if (sensor2.type == SensorType::DALLAS_TEMP && sensor2.is_connected) {
        if (sensor2.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
        }
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
        switch (sensor.type) {
            case SensorType::DALLAS_TEMP:
                return "Dallas Temperature Sensor - Disconnected";
            default:
                return "Sensor - Disconnected";
        }
    }
    
    switch (sensor.type) {
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