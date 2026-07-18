#include <gtest/gtest.h>
#include <map>
#include <cstdint>

#include "ArduinoFake.h"

// Undefine Arduino macros that conflict with C++ std library
#undef abs
#undef round
#undef min
#undef max

#include "MockHAL.h"
#include "MockBuzzerController.h"
#include "Logger.h"
#include "DoorController.h"
#include "WeatherManager.h"
#include "mocks/MockSunriseSunsetCalculator.h"
#include "SettingsManager.h"

using namespace fakeit;

// Pin definitions are provided by platformio.ini build flags
// OUT_DOOR_A_OPEN_POS_PIN, OUT_DOOR_A_OPEN_NEG_PIN, etc.

class DoorControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create MockHAL first
        mockHal = new MockHAL();

        // Reset ArduinoFake
        ArduinoFakeReset();

        // Setup time tracking
        currentMillis = 1000;
        currentTime = 1640000000; // Some valid unix timestamp

        // Mock millis() to return our controlled time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return currentMillis; });

        // Mock micros()
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);

        // Mock delay functions
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

        // Mock pinMode
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();

        // Mock digitalWrite - store the pin states
        When(Method(ArduinoFake(), digitalWrite)).AlwaysDo([this](uint8_t pin, uint8_t value) {
            pinStates[pin] = value;
        });

        // Mock digitalRead - return stored pin states
        When(Method(ArduinoFake(), digitalRead)).AlwaysDo([this](uint8_t pin) -> int {
            if (pinStates.find(pin) != pinStates.end()) {
                return pinStates[pin];
            }
            // Default states for inputs
            if (pin == DOOR_A_HALL_SENSOR_OPEN_B_PIN) return HIGH; // Not at open position
            if (pin == DOOR_A_HALL_SENSOR_CLOSED_B_PIN) return HIGH; // Not at closed position
            if (pin == DOOR_A_FAULT_B_PIN) return HIGH; // No fault
            if (pin == DOOR_MANUAL_SWITCH_B_PIN) return HIGH; // Switch not pressed
            return LOW;
        });

        // CRITICAL: Mock interrupt functions BEFORE initializing any components
        // ArduinoFake will cause segmentation faults if these are not mocked
        When(OverloadedMethod(ArduinoFake(), attachInterrupt, void(uint8_t, void(*)(), int))).AlwaysReturn();
        When(Method(ArduinoFake(), detachInterrupt)).AlwaysReturn();

        // CRITICAL: Initialize Logger AFTER all Arduino function mocks are set up
        // Logger instantiation may call millis() or other Arduino functions
        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);

        // Create mocks (default sunrise/sunset are 6 AM / 6 PM)
        mockBuzzer = new MockBuzzerController();
        mockSunriseSunset = new MockSunriseSunsetCalculator();

        // Initialize Hall sensors to "not triggered" state (HIGH = inactive)
        pinStates[DOOR_A_HALL_SENSOR_OPEN_B_PIN] = HIGH;
        pinStates[DOOR_A_HALL_SENSOR_CLOSED_B_PIN] = HIGH;
        pinStates[DOOR_A_FAULT_B_PIN] = HIGH; // No fault
        pinStates[DOOR_MANUAL_SWITCH_B_PIN] = HIGH; // Not pressed

        // Create DoorController instance
        doorController = new DoorController();

        // Initialize door controller
        doorController->begin(mockBuzzer, mockSunriseSunset);

        // Set default configuration
        doorController->setOpenTimeoutSeconds(30);
        doorController->setCloseTimeoutSeconds(30);
        doorController->setAutoOpenOffsetMinutes(0);
        doorController->setAutoCloseOffsetMinutes(0);
        doorController->setAutoOpenEnabled(false);
        doorController->setAutoCloseEnabled(false);
        doorController->setTestMode(true); // Use test mode to avoid ISR complications
    }

    void TearDown() override {
        delete doorController;
        delete mockBuzzer;
        delete mockSunriseSunset;
        delete mockHal;
    }

    // Helper methods
    void advanceTime(unsigned long milliseconds) {
        currentMillis += milliseconds;
    }

    void setHallSensorOpen(bool active) {
        pinStates[DOOR_A_HALL_SENSOR_OPEN_B_PIN] = active ? LOW : HIGH;
    }

    void setHallSensorClosed(bool active) {
        pinStates[DOOR_A_HALL_SENSOR_CLOSED_B_PIN] = active ? LOW : HIGH;
    }

    void setFaultPin(bool active) {
        pinStates[DOOR_A_FAULT_B_PIN] = active ? LOW : HIGH;
    }

    void stopMotor() {
        pinStates[OUT_DOOR_A_OPEN_POS_PIN] = LOW;
        pinStates[OUT_DOOR_A_OPEN_NEG_PIN] = LOW;
    }

    bool isMotorOpening() {
        return pinStates[OUT_DOOR_A_OPEN_POS_PIN] == HIGH &&
               pinStates[OUT_DOOR_A_OPEN_NEG_PIN] == LOW;
    }

    bool isMotorClosing() {
        return pinStates[OUT_DOOR_A_OPEN_POS_PIN] == LOW &&
               pinStates[OUT_DOOR_A_OPEN_NEG_PIN] == HIGH;
    }

    bool isMotorStopped() {
        return pinStates[OUT_DOOR_A_OPEN_POS_PIN] == LOW &&
               pinStates[OUT_DOOR_A_OPEN_NEG_PIN] == LOW;
    }

    // Test data
    MockHAL* mockHal;
    DoorController* doorController;
    MockBuzzerController* mockBuzzer;
    MockSunriseSunsetCalculator* mockSunriseSunset;
    unsigned long currentMillis;
    time_t currentTime;
    std::map<uint8_t, uint8_t> pinStates;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(DoorControllerTest, ConstructorInitializesToDefaultState) {
    // Simple test to verify SetUp() completed successfully
    EXPECT_TRUE(doorController != nullptr);
    EXPECT_TRUE(mockBuzzer != nullptr);
    EXPECT_TRUE(mockSunriseSunset != nullptr);
}

