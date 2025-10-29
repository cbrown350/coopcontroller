#include "TempSensor.h"
#include "Logger.h"
#include "SettingsManager.h"

// Interrupt service routines for water meter pulses
volatile unsigned long sensor1PulseCount = 0;
volatile unsigned long sensor2PulseCount = 0;
volatile unsigned long sensor1LastPulseTime = 0;
volatile unsigned long sensor2LastPulseTime = 0;

void IRAM_ATTR sensor1PulseISR() {
    sensor1PulseCount++;
    sensor1LastPulseTime = millis();
}

void IRAM_ATTR sensor2PulseISR() {
    sensor2PulseCount++;
    sensor2LastPulseTime = millis();
}

TempSensor::TempSensor() {
    oneWire1 = nullptr;
    oneWire2 = nullptr;
    dallasTemp1 = nullptr;
    dallasTemp2 = nullptr;
    
    // Initialize sensor data
    sensor1 = {SENSOR_TYPE_NONE, 0.0f, false, 0, 0, 0.0f, 0};
    sensor2 = {SENSOR_TYPE_NONE, 0.0f, false, 0, 0, 0.0f, 0};
    
    // Default pulse to gallons conversion (typical water meter: 1 pulse = 0.1 gallons)
    pulseToGallons = 0.1f;
}

TempSensor::~TempSensor() {
    if (oneWire1) delete oneWire1;
    if (oneWire2) delete oneWire2;
    if (dallasTemp1) delete dallasTemp1;
    if (dallasTemp2) delete dallasTemp2;
}

