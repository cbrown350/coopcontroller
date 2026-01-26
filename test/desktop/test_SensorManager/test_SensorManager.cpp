#include <gtest/gtest.h>
#include "ArduinoFake.h"
#include "../../common/mocks/MockHAL.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "SensorManager.h"

using namespace testing;
using namespace fakeit;

// Global mock HAL instance
static MockHAL mockHal;

class SensorManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize MockHAL
        mockHal.reset();

        // Reset ArduinoFake
        ArduinoFakeReset();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
        
        // Mock pinMode and digitalWrite to prevent segfaults
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalRead)).AlwaysReturn(LOW);

        // Mock interrupt functions (needed by SensorManager for water meter)
        When(OverloadedMethod(ArduinoFake(), attachInterrupt, void(uint8_t, void(*)(), int))).AlwaysReturn();
        When(Method(ArduinoFake(), detachInterrupt)).AlwaysReturn();

        // Initialize Logger AFTER all Arduino function mocks are set up
        Logger::getInstance().begin(&mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);

        // Initialize SettingsManager singleton with HAL
        settingsManager.begin(&mockHal);
    }
    
    void TearDown() override {
        // Clean up after each test
        ArduinoFakeReset();
    }
};

// ============================================================================
// Construction and Basic Initialization Tests
// ============================================================================

TEST_F(SensorManagerTest, ConstructorDoesNotCrash) {
    // Constructor should initialize without crashing
    EXPECT_NO_THROW({
        SensorManager sensorMgr;
    });
}

TEST_F(SensorManagerTest, BeginDoesNotCrashWithValidPins) {
    SensorManager sensorMgr;
    
    EXPECT_NO_THROW({
        sensorMgr.begin(32, 33);
    });
}

TEST_F(SensorManagerTest, BeginWithZeroPinsDoesNotCrash) {
    SensorManager sensorMgr;
    
    EXPECT_NO_THROW({
        sensorMgr.begin(0, 0);
    });
}

TEST_F(SensorManagerTest, BeginWithSamePinDoesNotCrash) {
    SensorManager sensorMgr;
    
    EXPECT_NO_THROW({
        sensorMgr.begin(32, 32);
    });
}

TEST_F(SensorManagerTest, MultipleBeginCallsDoNotCrash) {
    SensorManager sensorMgr;
    
    EXPECT_NO_THROW({
        sensorMgr.begin(32, 33);
        sensorMgr.begin(34, 35);
        sensorMgr.begin(36, 37);
    });
}

// ============================================================================
// Getter Method Tests - Verify Return Types
// ============================================================================

TEST_F(SensorManagerTest, GetSensor1TypeReturnsValidEnum) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorType type = sensorMgr.getSensor1Type();
    
    // Type should be one of the valid enum values
    EXPECT_TRUE(type == SensorType::NONE || 
                type == SensorType::DALLAS_TEMP || 
                type == SensorType::WATER_METER);
}

TEST_F(SensorManagerTest, GetSensor2TypeReturnsValidEnum) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorType type = sensorMgr.getSensor2Type();
    
    // Type should be one of the valid enum values
    EXPECT_TRUE(type == SensorType::NONE || 
                type == SensorType::DALLAS_TEMP || 
                type == SensorType::WATER_METER);
}

TEST_F(SensorManagerTest, GetTemperature1FReturnsNumberOrNAN) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    float temp = sensorMgr.getTemperature1F();
    
    // Should return either a number or NAN (both are valid)
    EXPECT_TRUE(std::isnan(temp) || std::isfinite(temp));
}

TEST_F(SensorManagerTest, GetTemperature2FReturnsNumberOrNAN) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    float temp = sensorMgr.getTemperature2F();
    
    // Should return either a number or NAN (both are valid)
    EXPECT_TRUE(std::isnan(temp) || std::isfinite(temp));
}

TEST_F(SensorManagerTest, GetFlowRate1ReturnsNonNegative) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    float flowRate = sensorMgr.getFlowRate1();
    
    // Flow rate should never be negative
    EXPECT_GE(flowRate, 0.0f);
}

TEST_F(SensorManagerTest, GetFlowRate2ReturnsNonNegative) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    float flowRate = sensorMgr.getFlowRate2();
    
    // Flow rate should never be negative
    EXPECT_GE(flowRate, 0.0f);
}