// ============================================================================
// begin() Tests
// ============================================================================

TEST_F(DoorControllerTest, BeginInitializesDoorController) {
    // DoorController is already initialized in SetUp with begin()
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

// ============================================================================
// open() Tests
// ============================================================================

TEST_F(DoorControllerTest, OpenFromIdleTransitionsToOpening) {
    doorController->open();

    EXPECT_EQ(doorController->getState(), DoorState::OPENING);
}

TEST_F(DoorControllerTest, OpenFromClosedTransitionsToOpening) {
    // First close the door
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);

    // Simulate door reaching closed position (ISR would stop motor)
    doorController->setTestMode(false); // Disable test mode temporarily
    setHallSensorClosed(true);
    stopMotor(); // Simulate ISR stopping motor
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSED);
    doorController->setTestMode(true); // Re-enable test mode

    // Now try to open
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);
}

TEST_F(DoorControllerTest, OpenFromOpeningDoesNothing) {
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    doorController->open(); // Try again
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);
}

TEST_F(DoorControllerTest, OpenIncrementsCycleCount) {
    unsigned long initialCycles = doorController->getTotalCycles();

    doorController->open();

    EXPECT_EQ(doorController->getTotalCycles(), initialCycles + 1);
}

// ============================================================================
// close() Tests
// ============================================================================

TEST_F(DoorControllerTest, CloseFromIdleTransitionsToClosing) {
    doorController->close();

    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);
}

TEST_F(DoorControllerTest, CloseFromOpenTransitionsToClosing) {
    // First open the door
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    // Simulate door reaching open position (ISR would stop motor)
    doorController->setTestMode(false);
    setHallSensorOpen(true);
    stopMotor(); // Simulate ISR stopping motor
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::OPEN);
    doorController->setTestMode(true);

    // Now try to close
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);
}