void TempSensor::begin() {
    logger.log("Initializing temperature sensors...");
    
    if (settingsManager.getDebugEnabled()) {
        logger.logf("DEBUG: Pin configuration - TEMP_METER_PIN: %d, TEMP_METER_2_PIN: %d", TEMP_METER_PIN, TEMP_METER_2_PIN);
        logger.logf("DEBUG: OUT_PUMP_PIN: %d, OUT_LIGHT_PIN: %d", OUT_PUMP_PIN, OUT_LIGHT_PIN);
    }
    
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
    if (sensor1.type == SENSOR_TYPE_WATER_METER) {
        // Configure pin for water meter input first
        pinMode(TEMP_METER_PIN, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Only attach interrupt if we're sure it's a water meter
        attachInterrupt(digitalPinToInterrupt(TEMP_METER_PIN), sensor1PulseISR, FALLING);
        logger.logf("Sensor 1 (Pin %d): Water meter interrupt attached (FALLING mode)", TEMP_METER_PIN);
        if (settingsManager.getDebugEnabled()) {
            logger.logf("DEBUG: Sensor 1 interrupt attached to pin %d", TEMP_METER_PIN);
        }
    }
    
    if (sensor2.type == SENSOR_TYPE_WATER_METER) {
        // Configure pin for water meter input first
        pinMode(TEMP_METER_2_PIN, INPUT_PULLUP);
        delay(10); // Small delay to let pin stabilize
        
        // Attach interrupt for water meter pulse detection
        attachInterrupt(digitalPinToInterrupt(TEMP_METER_2_PIN), sensor2PulseISR, FALLING);
        logger.logf("Sensor 2 (Pin %d): Water meter interrupt attached (FALLING mode)", TEMP_METER_2_PIN);
        if (settingsManager.getDebugEnabled()) {
            logger.logf("DEBUG: Sensor 2 interrupt attached to pin %d", TEMP_METER_2_PIN);
        }
    }
    
    logger.log("Temperature sensor initialization complete");
}

void TempSensor::detectSensorType(int pin, SensorData& sensor) {
    if (settingsManager.getDebugEnabled()) {
        Serial.printf("DEBUG: Starting sensor detection on pin %d\n", pin);
        Serial.printf("DEBUG: Pin %d state before detection: %d\n", pin, digitalRead(pin));
    }
    
    // Try to detect Dallas temperature sensor first
    DallasTemperature* testDallas = (pin == TEMP_METER_PIN) ? dallasTemp1 : dallasTemp2;
    
    if (settingsManager.getDebugEnabled()) {
        Serial.printf("DEBUG: Using DallasTemperature instance at address %p\n", (void*)testDallas);
        Serial.printf("DEBUG: OneWire instance address: %p\n", (void*)((pin == TEMP_METER_PIN) ? oneWire1 : oneWire2));
    }
    
    testDallas->begin();
    int deviceCount = testDallas->getDeviceCount();
    
    if (settingsManager.getDebugEnabled()) {
        Serial.printf("DEBUG: Dallas device count on pin %d: %d\n", pin, deviceCount);
    }
    
    if (deviceCount > 0) {
        sensor.type = SENSOR_TYPE_DALLAS_TEMP;
        sensor.is_connected = true;
        logger.logf("Sensor on pin %d: Dallas temperature sensor detected (%d devices)", pin, deviceCount);
        
        // Set resolution for all devices
        testDallas->setResolution(12); // 12-bit resolution
        
        if (settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Dallas temperature sensor successfully detected on pin %d\n", pin);
            // Try to read a test temperature
            testDallas->requestTemperatures();
            delay(750); // Wait for conversion
            float testTemp = testDallas->getTempCByIndex(0);
            if (testTemp != DEVICE_DISCONNECTED_C) {
                Serial.printf("DEBUG: Test temperature reading: %.2f°C (%.1f°F)\n", testTemp, celsiusToFahrenheit(testTemp));
            } else {
                Serial.println("DEBUG: Test temperature reading failed");
            }
        }
    } else {
        // Check if it's a water meter by looking for initial state
        // Water meters can be either HIGH or LOW when idle, so we'll test for activity
        pinMode(pin, INPUT_PULLUP);
        delay(100); // Allow pin to stabilize
        
        int pinState = digitalRead(pin);
        if (settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Pin %d state after pull-up: %d\n", pin, pinState);
        }
        
        // For water meter detection, we'll assume it's a water meter if no Dallas sensor is found
        // The actual functionality will be determined by pulse detection
        sensor.type = SENSOR_TYPE_WATER_METER;
        sensor.is_connected = true;
        logger.logf("Sensor on pin %d: Water meter configured (no Dallas sensor found, pin state: %d)", pin, pinState);
        if (settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Water meter configured on pin %d (pin state: %d)\n", pin, pinState);
        }
    }
}

void TempSensor::update() {
    // Update sensor 1
    if (sensor1.type == SENSOR_TYPE_DALLAS_TEMP) {
        readDallasTemperature(dallasTemp1, sensor1);
    } else if (sensor1.type == SENSOR_TYPE_WATER_METER) {
        handleWaterMeterPulse(sensor1);
        calculateFlowRate(sensor1);
    }
    
    // Update sensor 2
    if (sensor2.type == SENSOR_TYPE_DALLAS_TEMP) {
        readDallasTemperature(dallasTemp2, sensor2);
    } else if (sensor2.type == SENSOR_TYPE_WATER_METER) {
        handleWaterMeterPulse(sensor2);
        calculateFlowRate(sensor2);
    }
}

void TempSensor::readDallasTemperature(DallasTemperature* dallas, SensorData& sensor) {
    unsigned long currentTime = millis();
    
    // Read temperature every 5 seconds
    if (currentTime - sensor.last_reading_time >= 5000) {
        // Request temperatures from all devices on the bus
        dallas->requestTemperatures();
        
        // Wait for conversion to complete (minimum 750ms for 12-bit resolution)
        delay(750);
        
        // Read temperature from first device
        float tempC = dallas->getTempCByIndex(0);
        
        if (tempC != DEVICE_DISCONNECTED_C && !isnan(tempC) && !isinf(tempC)) {
            // Valid temperature reading
            sensor.temperature_f = celsiusToFahrenheit(tempC);
            sensor.is_connected = true;
            
            if (settingsManager.getDebugEnabled()) {
                Serial.printf("DEBUG: Temperature reading successful: %.2f°C (%.1f°F)\n", 
                             tempC, sensor.temperature_f);
            }
        } else {
            // Sensor disconnected or error
            sensor.is_connected = false;
            sensor.temperature_f = 0.0f;
            
            if (settingsManager.getDebugEnabled()) {
                Serial.printf("DEBUG: Temperature reading failed or sensor disconnected\n");
            }
            logger.log("Temperature sensor disconnected or reading error");
        }
        
        sensor.last_reading_time = currentTime;
    }
}

void TempSensor::handleWaterMeterPulse(SensorData& sensor) {
    noInterrupts();
    if (sensor.type == SENSOR_TYPE_WATER_METER) {
        // Update pulse count from volatile variables
        unsigned long currentPulseCount, currentLastPulseTime;
        if (&sensor == &sensor1) {
            currentPulseCount = sensor1PulseCount;
            currentLastPulseTime = sensor1LastPulseTime;
        } else {
            currentPulseCount = sensor2PulseCount;
            currentLastPulseTime = sensor2LastPulseTime;
        }
        
        sensor.pulse_count = currentPulseCount;
        sensor.last_pulse_time = currentLastPulseTime;
    }
    interrupts();
    
    // Move logging outside interrupt protection - it's not ISR-safe
    if (sensor.type == SENSOR_TYPE_WATER_METER) {
        // Check if we have new pulses since last update (outside interrupts)
        static unsigned long lastSensor1PulseCount = 0;
        static unsigned long lastSensor2PulseCount = 0;
        unsigned long newPulses = 0;
        int sensorNum = (&sensor == &sensor1) ? 1 : 2;
        int sensorPin = (&sensor == &sensor1) ? TEMP_METER_PIN : TEMP_METER_2_PIN;
        
        if (&sensor == &sensor1) {
            newPulses = sensor.pulse_count - lastSensor1PulseCount;
            lastSensor1PulseCount = sensor.pulse_count;
        } else {
            newPulses = sensor.pulse_count - lastSensor2PulseCount;
            lastSensor2PulseCount = sensor.pulse_count;
        }
        
        if (newPulses > 0) {
            // Always log when flow is detected, not just in debug mode
            logger.logf("Water flow detected on Sensor %d (Pin %d): %lu new pulses, total: %lu", 
                       sensorNum, sensorPin, newPulses, sensor.pulse_count);
                       
            if (settingsManager.getDebugEnabled()) {
                Serial.printf("DEBUG: Water meter detected %lu new pulses on sensor %d\n", 
                           newPulses, sensorNum);
            }
        }
    }
}

void TempSensor::calculateFlowRate(SensorData& sensor) {
    unsigned long currentTime = millis();
    
    // Calculate flow rate every minute
    if (currentTime - sensor.last_reading_time >= FLOW_CALCULATION_INTERVAL) {
        // Calculate pulses in the last minute
        unsigned long pulseDelta;
        
        if (&sensor == &sensor1) {
            static unsigned long lastPulseCount1 = 0;
            pulseDelta = sensor.pulse_count - lastPulseCount1;
            lastPulseCount1 = sensor.pulse_count;
        } else {
            static unsigned long lastPulseCount2 = 0;
            pulseDelta = sensor.pulse_count - lastPulseCount2;
            lastPulseCount2 = sensor.pulse_count;
        }
        
        // Calculate gallons per minute
        sensor.flow_rate = (pulseDelta * pulseToGallons) / (FLOW_CALCULATION_INTERVAL / 60000.0f);
        sensor.last_reading_time = currentTime;
        
        // Log flow activity if there's flow
        if (pulseDelta > 0 && settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Water flow detected on sensor %d: %.2f GPM (%lu pulses)\n", 
                       (&sensor == &sensor1) ? 1 : 2, sensor.flow_rate, pulseDelta);
        }
    }
}

