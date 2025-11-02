#include "SensorManager.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <FunctionalInterrupt.h>


// Interrupt service routines for water meter pulses 
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
       sensor1{SensorType::NONE, 0.0f, false, 0, 0, 0.0f, 0},
       sensor2{SensorType::NONE, 0.0f, false, 0, 0, 0.0f, 0},
       pulseToGallons(0.1f)
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
            logger.logf("Sensor on pin %d: Detected Dallas temperature sensor (%d devices)", pin, deviceCount);
            return;
        }
    }
    
    // If no Dallas sensor found, configure as water meter
    sensor.type = SensorType::WATER_METER;
    sensor.is_connected = true;  // Water meters are considered "connected" once configured
    logger.logf("Sensor on pin %d: No Dallas sensor found, configured as water meter", pin);
}

void SensorManager::readDallasTemperature(DallasTemperature* dallas, SensorData& sensor) {
    if (!dallas || sensor.type != SensorType::DALLAS_TEMP) {
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
        sensor.temperature_f = 0.0f;
        logger.logf("WARNING: Dallas temperature sensor disconnected on pin %d", 
                   (dallas == dallasTemp1) ? TEMP_METER_PIN : TEMP_METER_2_PIN);
    }
}

void SensorManager::calculateFlowRate(SensorData& sensor) const {
    if (sensor.type != SensorType::WATER_METER) {
        return;
    }
    
    static unsigned long lastCalculationTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastCalculationTime >= FLOW_CALCULATION_INTERVAL) {
        unsigned long pulses = sensor.pulse_count.load();
        unsigned long timeDiff = currentTime - lastCalculationTime;
        
        // Calculate flow rate in gallons per minute
        // pulses * pulseToGallons gives gallons in the interval
        // Divide by (timeDiff / 60000.0) to get gallons per minute
        float gallonsInInterval = pulses * pulseToGallons;
        sensor.flow_rate = gallonsInInterval * (60000.0f / timeDiff);
        
        // Reset pulse count for next interval
        sensor.pulse_count.store(0);
        lastCalculationTime = currentTime;
        
        logger.logDebug(String("Flow rate calculation - Pulses: ") + String(pulses) + ", Time: " + String(timeDiff) + " ms, Rate: " + String(sensor.flow_rate) + " GPM");
    }
}

void SensorManager::resetPulseCount(int sensor) {
    if (sensor == 1) {
        sensor1.pulse_count.store(0);
        sensor1.last_pulse_time.store(0);
        logger.log("Sensor 1 pulse count reset");
    } else if (sensor == 2) {
        sensor2.pulse_count.store(0);
        sensor2.last_pulse_time.store(0);
        logger.log("Sensor 2 pulse count reset");
    }
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
    if (!sensor.is_connected) {
        return "Not Connected";
    }
    
    switch (sensor.type) {
        case SensorType::DALLAS_TEMP:
            return String(sensor.temperature_f, 1) + "°F";
        case SensorType::WATER_METER:
            return String(sensor.flow_rate, 2) + " GPM";
        default:
            return "Unknown";
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