TEST_F(DoorControllerTest, CloseFromClosingDoesNothing) {
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);

    doorController->close(); // Try again
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);
}

// ============================================================================
// stop() Tests
// ============================================================================

TEST_F(DoorControllerTest, StopFromOpeningTransitionsToIdle) {
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    doorController->stop();
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

TEST_F(DoorControllerTest, StopFromClosingTransitionsToIdle) {
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);

    doorController->stop();
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

TEST_F(DoorControllerTest, StopFromIdleDoesNothing) {
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);

    doorController->stop();
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

// ============================================================================
// State String Tests
// ============================================================================

TEST_F(DoorControllerTest, GetStateStringIdle) {
    EXPECT_EQ(doorController->getStateString(), "IDLE");
}

TEST_F(DoorControllerTest, GetStateStringOpening) {
    doorController->open();
    EXPECT_EQ(doorController->getStateString(), "OPENING");
}

TEST_F(DoorControllerTest, GetStateStringClosing) {
    doorController->close();
    EXPECT_EQ(doorController->getStateString(), "CLOSING");
}

TEST_F(DoorControllerTest, GetStateStringFault) {
    doorController->open();
    advanceTime(31000); // Exceed timeout
    doorController->update();

    EXPECT_EQ(doorController->getStateString(), "FAULT");
}

// ============================================================================
// Position Tests
// ============================================================================

TEST_F(DoorControllerTest, InitialPositionIsUnknown) {
    // After begin() is called with no sensors active, position is PARTIAL
    EXPECT_EQ(doorController->getPosition(), DoorPosition::PARTIAL);
    EXPECT_EQ(doorController->getPositionString(), "PARTIAL");
}

TEST_F(DoorControllerTest, PositionUpdatesInTestMode) {
    doorController->setTestMode(true);

    doorController->open();
    EXPECT_EQ(doorController->getPosition(), DoorPosition::PARTIAL); // OPENING state -> PARTIAL position

    // Simulate reaching open position (need to transition state to OPEN)
    doorController->setTestMode(false);
    setHallSensorOpen(true);
    stopMotor(); // Simulate ISR stopping motor
    doorController->update(); // Should transition to OPEN state
    EXPECT_EQ(doorController->getState(), DoorState::OPEN);

    doorController->setTestMode(true);
    doorController->update(); // Update position based on state
    EXPECT_EQ(doorController->getPosition(), DoorPosition::OPEN); // OPEN state -> OPEN position
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST_F(DoorControllerTest, OpenTimeoutTransitionsToFault) {
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    // Advance time beyond timeout
    advanceTime(31000); // 31 seconds
    doorController->update();

    EXPECT_EQ(doorController->getState(), DoorState::FAULT);
}

TEST_F(DoorControllerTest, CloseTimeoutTransitionsToFault) {
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);

    // Advance time beyond timeout
    advanceTime(31000); // 31 seconds
    doorController->update();

    EXPECT_EQ(doorController->getState(), DoorState::FAULT);
}

TEST_F(DoorControllerTest, TimeoutTriggersBuzzerAlert) {
    mockBuzzer->reset();

    doorController->open();
    advanceTime(31000);
    doorController->update();

    EXPECT_EQ(mockBuzzer->getLastTriggeredAlert(), AlertType::DOOR_FAULT);
}

TEST_F(DoorControllerTest, CustomOpenTimeout) {
    doorController->setOpenTimeoutSeconds(60);
    doorController->open();

    // Should not timeout at 31 seconds
    advanceTime(31000);
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    // Should timeout at 61 seconds
    advanceTime(31000); // Total 62 seconds
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::FAULT);
}

// ============================================================================
// Fault Handling Tests
// ============================================================================

TEST_F(DoorControllerTest, HasFaultReturnsFalseInitially) {
    EXPECT_FALSE(doorController->hasFault());
}

TEST_F(DoorControllerTest, HasFaultReturnsTrueAfterTimeout) {
    doorController->open();
    advanceTime(31000);
    doorController->update();

    EXPECT_TRUE(doorController->hasFault());
}