float TempSensor::celsiusToFahrenheit(float celsius) const {
    // Add safety check for invalid input
    if (isnan(celsius) || isinf(celsius)) {
        if (settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Invalid temperature input: %.2f\n", celsius);
        }
        return 0.0f; // Return safe default
    }
    
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

bool TempSensor::isTemperatureBelowThreshold() const {
    float onThreshold = settingsManager.getTempThresholdOnF();
    float offThreshold = settingsManager.getTempThresholdOffF();
    
    // Check if any temperature sensor is below the ON threshold
    bool anySensorBelowOnThreshold = false;
    if (sensor1.type == SENSOR_TYPE_DALLAS_TEMP && sensor1.is_connected) {
        if (sensor1.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
        }
    }
    
    if (sensor2.type == SENSOR_TYPE_DALLAS_TEMP && sensor2.is_connected) {
        if (sensor2.temperature_f < onThreshold) {
            anySensorBelowOnThreshold = true;
        }
    }
    
    // Check if any temperature sensor is above the OFF threshold
    bool anySensorAboveOffThreshold = false;
    if (sensor1.type == SENSOR_TYPE_DALLAS_TEMP && sensor1.is_connected) {
        if (sensor1.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
        }
    }
    
    if (sensor2.type == SENSOR_TYPE_DALLAS_TEMP && sensor2.is_connected) {
        if (sensor2.temperature_f > offThreshold) {
            anySensorAboveOffThreshold = true;
        }
    }
    
    // Return true if any sensor is below ON threshold AND no sensor is above OFF threshold
    return anySensorBelowOnThreshold && !anySensorAboveOffThreshold;
}

void TempSensor::resetPulseCount(int sensorNum) {
    noInterrupts();
    if (sensorNum == 1) {
        sensor1PulseCount = 0;
        sensor1.pulse_count = 0;
    } else if (sensorNum == 2) {
        sensor2PulseCount = 0;
        sensor2.pulse_count = 0;
    }
    interrupts();
}

String TempSensor::getSensorStatusString(const SensorData& sensor) const {
    if (!sensor.is_connected) {
        return "Not Connected";
    }
    
    switch (sensor.type) {
        case SENSOR_TYPE_DALLAS_TEMP:
            return String(sensor.temperature_f, 1) + "°F";
        case SENSOR_TYPE_WATER_METER:
            return String(sensor.flow_rate, 2) + " GPM";
        default:
            return "Unknown";
    }
}

bool TempSensor::hasWaterFlowError(int sensorNum) const {
    const SensorData* sensor = (sensorNum == 1) ? &sensor1 : &sensor2;
    
    if (sensor->type != SENSOR_TYPE_WATER_METER || !sensor->is_connected) {
        return false;
    }
    
    // Check if there's been no pulse activity for the configured timeout period
    unsigned long currentTime = millis();
    int timeoutSeconds = settingsManager.getWaterFlowErrorTimeoutSeconds();
    unsigned long timeoutMs = (unsigned long)timeoutSeconds * 1000;
    if (currentTime - sensor->last_pulse_time > timeoutMs) {
        if (settingsManager.getDebugEnabled()) {
            Serial.printf("DEBUG: Water flow error detected on sensor %d - no flow for %lu ms (timeout: %d seconds)\n", 
                       sensorNum, currentTime - sensor->last_pulse_time, timeoutSeconds);
        }
        return true;
    }
    
    return false;
}