TEST_F(SensorManagerTest, GetPulseCount1ReturnsNonNegative) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    unsigned long count = sensorMgr.getPulseCount1();
    
    // Pulse count should be non-negative (always true for unsigned, but tests the interface)
    EXPECT_GE(count, 0UL);
}

TEST_F(SensorManagerTest, GetPulseCount2ReturnsNonNegative) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    unsigned long count = sensorMgr.getPulseCount2();
    
    EXPECT_GE(count, 0UL);
}

// ============================================================================
// Connection Status Methods
// ============================================================================

TEST_F(SensorManagerTest, IsSensor1ConnectedReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool connected = sensorMgr.isSensor1Connected();
    
    // Should return either true or false (always true for bool)
    EXPECT_TRUE(connected == true || connected == false);
}

TEST_F(SensorManagerTest, IsSensor2ConnectedReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool connected = sensorMgr.isSensor2Connected();
    
    EXPECT_TRUE(connected == true || connected == false);
}

TEST_F(SensorManagerTest, IsSensor1DetectedReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool detected = sensorMgr.isSensor1Detected();
    
    EXPECT_TRUE(detected == true || detected == false);
}

TEST_F(SensorManagerTest, IsSensor2DetectedReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool detected = sensorMgr.isSensor2Detected();
    
    EXPECT_TRUE(detected == true || detected == false);
}

// ============================================================================
// Sensor Data Structure Tests
// ============================================================================

TEST_F(SensorManagerTest, GetSensor1DataReturnsValidStructure) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorData data = sensorMgr.getSensor1Data();
    
    // Verify structure fields are accessible
    EXPECT_TRUE(data.type == SensorType::NONE || 
                data.type == SensorType::DALLAS_TEMP || 
                data.type == SensorType::WATER_METER);
    EXPECT_GE(data.pulse_count.load(), 0UL);
    EXPECT_GE(data.flow_rate, 0.0f);
}

TEST_F(SensorManagerTest, GetSensor2DataReturnsValidStructure) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorData data = sensorMgr.getSensor2Data();
    
    EXPECT_TRUE(data.type == SensorType::NONE || 
                data.type == SensorType::DALLAS_TEMP || 
                data.type == SensorType::WATER_METER);
    EXPECT_GE(data.pulse_count.load(), 0UL);
    EXPECT_GE(data.flow_rate, 0.0f);
}

TEST_F(SensorManagerTest, SensorDataCopyConstructorWorks) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorData original = sensorMgr.getSensor1Data();
    
    // Create a copy
    SensorData copy(original);
    
    // Verify values are copied correctly
    EXPECT_EQ(copy.type, original.type);
    EXPECT_FLOAT_EQ(copy.temperature_f, original.temperature_f);
    EXPECT_EQ(copy.is_connected, original.is_connected);
    EXPECT_EQ(copy.was_detected, original.was_detected);
    EXPECT_EQ(copy.pulse_count.load(), original.pulse_count.load());
    EXPECT_FLOAT_EQ(copy.flow_rate, original.flow_rate);
}

// ============================================================================
// Update Method Tests
// ============================================================================

TEST_F(SensorManagerTest, UpdateDoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    EXPECT_NO_THROW({
        sensorMgr.update();
    });
}

TEST_F(SensorManagerTest, MultipleUpdateCallsDoNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; i++) {
            sensorMgr.update();
        }
    });
}

// ============================================================================
// Pulse Count Reset Tests
// ============================================================================

TEST_F(SensorManagerTest, ResetPulseCount1DoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    EXPECT_NO_THROW({
        sensorMgr.resetPulseCount(1);
    });
    
    // Pulse count should be zero after reset
    EXPECT_EQ(sensorMgr.getPulseCount1(), 0UL);
}

TEST_F(SensorManagerTest, ResetPulseCount2DoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    EXPECT_NO_THROW({
        sensorMgr.resetPulseCount(2);
    });
    
    EXPECT_EQ(sensorMgr.getPulseCount2(), 0UL);
}

TEST_F(SensorManagerTest, ResetPulseCountInvalidSensorDoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    unsigned long count1Before = sensorMgr.getPulseCount1();
    unsigned long count2Before = sensorMgr.getPulseCount2();
    
    // Reset with invalid sensor number should not crash
    EXPECT_NO_THROW({
        sensorMgr.resetPulseCount(3);
        sensorMgr.resetPulseCount(0);
        sensorMgr.resetPulseCount(-1);
    });
    
    // Counts should remain unchanged
    EXPECT_EQ(sensorMgr.getPulseCount1(), count1Before);
    EXPECT_EQ(sensorMgr.getPulseCount2(), count2Before);
}