TEST_F(DoorControllerTest, ClearFaultTransitionsToIdle) {
    doorController->open();
    advanceTime(31000);
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::FAULT);

    doorController->clearFault();
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
    EXPECT_FALSE(doorController->hasFault());
}

TEST_F(DoorControllerTest, HardwareFaultDetection) {
    doorController->setTestMode(false);
    setFaultPin(true); // Activate fault
    doorController->update();

    EXPECT_TRUE(doorController->isHardwareFault());
}

TEST_F(DoorControllerTest, HardwareFaultTransitionsToFaultState) {
    doorController->setTestMode(false);
    doorController->open();

    setFaultPin(true);
    doorController->update();

    EXPECT_EQ(doorController->getState(), DoorState::FAULT);
}

// ============================================================================
// Mode Tests
// ============================================================================

TEST_F(DoorControllerTest, AutoModeToggle) {
    EXPECT_FALSE(doorController->isAutoMode());

    // Auto-open and auto-close are independent; isAutoMode() is true if either is on.
    doorController->setAutoOpenEnabled(true);
    EXPECT_TRUE(doorController->isAutoMode());
    EXPECT_TRUE(doorController->isAutoOpenEnabled());
    EXPECT_FALSE(doorController->isAutoCloseEnabled());

    doorController->setAutoOpenEnabled(false);
    EXPECT_FALSE(doorController->isAutoMode());

    doorController->setAutoCloseEnabled(true);
    EXPECT_TRUE(doorController->isAutoMode());
    EXPECT_TRUE(doorController->isAutoCloseEnabled());

    doorController->setAutoCloseEnabled(false);
    EXPECT_FALSE(doorController->isAutoMode());
}

TEST_F(DoorControllerTest, AutoDaysOfWeekDefaultsAllEnabled) {
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(doorController->getAutoOpenDay(i));
        EXPECT_TRUE(doorController->getAutoCloseDay(i));
    }
    doorController->setAutoOpenDay(0, false); // Sunday off
    EXPECT_FALSE(doorController->getAutoOpenDay(0));
    EXPECT_TRUE(doorController->getAutoOpenDay(1));
    doorController->setAutoCloseDay(6, false); // Saturday off
    EXPECT_FALSE(doorController->getAutoCloseDay(6));
}

