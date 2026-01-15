#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "BuzzerController.h"

using namespace fakeit;

class BuzzerControllerTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    BuzzerController buzzer;

    void SetUp() override {
        mockHal = new MockHAL();

        // Must set up logging, which uses micros, for it to work in buzzer tests
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);

        // Mock ArduinoFake functions used by BuzzerController
        When(Method(ArduinoFake(), millis)).AlwaysReturn(1000000);
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
        When(Method(ArduinoFake(), tone)).AlwaysReturn();
        When(Method(ArduinoFake(), noTone)).AlwaysReturn();

        // Set up buzzer
        buzzer.begin(5); // Using pin 5 for testing
        buzzer.setEnabled(true);
    }

    void TearDown() override {
        // Clean up mock
        delete mockHal;
        mockHal = nullptr;
    }
};

TEST_F(BuzzerControllerTest, DisabledAlertDoesNotActivate) {
    buzzer.setEnabled(false);
    buzzer.triggerAlert(AlertType::SENSOR_ERROR);

    EXPECT_FALSE(buzzer.hasActiveAlert());
}

TEST_F(BuzzerControllerTest, LowerPriorityAlertIsBlocked) {
    buzzer.triggerAlert(AlertType::PUMP_ERROR);           // higher priority (lower enum value)
    buzzer.triggerAlert(AlertType::SYSTEM_ERROR);         // lower priority (higher enum value)

    EXPECT_TRUE(buzzer.hasActiveAlert());
    EXPECT_EQ(AlertType::PUMP_ERROR, buzzer.getCurrentAlertType());
}

TEST_F(BuzzerControllerTest, ClearAlertResetsState) {
    buzzer.triggerAlert(AlertType::SENSOR_ERROR);
    buzzer.clearAlert(AlertType::SENSOR_ERROR);

    EXPECT_FALSE(buzzer.hasActiveAlert());
}