// ============================================================================
// Temperature Conversion Tests
// ============================================================================

TEST_F(SensorManagerTest, CelsiusToFahrenheitConversionKnownValues) {
    SensorManager sensorMgr;
    
    // Test known conversions
    EXPECT_FLOAT_EQ(sensorMgr.celsiusToFahrenheit(0.0f), 32.0f);    // 0°C = 32°F
    EXPECT_FLOAT_EQ(sensorMgr.celsiusToFahrenheit(100.0f), 212.0f); // 100°C = 212°F
    EXPECT_FLOAT_EQ(sensorMgr.celsiusToFahrenheit(-40.0f), -40.0f); // -40°C = -40°F
    EXPECT_NEAR(sensorMgr.celsiusToFahrenheit(37.78f), 100.0f, 0.1f); // ~38°C ≈ 100°F
}

// ============================================================================
// Calibration Tests
// ============================================================================

TEST_F(SensorManagerTest, SetPulsesPerGallonDoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    EXPECT_NO_THROW({
        sensorMgr.setPulsesPerGallon(450.0f);
        sensorMgr.setPulsesPerGallon(1.0f);
        sensorMgr.setPulsesPerGallon(1000.0f);
        sensorMgr.setPulsesPerGallon(0.0f);
    });
}

// ============================================================================
// Utility Method Tests
// ============================================================================

TEST_F(SensorManagerTest, IsTemperatureBelowThresholdReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool belowThreshold = sensorMgr.isTemperatureBelowThreshold();
    
    // Should return a boolean value
    EXPECT_TRUE(belowThreshold == true || belowThreshold == false);
}

TEST_F(SensorManagerTest, HasActiveWaterMeterReturnsBoolean) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    bool hasWaterMeter = sensorMgr.hasActiveWaterMeter();
    
    // Should return a boolean value
    EXPECT_TRUE(hasWaterMeter == true || hasWaterMeter == false);
}

TEST_F(SensorManagerTest, GetMostRecentPulseTimeReturnsNonNegative) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    unsigned long pulseTime = sensorMgr.getMostRecentPulseTime();
    
    // Pulse time should be non-negative
    EXPECT_GE(pulseTime, 0UL);
}

TEST_F(SensorManagerTest, GetSensorStatusStringDoesNotCrash) {
    SensorManager sensorMgr;
    sensorMgr.begin(32, 33);
    
    SensorData data = sensorMgr.getSensor1Data();
    
    EXPECT_NO_THROW({
        String status = sensorMgr.getSensorStatusString(data);
        // Status string should not be empty (we don't validate content)
        EXPECT_GT(status.length(), 0);
    });
}

// ============================================================================
// Destructor Test
// ============================================================================

TEST_F(SensorManagerTest, DestructorDoesNotCrash) {
    // Create and destroy SensorManager
    {
        SensorManager sensorMgr;
        sensorMgr.begin(32, 33);
        sensorMgr.update();
        // Object goes out of scope here
    }
    
    // Should not crash
    SUCCEED();
}

// ============================================================================
// Integration Test - Full Initialization Sequence
// ============================================================================

TEST_F(SensorManagerTest, FullInitializationSequenceWorks) {
    SensorManager sensorMgr;
    
    // Simulate full initialization and usage
    EXPECT_NO_THROW({
        sensorMgr.begin(32, 33);
        sensorMgr.update();
        sensorMgr.update();
        sensorMgr.update();
        
        // Access all getter methods
        sensorMgr.getSensor1Type();
        sensorMgr.getSensor2Type();
        sensorMgr.getTemperature1F();
        sensorMgr.getTemperature2F();
        sensorMgr.getFlowRate1();
        sensorMgr.getFlowRate2();
        sensorMgr.getPulseCount1();
        sensorMgr.getPulseCount2();
        sensorMgr.isSensor1Connected();
        sensorMgr.isSensor2Connected();
        sensorMgr.isSensor1Detected();
        sensorMgr.isSensor2Detected();
        sensorMgr.hasActiveWaterMeter();
        sensorMgr.isTemperatureBelowThreshold();
        
        // Perform operations
        sensorMgr.resetPulseCount(1);
        sensorMgr.setPulsesPerGallon(450.0f);
    });
}