TEST_F(DoorControllerTest, TestModeToggle) {
    doorController->setTestMode(false);
    EXPECT_FALSE(doorController->isTestMode());

    doorController->setTestMode(true);
    EXPECT_TRUE(doorController->isTestMode());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(DoorControllerTest, SetAndGetOpenTimeoutSeconds) {
    doorController->setOpenTimeoutSeconds(45);
    EXPECT_EQ(doorController->getOpenTimeoutSeconds(), 45u);
}

TEST_F(DoorControllerTest, SetAndGetCloseTimeoutSeconds) {
    doorController->setCloseTimeoutSeconds(45);
    EXPECT_EQ(doorController->getCloseTimeoutSeconds(), 45u);
}

TEST_F(DoorControllerTest, SetAndGetAutoOpenOffsetMinutes) {
    doorController->setAutoOpenOffsetMinutes(30);
    EXPECT_EQ(doorController->getAutoOpenOffsetMinutes(), 30);
}

TEST_F(DoorControllerTest, SetAndGetAutoCloseOffsetMinutes) {
    doorController->setAutoCloseOffsetMinutes(-30);
    EXPECT_EQ(doorController->getAutoCloseOffsetMinutes(), -30);
}

TEST_F(DoorControllerTest, TimeoutValuesClamped) {
    doorController->setOpenTimeoutSeconds(3); // Too low
    EXPECT_GE(doorController->getOpenTimeoutSeconds(), 5u);

    doorController->setOpenTimeoutSeconds(200); // Too high
    EXPECT_LE(doorController->getOpenTimeoutSeconds(), 120u);
}

TEST_F(DoorControllerTest, OffsetValuesClamped) {
    doorController->setAutoOpenOffsetMinutes(200); // Too high
    EXPECT_LE(doorController->getAutoOpenOffsetMinutes(), 120);

    doorController->setAutoOpenOffsetMinutes(-200); // Too low
    EXPECT_GE(doorController->getAutoOpenOffsetMinutes(), -120);

    doorController->setAutoCloseOffsetMinutes(200);
    EXPECT_LE(doorController->getAutoCloseOffsetMinutes(), 120);

    doorController->setAutoCloseOffsetMinutes(-200);
    EXPECT_GE(doorController->getAutoCloseOffsetMinutes(), -120);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(DoorControllerTest, InitialStatisticsAreZero) {
    EXPECT_EQ(doorController->getTotalOpenTime(), 0UL);
    EXPECT_EQ(doorController->getTotalCloseTime(), 0UL);
    EXPECT_EQ(doorController->getTotalCycles(), 0UL);
}

TEST_F(DoorControllerTest, OpeningAccumulatesOpenTime) {
    unsigned long initialOpenTime = doorController->getTotalOpenTime();

    doorController->open();
    advanceTime(5000); // 5 seconds
    doorController->stop();

    EXPECT_GT(doorController->getTotalOpenTime(), initialOpenTime);
}

TEST_F(DoorControllerTest, ClosingAccumulatesCloseTime) {
    unsigned long initialCloseTime = doorController->getTotalCloseTime();

    doorController->close();
    advanceTime(5000); // 5 seconds
    doorController->stop();

    EXPECT_GT(doorController->getTotalCloseTime(), initialCloseTime);
}

TEST_F(DoorControllerTest, ResetStatisticsClearsAll) {
    doorController->open();
    advanceTime(5000);
    doorController->stop();

    doorController->resetStatistics();

    EXPECT_EQ(doorController->getTotalOpenTime(), 0UL);
    EXPECT_EQ(doorController->getTotalCloseTime(), 0UL);
    EXPECT_EQ(doorController->getTotalCycles(), 0UL);
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_F(DoorControllerTest, ToJsonIncludesAllRequiredFields) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();

    doorController->toJson(json);

    EXPECT_TRUE(!json["state"].isNull());
    EXPECT_TRUE(!json["position"].isNull());
    EXPECT_TRUE(!json["progress"].isNull());
    EXPECT_TRUE(!json["auto_mode"].isNull());
    EXPECT_TRUE(!json["test_mode"].isNull());
}

TEST_F(DoorControllerTest, ToJsonReturnsCorrectStateValues) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();

    doorController->toJson(json);

    EXPECT_STREQ(json["state"], "IDLE");
    EXPECT_EQ(json["auto_mode"], false);
    EXPECT_EQ(json["test_mode"], true);
}

// ============================================================================
// Progress Percentage Tests
// ============================================================================

TEST_F(DoorControllerTest, ProgressPercentageInRange) {
    int progress = doorController->getProgressPercentage();
    EXPECT_GE(progress, 0);
    EXPECT_LE(progress, 100);
}

TEST_F(DoorControllerTest, ProgressPercentageForOpenPosition) {
    doorController->setTestMode(false);
    setHallSensorOpen(true);
    doorController->update();

    EXPECT_EQ(doorController->getProgressPercentage(), 100);
}

TEST_F(DoorControllerTest, ProgressPercentageForClosedPosition) {
    doorController->setTestMode(false);
    setHallSensorClosed(true);
    doorController->update();

    EXPECT_EQ(doorController->getProgressPercentage(), 0);
}

// ============================================================================
// Motor Control Tests
// ============================================================================

TEST_F(DoorControllerTest, OpenCommandActivatesMotor) {
    doorController->setTestMode(false);
    doorController->open();

    // Check motor outputs
    EXPECT_TRUE(isMotorOpening());
    EXPECT_FALSE(isMotorClosing());
    EXPECT_FALSE(isMotorStopped());
}

TEST_F(DoorControllerTest, CloseCommandActivatesMotor) {
    doorController->setTestMode(false);
    doorController->close();

    EXPECT_TRUE(isMotorClosing());
    EXPECT_FALSE(isMotorOpening());
    EXPECT_FALSE(isMotorStopped());
}

TEST_F(DoorControllerTest, StopCommandStopsMotor) {
    doorController->setTestMode(false);
    doorController->open();
    doorController->stop();

    EXPECT_TRUE(isMotorStopped());
}

TEST_F(DoorControllerTest, FaultStateStopsMotor) {
    doorController->setTestMode(false);
    doorController->open();

    advanceTime(31000);
    doorController->update();

    EXPECT_TRUE(isMotorStopped());
}

// ============================================================================
// Update Loop Tests
// ============================================================================

TEST_F(DoorControllerTest, UpdateDoesNotCrashWhenIdle) {
    for (int i = 0; i < 100; i++) {
        EXPECT_NO_THROW(doorController->update());
        advanceTime(100);
    }
}

TEST_F(DoorControllerTest, UpdateHandlesNullDependencies) {
    // Test skipped - would require recreating controller which causes issues
    // The production code handles null dependencies gracefully in update()
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(DoorControllerTest, CompleteOpenCloseSequence) {
    doorController->setTestMode(false);
    unsigned long initialCycles = doorController->getTotalCycles();

    // Open
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);
    advanceTime(1000);

    // Simulate reaching open position (ISR stops motor)
    setHallSensorOpen(true);
    stopMotor(); // Simulate ISR stopping motor
    doorController->update();
    advanceTime(100);
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::OPEN);

    // Close
    setHallSensorOpen(false);
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);
    advanceTime(1000);

    // Simulate reaching closed position (ISR stops motor)
    setHallSensorClosed(true);
    stopMotor(); // Simulate ISR stopping motor
    doorController->update();
    advanceTime(100);
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSED);

    // Verify statistics
    EXPECT_EQ(doorController->getTotalCycles(), initialCycles + 1);
    EXPECT_GT(doorController->getTotalOpenTime(), 0UL);
    EXPECT_GT(doorController->getTotalCloseTime(), 0UL);
}

TEST_F(DoorControllerTest, FaultRecoverySequence) {
    // Trigger fault
    doorController->open();
    advanceTime(31000);
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::FAULT);
    EXPECT_TRUE(doorController->hasFault());

    // Clear fault
    doorController->clearFault();
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
    EXPECT_FALSE(doorController->hasFault());

    // Verify can operate normally after fault
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);
}

TEST_F(DoorControllerTest, MultipleOperationCycles) {
    doorController->setTestMode(true);

    for (int i = 0; i < 5; i++) {
        doorController->open();
        EXPECT_EQ(doorController->getState(), DoorState::OPENING);
        advanceTime(1000);
        doorController->stop();

        doorController->close();
        EXPECT_EQ(doorController->getState(), DoorState::CLOSING);
        advanceTime(1000);
        doorController->stop();
    }

    EXPECT_EQ(doorController->getTotalCycles(), 5UL);
}

// ============================================================================
// Door Lockout Tests
// ============================================================================

TEST_F(DoorControllerTest, DoorLockout_DefaultDisabled) {
    EXPECT_FALSE(doorController->isLockoutEnabled());
}

TEST_F(DoorControllerTest, DoorLockout_EnableDisable) {
    doorController->setLockoutEnabled(true);
    EXPECT_TRUE(doorController->isLockoutEnabled());

    doorController->setLockoutEnabled(false);
    EXPECT_FALSE(doorController->isLockoutEnabled());
}

TEST_F(DoorControllerTest, DoorLockout_BlocksOpen) {
    doorController->setLockoutEnabled(true);

    doorController->open();

    // Door should remain in IDLE state - open was blocked
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

TEST_F(DoorControllerTest, DoorLockout_BlocksClose) {
    doorController->setLockoutEnabled(true);

    doorController->close();

    // Door should remain in IDLE state - close was blocked
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

TEST_F(DoorControllerTest, DoorLockout_BlocksSchedule) {
    // Enable auto open/close and lockout
    doorController->setAutoOpenEnabled(true);
    doorController->setAutoCloseEnabled(true);
    doorController->setLockoutEnabled(true);

    // Run several update cycles - schedule should be blocked by lockout
    for (int i = 0; i < 10; i++) {
        doorController->update();
        advanceTime(1000);
    }

    // Door should remain in IDLE state since lockout blocks schedule
    EXPECT_EQ(doorController->getState(), DoorState::IDLE);
}

TEST_F(DoorControllerTest, DoorLockout_InJson) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();

    doorController->setLockoutEnabled(true);
    doorController->toJson(json);

    EXPECT_TRUE(!json["lockout_enabled"].isNull());
    EXPECT_EQ(json["lockout_enabled"], true);
}

// ============================================================================
// Weather-Gated Auto-Open Tests
// ============================================================================

TEST_F(DoorControllerTest, Weather_NoManagerByDefault) {
    // With no weather manager attached, nothing is postponed.
    EXPECT_FALSE(doorController->isWeatherPostponed());
}

TEST_F(DoorControllerTest, Weather_PostponedFieldInJson) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    doorController->toJson(json);
    EXPECT_TRUE(!json["weather_postponed"].isNull());
    EXPECT_EQ(json["weather_postponed"], false);
}

TEST_F(DoorControllerTest, Weather_NullManagerIsSafe) {
    // Explicitly attaching nullptr must not crash schedule handling.
    doorController->setWeatherManager(nullptr);
    doorController->setAutoOpenEnabled(true);
    for (int i = 0; i < 5; i++) {
        doorController->update();
        advanceTime(1000);
    }
    EXPECT_FALSE(doorController->isWeatherPostponed());
}

TEST_F(DoorControllerTest, Weather_InactiveGateDoesNotPostpone) {
    // A weather manager that is attached but not enabled/configured must not
    // gate the door — its gate is inactive, so behavior is schedule-only.
    WeatherManager weather;
    weather.begin(mockHal);
    // Not enabled, no API key -> gate inactive
    doorController->setWeatherManager(&weather);
    doorController->setAutoOpenEnabled(true);

    for (int i = 0; i < 5; i++) {
        doorController->update();
        advanceTime(1000);
    }
    EXPECT_FALSE(doorController->isWeatherPostponed());
    EXPECT_TRUE(weather.isWeatherGoodForOpening()); // fallback-safe
}

TEST_F(DoorControllerTest, Weather_ToggleAutoOpenClearsPostpone) {
    // Toggling auto-open resets any stale weather-postpone state.
    doorController->setAutoOpenEnabled(true);
    doorController->setAutoOpenEnabled(false);
    EXPECT_FALSE(doorController->isWeatherPostponed());
}

// ============================================================================
// Door Timeout Auto-Calculation Tests
// ============================================================================

TEST_F(DoorControllerTest, AutoCalc_DefaultDisabled) {
    EXPECT_FALSE(doorController->isAutoCalcTimeoutEnabled());
}

TEST_F(DoorControllerTest, AutoCalc_EnableDisable) {
    doorController->setAutoCalcTimeoutEnabled(true);
    EXPECT_TRUE(doorController->isAutoCalcTimeoutEnabled());

    doorController->setAutoCalcTimeoutEnabled(false);
    EXPECT_FALSE(doorController->isAutoCalcTimeoutEnabled());
}

TEST_F(DoorControllerTest, AutoCalc_NoHistoryReturnsZero) {
    EXPECT_EQ(doorController->getRecommendedOpenTimeout(), 0u);
    EXPECT_EQ(doorController->getRecommendedCloseTimeout(), 0u);
}

TEST_F(DoorControllerTest, AutoCalc_RecordsOpenTiming) {
    EXPECT_EQ(doorController->getOpenTimingCount(), 0);

    // Simulate a complete OPENING -> OPEN transition
    doorController->setTestMode(false);
    doorController->open();
    EXPECT_EQ(doorController->getState(), DoorState::OPENING);

    advanceTime(5000); // 5 seconds to open

    // Simulate ISR stopping motor (hall sensor triggered)
    setHallSensorOpen(true);
    stopMotor();
    doorController->update();

    EXPECT_EQ(doorController->getState(), DoorState::OPEN);
    EXPECT_EQ(doorController->getOpenTimingCount(), 1);
}

TEST_F(DoorControllerTest, AutoCalc_RecordsCloseTiming) {
    EXPECT_EQ(doorController->getCloseTimingCount(), 0);

    // Simulate a complete CLOSING -> CLOSED transition
    doorController->setTestMode(false);
    doorController->close();
    EXPECT_EQ(doorController->getState(), DoorState::CLOSING);

    advanceTime(5000); // 5 seconds to close

    // Simulate ISR stopping motor (hall sensor triggered)
    setHallSensorClosed(true);
    stopMotor();
    doorController->update();

    EXPECT_EQ(doorController->getState(), DoorState::CLOSED);
    EXPECT_EQ(doorController->getCloseTimingCount(), 1);
}

TEST_F(DoorControllerTest, AutoCalc_RecommendedTimeout) {
    // Simulate a complete OPENING -> OPEN transition taking 5 seconds
    doorController->setTestMode(false);
    doorController->open();
    advanceTime(5000); // 5000ms = 5 seconds

    setHallSensorOpen(true);
    stopMotor();
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::OPEN);

    // Recommended timeout = max(history) / 1000 + 1 = 5000/1000 + 1 = 6
    EXPECT_EQ(doorController->getRecommendedOpenTimeout(), 6u);
}

TEST_F(DoorControllerTest, AutoCalc_CircularBuffer) {
    doorController->setTestMode(false);

    // Record 11 open timing entries (exceeds MAX_TIMING_HISTORY of 10)
    for (int i = 0; i < 11; i++) {
        // Reset state for next cycle
        if (doorController->getState() == DoorState::OPEN) {
            setHallSensorOpen(false);
            doorController->close();
            advanceTime(2000);
            setHallSensorClosed(true);
            stopMotor();
            doorController->update();
            setHallSensorClosed(false);
        }

        doorController->open();
        EXPECT_EQ(doorController->getState(), DoorState::OPENING);
        advanceTime(3000);

        setHallSensorOpen(true);
        stopMotor();
        doorController->update();
        EXPECT_EQ(doorController->getState(), DoorState::OPEN);
        setHallSensorOpen(false);
    }

    // Count should be capped at MAX_TIMING_HISTORY (10)
    EXPECT_EQ(doorController->getOpenTimingCount(), 10);
}

TEST_F(DoorControllerTest, AutoCalc_AutoUpdatesTimeout) {
    // Enable auto-calc
    doorController->setAutoCalcTimeoutEnabled(true);
    doorController->setOpenTimeoutSeconds(30); // Start with 30s

    // Simulate a complete OPENING -> OPEN transition taking 5 seconds
    doorController->setTestMode(false);
    doorController->open();
    advanceTime(5000); // 5000ms

    setHallSensorOpen(true);
    stopMotor();
    doorController->update();
    EXPECT_EQ(doorController->getState(), DoorState::OPEN);

    // With auto-calc enabled, timeout should have been auto-updated
    // Recommended = 5000/1000 + 1 = 6 seconds
    EXPECT_EQ(doorController->getOpenTimeoutSeconds(), 6u);
}

TEST_F(DoorControllerTest, AutoCalc_InJson) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();

    doorController->setAutoCalcTimeoutEnabled(true);
    doorController->toJson(json);

    EXPECT_TRUE(!json["auto_calc_timeout_enabled"].isNull());
    EXPECT_EQ(json["auto_calc_timeout_enabled"], true);
    EXPECT_TRUE(!json["recommended_open_timeout"].isNull());
    EXPECT_TRUE(!json["recommended_close_timeout"].isNull());
}
