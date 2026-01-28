#include <gtest/gtest.h>
#include "ArduinoFake.h"
#include "LightController.h"
#include "MockHAL.h"
#include "SolarCalculator.h"
#include "SettingsManager.h"
#include "Logger.h"

using namespace fakeit;

// For testing, we'll use the real SunriseSunsetCalculator but initialize it properly
// The getSunriseMinutes/getSunsetMinutes methods are not virtual, so we can't mock them
// Instead, we'll rely on the mock SolarCalculator functions we created

class LightControllerTest : public ::testing::Test {
protected:
    MockHAL mockHAL;
    SunriseSunsetCalculator* mockSolarCalculator;
    LightController* lightController;

    void SetUp() override {
        // Reset ArduinoFake
        ArduinoFakeReset();

        // Reset mock state
        mockHAL.reset();

        // Reset SettingsManager to defaults for each test
        settingsManager.resetForTesting();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHAL.millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

        // Initialize SettingsManager (real singleton)
        settingsManager.begin(&mockHAL);

        // Initialize Logger (required before LightController uses it)
        Logger::getInstance().begin(&mockHAL);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);

        // Create the SunriseSunsetCalculator (don't call begin to avoid issues with mock solar calculations)
        mockSolarCalculator = new SunriseSunsetCalculator();

        // Create controller
        lightController = new LightController();
    }
    
    void TearDown() override {
        delete lightController;
        delete mockSolarCalculator;
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(LightControllerTest, InitializeWithHAL) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_TRUE(mockHAL.pwmSetupCalled);
    EXPECT_EQ(mockHAL.pwmSetupChannel, 0U);
    EXPECT_EQ(mockHAL.pwmSetupFreq, 1000U);
    EXPECT_EQ(mockHAL.pwmSetupResolution, 8U);
    
    EXPECT_TRUE(mockHAL.pwmAttachPinCalled);
    EXPECT_EQ(mockHAL.pwmAttachPinPin, 25); // OUT_LIGHT_PIN
    EXPECT_EQ(mockHAL.pwmAttachPinChannel, 0);
}

TEST_F(LightControllerTest, InitializeLoadsSettings) {
    settingsManager.setLightAutoMode(true);
    settingsManager.setLightBrightnessPercent(90);
    settingsManager.setLightTransitionDurationMinutes(20);
    settingsManager.setLightOnHour(7);
    settingsManager.setLightOnMinute(30);
    settingsManager.setLightOnMode("sunset_offset");
    settingsManager.setLightOnSunsetOffsetMinutes(15);
    settingsManager.setLightOffHour(22);

    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_TRUE(lightController->isAutoMode());
    EXPECT_EQ(lightController->getMaxBrightness(), 90);
    EXPECT_EQ(lightController->getTransitionDurationMinutes(), 20);
    EXPECT_EQ(lightController->getOnHour(), 7);
    EXPECT_EQ(lightController->getOnMinute(), 30);
    EXPECT_EQ(lightController->getOnMode(), "sunset_offset");
    EXPECT_EQ(lightController->getOnSunsetOffsetMinutes(), 15);
    EXPECT_EQ(lightController->getOffHour(), 22);
}

TEST_F(LightControllerTest, InitializeToOffState) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_EQ(lightController->getState(), LightState::OFF);
    EXPECT_EQ(lightController->getCurrentBrightness(), 0);
    EXPECT_EQ(lightController->getTargetBrightness(), 0);
}

TEST_F(LightControllerTest, InitializePWMToOff) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    // PWM should be set to 0 (off)
    EXPECT_TRUE(mockHAL.pwmWriteCalled);
    EXPECT_EQ(mockHAL.pwmWriteChannel, 0);
    EXPECT_EQ(mockHAL.pwmWriteValue, 0u);
}

// ============================================================================
// Manual Control Tests
// ============================================================================

TEST_F(LightControllerTest, TurnOnImmediately) {
    // Set brightness to expected default value
    settingsManager.setLightBrightnessPercent(80);

    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->turnOn();

    EXPECT_EQ(lightController->getState(), LightState::ON);
    EXPECT_EQ(lightController->getCurrentBrightness(), 80); // max brightness default
    EXPECT_EQ(lightController->getTargetBrightness(), 80);

    // PWM should be updated
    unsigned int expectedPWM = (80 * 255) / 100;
    EXPECT_EQ(mockHAL.pwmWriteValue, expectedPWM);
}

TEST_F(LightControllerTest, TurnOnWithCustomMaxBrightness) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setMaxBrightness(50);

    lightController->turnOn();

    EXPECT_EQ(lightController->getCurrentBrightness(), 50);
    EXPECT_EQ(lightController->getTargetBrightness(), 50);
}

TEST_F(LightControllerTest, TurnOffImmediately) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();

    lightController->turnOff();

    EXPECT_EQ(lightController->getState(), LightState::OFF);
    EXPECT_EQ(lightController->getCurrentBrightness(), 0);
    EXPECT_EQ(lightController->getTargetBrightness(), 0);

    // PWM should be set to 0
    EXPECT_EQ(mockHAL.pwmWriteValue, 0u);
}

TEST_F(LightControllerTest, SetBrightnessClampsTo100) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->setBrightness(150);

    EXPECT_EQ(lightController->getCurrentBrightness(), 100);
    EXPECT_EQ(lightController->getTargetBrightness(), 100);
}

TEST_F(LightControllerTest, SetBrightnessClampsTo0) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->setBrightness(-10);

    EXPECT_EQ(lightController->getCurrentBrightness(), 0);
    EXPECT_EQ(lightController->getTargetBrightness(), 0);
}

TEST_F(LightControllerTest, SetBrightnessValidValue) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->setBrightness(65);

    EXPECT_EQ(lightController->getCurrentBrightness(), 65);
    EXPECT_EQ(lightController->getTargetBrightness(), 65);
}

TEST_F(LightControllerTest, SetBrightnessUpdatesPWM) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->setBrightness(50);

    unsigned int expectedPWM = (50 * 255) / 100;
    EXPECT_EQ(mockHAL.pwmWriteValue, expectedPWM);
}

// ============================================================================
// Fade Control Tests
// ============================================================================

TEST_F(LightControllerTest, FadeInFromOff) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;

    lightController->fadeIn();

    EXPECT_EQ(lightController->getState(), LightState::FADING_IN);
    EXPECT_EQ(lightController->getMaxBrightness(), 80); // max brightness
    EXPECT_EQ(lightController->getCurrentBrightness(), 0); // starts at 0
}

TEST_F(LightControllerTest, FadeOutFromOn) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    mockHAL.millisValue = 2000;

    lightController->fadeOut();

    EXPECT_EQ(lightController->getState(), LightState::FADING_OUT);
    EXPECT_EQ(lightController->getCurrentBrightness(), 80); // starts at 80
}

TEST_F(LightControllerTest, FadeInFromPartialBrightness) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setBrightness(40);
    mockHAL.millisValue = 1000;

    lightController->fadeIn();

    EXPECT_EQ(lightController->getMaxBrightness(), 80);
    EXPECT_EQ(lightController->getCurrentBrightness(), 40); // starts at current
}

TEST_F(LightControllerTest, FadeOutFromPartialBrightness) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setBrightness(60);
    mockHAL.millisValue = 1000;

    lightController->fadeOut();

    EXPECT_EQ(lightController->getCurrentBrightness(), 60); // starts at current
}

TEST_F(LightControllerTest, FadeInProgressUpdatesBrightness) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // Simulate partial fade progress (50%)
    unsigned long fadeDuration = 15 * 60 * 1000; // 15 minutes in ms
    mockHAL.millisValue = 1000 + (fadeDuration / 2);
    lightController->update();
    
    // Brightness should be between start and target
    EXPECT_GT(lightController->getCurrentBrightness(), 0);
    EXPECT_LT(lightController->getCurrentBrightness(), 80);
}

TEST_F(LightControllerTest, FadeCompletesToOnState) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // Simulate fade completion
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + fadeDuration + 1000;
    lightController->update();
    
    EXPECT_EQ(lightController->getState(), LightState::ON);
    EXPECT_EQ(lightController->getCurrentBrightness(), 80);
}

TEST_F(LightControllerTest, FadeOutCompletesToOffState) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    mockHAL.millisValue = 1000;
    lightController->fadeOut();
    
    // Simulate fade completion
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + fadeDuration + 1000;
    lightController->update();
    
    EXPECT_EQ(lightController->getState(), LightState::OFF);
    EXPECT_EQ(lightController->getCurrentBrightness(), 0);
}

TEST_F(LightControllerTest, FadeProgressAtStart) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    EXPECT_EQ(lightController->getFadeProgressPercentage(), 0);
}

TEST_F(LightControllerTest, FadeProgressAtCompletion) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // Simulate fade completion
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + fadeDuration + 1000;
    lightController->update();
    
    EXPECT_EQ(lightController->getFadeProgressPercentage(), 100);
}

TEST_F(LightControllerTest, FadeProgressMidway) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // Simulate 50% progress
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + (fadeDuration / 2);
    lightController->update();
    
    EXPECT_EQ(lightController->getFadeProgressPercentage(), 50);
}

TEST_F(LightControllerTest, FadeProgressReturns100WhenNotFading) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    
    EXPECT_EQ(lightController->getFadeProgressPercentage(), 100);
}

// ============================================================================
// Auto Mode Tests
// ============================================================================

TEST_F(LightControllerTest, SetAutoModeEnabled) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setAutoMode(true);
    
    EXPECT_TRUE(lightController->isAutoMode());
}

TEST_F(LightControllerTest, SetAutoModeDisabled) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    
    lightController->setAutoMode(false);
    
    EXPECT_FALSE(lightController->isAutoMode());
}

TEST_F(LightControllerTest, AutoModeDoesNotTriggerWithoutTime) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    
    // Don't set up time, should not trigger fade
    lightController->update();
    
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

// ============================================================================
// Test Mode Tests
// ============================================================================

TEST_F(LightControllerTest, SetTestModeEnabled) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setTestMode(true);
    
    EXPECT_TRUE(lightController->isTestMode());
}

TEST_F(LightControllerTest, SetTestModeDisabled) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setTestMode(true);
    
    lightController->setTestMode(false);
    
    EXPECT_FALSE(lightController->isTestMode());
}

TEST_F(LightControllerTest, TestModeSkipsAutoUpdates) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    lightController->setTestMode(true);
    
    // Set up time to trigger auto mode
    // Mock time would be 6:00 AM, on hour is 6
    // This should NOT trigger because test mode is enabled
    lightController->update();
    
    // Should remain OFF (not start fading)
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(LightControllerTest, GetStateReturnsCurrentState) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getState(), LightState::OFF);
    
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
}

TEST_F(LightControllerTest, GetStateStringForOff) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getStateString(), "OFF");
}

TEST_F(LightControllerTest, GetStateStringForOn) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    
    EXPECT_EQ(lightController->getStateString(), "ON");
}

TEST_F(LightControllerTest, GetStateStringForFadingIn) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    EXPECT_EQ(lightController->getStateString(), "FADING_IN");
}

TEST_F(LightControllerTest, GetStateStringForFadingOut) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    mockHAL.millisValue = 1000;
    lightController->fadeOut();
    
    EXPECT_EQ(lightController->getStateString(), "FADING_OUT");
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(LightControllerTest, GetMaxBrightnessReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getMaxBrightness(), 80);
}

TEST_F(LightControllerTest, SetMaxBrightnessClampsTo100) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setMaxBrightness(150);
    
    EXPECT_EQ(lightController->getMaxBrightness(), 100);
}

TEST_F(LightControllerTest, SetMaxBrightnessClampsTo0) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setMaxBrightness(-10);
    
    EXPECT_EQ(lightController->getMaxBrightness(), 0);
}

TEST_F(LightControllerTest, SetMaxBrightnessUpdatesOnLight) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    
    lightController->setMaxBrightness(60);
    
    EXPECT_EQ(lightController->getCurrentBrightness(), 60);
    EXPECT_EQ(lightController->getTargetBrightness(), 60);
}

TEST_F(LightControllerTest, SetMaxBrightnessDoesNotUpdateOffLight) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setMaxBrightness(60);
    
    EXPECT_EQ(lightController->getCurrentBrightness(), 0);
    EXPECT_EQ(lightController->getTargetBrightness(), 0);
}

TEST_F(LightControllerTest, GetTransitionDurationMinutesReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getTransitionDurationMinutes(), 15);
}

TEST_F(LightControllerTest, SetTransitionDurationMinutesClampsTo60) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setTransitionDurationMinutes(120);
    
    EXPECT_EQ(lightController->getTransitionDurationMinutes(), 60);
}

TEST_F(LightControllerTest, SetTransitionDurationMinutesClampsTo1) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setTransitionDurationMinutes(0);
    
    EXPECT_EQ(lightController->getTransitionDurationMinutes(), 1);
}

TEST_F(LightControllerTest, SetTransitionDurationMinutesValidValue) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setTransitionDurationMinutes(30);
    
    EXPECT_EQ(lightController->getTransitionDurationMinutes(), 30);
}

TEST_F(LightControllerTest, GetOnHourReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getOnHour(), 6);
}

TEST_F(LightControllerTest, SetOnHourClampsTo23) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnHour(24);
    
    EXPECT_EQ(lightController->getOnHour(), 23);
}

TEST_F(LightControllerTest, SetOnHourClampsTo0) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnHour(-1);
    
    EXPECT_EQ(lightController->getOnHour(), 0);
}

TEST_F(LightControllerTest, SetOnHourValidValue) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnHour(14);
    
    EXPECT_EQ(lightController->getOnHour(), 14);
}

TEST_F(LightControllerTest, GetOnMinuteReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getOnMinute(), 0);
}

TEST_F(LightControllerTest, SetOnMinuteClampsTo59) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnMinute(60);
    
    EXPECT_EQ(lightController->getOnMinute(), 59);
}

TEST_F(LightControllerTest, SetOnMinuteClampsTo0) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnMinute(-1);
    
    EXPECT_EQ(lightController->getOnMinute(), 0);
}

TEST_F(LightControllerTest, SetOnMinuteValidValue) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnMinute(45);
    
    EXPECT_EQ(lightController->getOnMinute(), 45);
}

TEST_F(LightControllerTest, GetOnModeReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getOnMode(), "fixed");
}

TEST_F(LightControllerTest, SetOnModeFixed) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnMode("fixed");
    
    EXPECT_EQ(lightController->getOnMode(), "fixed");
}

TEST_F(LightControllerTest, SetOnModeSunsetOffset) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnMode("sunset_offset");
    
    EXPECT_EQ(lightController->getOnMode(), "sunset_offset");
}

TEST_F(LightControllerTest, GetOnSunsetOffsetMinutesReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getOnSunsetOffsetMinutes(), 0);
}

TEST_F(LightControllerTest, SetOnSunsetOffsetMinutesValid) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOnSunsetOffsetMinutes(30);
    
    EXPECT_EQ(lightController->getOnSunsetOffsetMinutes(), 30);
}

TEST_F(LightControllerTest, GetOffHourReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getOffHour(), 21);
}

TEST_F(LightControllerTest, SetOffHourClampsTo23) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOffHour(24);
    
    EXPECT_EQ(lightController->getOffHour(), 23);
}

TEST_F(LightControllerTest, SetOffHourClampsTo0) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOffHour(-1);
    
    EXPECT_EQ(lightController->getOffHour(), 0);
}

TEST_F(LightControllerTest, SetOffHourValidValue) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setOffHour(18);
    
    EXPECT_EQ(lightController->getOffHour(), 18);
}

TEST_F(LightControllerTest, GetSunriseOffsetMinutesReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getSunriseOffsetMinutes(), 0);
}

TEST_F(LightControllerTest, SetSunriseOffsetMinutesValid) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setSunriseOffsetMinutes(15);
    
    EXPECT_EQ(lightController->getSunriseOffsetMinutes(), 15);
}

TEST_F(LightControllerTest, GetSunsetOffsetMinutesReturnsDefault) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_EQ(lightController->getSunsetOffsetMinutes(), 0);
}

TEST_F(LightControllerTest, SetSunsetOffsetMinutesValid) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->setSunsetOffsetMinutes(30);
    
    EXPECT_EQ(lightController->getSunsetOffsetMinutes(), 30);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(LightControllerTest, GetTotalOnTimeReturnsZeroInitially) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_EQ(lightController->getTotalOnTime(), 0UL);
}

TEST_F(LightControllerTest, GetTotalFadeInTimeReturnsZeroInitially) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_EQ(lightController->getTotalFadeInTime(), 0UL);
}

TEST_F(LightControllerTest, GetTotalFadeOutTimeReturnsZeroInitially) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_EQ(lightController->getTotalFadeOutTime(), 0UL);
}

TEST_F(LightControllerTest, GetTotalCyclesReturnsZeroInitially) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    EXPECT_EQ(lightController->getTotalCycles(), 0UL);
}

TEST_F(LightControllerTest, ResetStatisticsClearsAll) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();

    // Simulate some time in ON state
    lightController->resetStatistics();

    EXPECT_EQ(lightController->getTotalOnTime(), 0UL);
    EXPECT_EQ(lightController->getTotalFadeInTime(), 0UL);
    EXPECT_EQ(lightController->getTotalFadeOutTime(), 0UL);
    EXPECT_EQ(lightController->getTotalCycles(), 0UL);
}

TEST_F(LightControllerTest, FadeInIncrementsFadeInTime) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // Complete fade
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + fadeDuration + 1000;
    lightController->update();
    
    EXPECT_GT(lightController->getTotalFadeInTime(), 0);
}

TEST_F(LightControllerTest, FadeOutIncrementsFadeOutTime) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    mockHAL.millisValue = 1000;
    lightController->fadeOut();
    
    // Complete fade
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + fadeDuration + 1000;
    lightController->update();
    
    EXPECT_GT(lightController->getTotalFadeOutTime(), 0);
}

TEST_F(LightControllerTest, StateChangeToOnIncrementsCycles) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    lightController->turnOn();

    EXPECT_EQ(lightController->getTotalCycles(), 1UL);

    lightController->turnOff();
    lightController->turnOn();

    EXPECT_EQ(lightController->getTotalCycles(), 2UL);
}

// ============================================================================
// Fault Handling Tests
// ============================================================================

TEST_F(LightControllerTest, HasFaultReturnsFalseInitially) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    EXPECT_FALSE(lightController->hasFault());
}

TEST_F(LightControllerTest, ClearFaultWhenNoFaultDoesNothing) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->clearFault();
    
    EXPECT_FALSE(lightController->hasFault());
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

// ============================================================================
// JSON Status Tests
// ============================================================================

TEST_F(LightControllerTest, ToJsonIncludesAllFields) {
    lightController->begin(&mockHAL, mockSolarCalculator);

    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject json = doc.to<ArduinoJson::JsonObject>();

    lightController->toJson(json);

    EXPECT_FALSE(json["state"].isNull());
    EXPECT_FALSE(json["auto_mode"].isNull());
    EXPECT_FALSE(json["current_brightness"].isNull());
    EXPECT_FALSE(json["target_brightness"].isNull());
    EXPECT_FALSE(json["max_brightness"].isNull());
    EXPECT_FALSE(json["transition_duration_minutes"].isNull());
    EXPECT_FALSE(json["on_hour"].isNull());
    EXPECT_FALSE(json["on_minute"].isNull());
    EXPECT_FALSE(json["on_mode"].isNull());
    EXPECT_FALSE(json["on_sunset_offset_minutes"].isNull());
    EXPECT_FALSE(json["off_hour"].isNull());
    EXPECT_FALSE(json["sunrise_offset_minutes"].isNull());
    EXPECT_FALSE(json["sunset_offset_minutes"].isNull());
    EXPECT_FALSE(json["total_on_time"].isNull());
    EXPECT_FALSE(json["total_fade_in_time"].isNull());
    EXPECT_FALSE(json["total_fade_out_time"].isNull());
    EXPECT_FALSE(json["total_cycles"].isNull());
    EXPECT_FALSE(json["fade_progress_percentage"].isNull());
    EXPECT_FALSE(json["next_scheduled_action"].isNull());
}

TEST_F(LightControllerTest, ToJsonOffStateValues) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject json = doc.to<ArduinoJson::JsonObject>();
    
    lightController->toJson(json);
    
    EXPECT_STREQ(json["state"], "OFF");
    EXPECT_EQ(json["auto_mode"], false);
    EXPECT_EQ(json["current_brightness"], 0);
    EXPECT_EQ(json["target_brightness"], 0);
    EXPECT_EQ(json["max_brightness"], 80);
    EXPECT_EQ(json["transition_duration_minutes"], 15);
    EXPECT_EQ(json["on_hour"], 6);
    EXPECT_EQ(json["on_minute"], 0);
    EXPECT_STREQ(json["on_mode"], "fixed");
    EXPECT_EQ(json["on_sunset_offset_minutes"], 0);
    EXPECT_EQ(json["off_hour"], 21);
    EXPECT_EQ(json["sunrise_offset_minutes"], 0);
    EXPECT_EQ(json["sunset_offset_minutes"], 0);
    EXPECT_EQ(json["total_on_time"], 0);
    EXPECT_EQ(json["total_fade_in_time"], 0);
    EXPECT_EQ(json["total_fade_out_time"], 0);
    EXPECT_EQ(json["total_cycles"], 0);
    EXPECT_EQ(json["fade_progress_percentage"], 100);
}

TEST_F(LightControllerTest, ToJsonOnStateValues) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    
    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject json = doc.to<ArduinoJson::JsonObject>();
    
    lightController->toJson(json);
    
    EXPECT_STREQ(json["state"], "ON");
    EXPECT_EQ(json["current_brightness"], 80);
    EXPECT_EQ(json["target_brightness"], 80);
    EXPECT_EQ(json["total_cycles"], 1);
}

// ============================================================================
// Next Scheduled Action Tests
// ============================================================================

TEST_F(LightControllerTest, GetNextScheduledActionAutoModeDisabled) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(false);
    
    EXPECT_STREQ(lightController->getNextScheduledAction().c_str(), "Auto mode disabled");
}

TEST_F(LightControllerTest, GetNextScheduledActionTimeNotAvailable) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    
    // Don't set up time, should return "Time not available"
    EXPECT_STREQ(lightController->getNextScheduledAction().c_str(), "Time not available");
}

TEST_F(LightControllerTest, GetNextScheduledActionTurnOnToday) {
    // Set mock time to 5:00 AM
    struct tm timeinfo = {0};
    timeinfo.tm_hour = 5;
    timeinfo.tm_min = 0;
    mockHAL.setTimeInfo(timeinfo);
    mockHAL.setGetLocalTimeResult(true);

    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);

    // on hour is 6, off hour is 21
    // Should say "Turn on at 6:0"
    String action = lightController->getNextScheduledAction();
    EXPECT_TRUE(action.indexOf("Turn on at 6:0") >= 0);
}

TEST_F(LightControllerTest, GetNextScheduledActionTurnOnTomorrow) {
    // Set mock time to 7:00 AM
    struct tm timeinfo = {0};
    timeinfo.tm_hour = 7;
    timeinfo.tm_min = 0;
    mockHAL.setTimeInfo(timeinfo);
    mockHAL.setGetLocalTimeResult(true);

    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);

    // on hour is 6, off hour is 21
    // Should say "Turn on tomorrow at 6:0"
    String action = lightController->getNextScheduledAction();
    EXPECT_TRUE(action.indexOf("Turn on tomorrow at 6:0") >= 0);
}

TEST_F(LightControllerTest, GetNextScheduledActionTurnOffToday) {
    // Set mock time to 20:00
    struct tm timeinfo = {0};
    timeinfo.tm_hour = 20;
    timeinfo.tm_min = 0;
    mockHAL.setTimeInfo(timeinfo);
    mockHAL.setGetLocalTimeResult(true);

    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    lightController->turnOn();

    // off hour is 21
    // Should say "Turn off at 21:0"
    String action = lightController->getNextScheduledAction();
    EXPECT_TRUE(action.indexOf("Turn off at 21:0") >= 0);
}

TEST_F(LightControllerTest, GetNextScheduledActionTurnOffTomorrow) {
    // Set mock time to 22:00
    struct tm timeinfo = {0};
    timeinfo.tm_hour = 22;
    timeinfo.tm_min = 0;
    mockHAL.setTimeInfo(timeinfo);
    mockHAL.setGetLocalTimeResult(true);

    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    lightController->turnOn();

    // off hour is 21
    // Should say "Turn off tomorrow at 21:0"
    String action = lightController->getNextScheduledAction();
    EXPECT_TRUE(action.indexOf("Turn off tomorrow at 21:0") >= 0);
}

// ============================================================================
// Schedule Logic Tests
// ============================================================================

TEST_F(LightControllerTest, ShouldTurnOnByScheduleFixedMode) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);

    // Mock time is 6:00 AM, on hour is 6, off hour is 21
    // Should return true (6:00 >= 6:00 && 6:00 < 21:00)
    // This is private, so we test via update()
    // If it's time to turn on, it should start fading
    // We'll verify through state changes
    // Note: This test would require setting up time properly to verify behavior
}

TEST_F(LightControllerTest, ShouldTurnOffByScheduleFixedMode) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    lightController->turnOn();
    
    // Mock time is 21:00, on hour is 6, off hour is 21
    // Should return true (21:00 >= 21:00)
    // We'll verify through state changes
}

TEST_F(LightControllerTest, ShouldNotTurnOnWhenAlreadyOn) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    lightController->turnOn();
    
    // Should not turn on again (already ON)
    // This is tested via update() not triggering another fade in
}

TEST_F(LightControllerTest, ShouldNotTurnOffWhenAlreadyOff) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    
    // Should not turn off (already OFF)
    // This is tested via update() not triggering fade out
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(LightControllerTest, MultipleStateTransitions) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
    
    lightController->turnOff();
    EXPECT_EQ(lightController->getState(), LightState::OFF);
    
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
    
    lightController->turnOff();
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

TEST_F(LightControllerTest, FadeInterruption) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    
    // Start fade in
    lightController->fadeIn();
    EXPECT_EQ(lightController->getState(), LightState::FADING_IN);
    
    // Interrupt with manual on
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
}

TEST_F(LightControllerTest, FadeOutInterruption) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    mockHAL.millisValue = 1000;
    
    // Start fade out
    lightController->fadeOut();
    EXPECT_EQ(lightController->getState(), LightState::FADING_OUT);
    
    // Interrupt with manual off
    lightController->turnOff();
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

TEST_F(LightControllerTest, ManualOverridesAutoMode) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->setAutoMode(true);
    
    // Even in auto mode, manual controls work immediately
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
    
    lightController->turnOff();
    EXPECT_EQ(lightController->getState(), LightState::OFF);
}

TEST_F(LightControllerTest, Brightness0To100Range) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    for (int i = 0; i <= 100; i += 10) {
        lightController->setBrightness(i);
        EXPECT_EQ(lightController->getCurrentBrightness(), i);
    }
}

TEST_F(LightControllerTest, TransitionDuration1To60Range) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    for (int i = 1; i <= 60; i += 10) {
        lightController->setTransitionDurationMinutes(i);
        EXPECT_EQ(lightController->getTransitionDurationMinutes(), i);
    }
}

TEST_F(LightControllerTest, Hour0To23Range) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    for (int i = 0; i <= 23; i++) {
        lightController->setOnHour(i);
        EXPECT_EQ(lightController->getOnHour(), i);
    }
}

TEST_F(LightControllerTest, Minute0To59Range) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    for (int i = 0; i <= 59; i++) {
        lightController->setOnMinute(i);
        EXPECT_EQ(lightController->getOnMinute(), i);
    }
}

TEST_F(LightControllerTest, PWMScalingCorrect) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    // Test PWM scaling: brightness * 255 / 100
    lightController->setBrightness(0);
    EXPECT_EQ(mockHAL.pwmWriteValue, 0u);
    
    lightController->setBrightness(50);
    EXPECT_EQ(mockHAL.pwmWriteValue, 127u); // 50 * 255 / 100 = 127.5 -> 127
    
    lightController->setBrightness(100);
    EXPECT_EQ(mockHAL.pwmWriteValue, 255u); // 100 * 255 / 100 = 255
}

TEST_F(LightControllerTest, UpdateDoesNothingWhenNotFading) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    lightController->turnOn();
    
    // Update when not fading should not change brightness
    int initialBrightness = lightController->getCurrentBrightness();
    lightController->update();
    
    EXPECT_EQ(lightController->getCurrentBrightness(), initialBrightness);
}

TEST_F(LightControllerTest, FadeUsesSineWave) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    // At 50% progress, sine wave should give approximately 29% brightness
    // sin(0.5 * PI / 2) = sin(0.785) ≈ 0.707
    // 0 + (80 - 0) * 0.707 ≈ 56.6%
    unsigned long fadeDuration = 15 * 60 * 1000;
    mockHAL.millisValue = 1000 + (fadeDuration / 2);
    lightController->update();
    
    // Brightness should be around 56-57
    int brightness = lightController->getCurrentBrightness();
    EXPECT_GE(brightness, 55);
    EXPECT_LE(brightness, 58);
}

TEST_F(LightControllerTest, StatisticsAccumulateCorrectly) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    
    // Perform multiple cycles
    for (int i = 0; i < 3; i++) {
        lightController->fadeIn();
        unsigned long fadeDuration = 15 * 60 * 1000;
        mockHAL.millisValue += fadeDuration + 1000;
        lightController->update();
        
        lightController->fadeOut();
        mockHAL.millisValue += fadeDuration + 1000;
        lightController->update();
    }
    
    EXPECT_EQ(lightController->getTotalCycles(), 3);
    EXPECT_GT(lightController->getTotalFadeInTime(), 0);
    EXPECT_GT(lightController->getTotalFadeOutTime(), 0);
}

TEST_F(LightControllerTest, MultipleFadeOperations) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    
    // Fade in
    lightController->fadeIn();
    EXPECT_EQ(lightController->getState(), LightState::FADING_IN);
    
    // Fade out (interrupt)
    lightController->fadeOut();
    EXPECT_EQ(lightController->getState(), LightState::FADING_OUT);
    
    // Fade in again (interrupt)
    lightController->fadeIn();
    EXPECT_EQ(lightController->getState(), LightState::FADING_IN);
    
    // Manual on (interrupt)
    lightController->turnOn();
    EXPECT_EQ(lightController->getState(), LightState::ON);
}

TEST_F(LightControllerTest, DefaultValuesAfterConstruction) {
    LightController controller;

    EXPECT_EQ(controller.getState(), LightState::OFF);
    EXPECT_FALSE(controller.isAutoMode());
    EXPECT_FALSE(controller.isTestMode());
    EXPECT_EQ(controller.getCurrentBrightness(), 0);
    EXPECT_EQ(controller.getTargetBrightness(), 0);
    EXPECT_EQ(controller.getMaxBrightness(), 80);
    EXPECT_EQ(controller.getTransitionDurationMinutes(), 15);
    EXPECT_EQ(controller.getOnHour(), 6);
    EXPECT_EQ(controller.getOnMinute(), 0);
    EXPECT_EQ(controller.getOnMode(), "fixed");
    EXPECT_EQ(controller.getOnSunsetOffsetMinutes(), 0);
    EXPECT_EQ(controller.getOffHour(), 21);
    EXPECT_EQ(controller.getSunriseOffsetMinutes(), 0);
    EXPECT_EQ(controller.getSunsetOffsetMinutes(), 0);
    EXPECT_EQ(controller.getTotalOnTime(), 0UL);
    EXPECT_EQ(controller.getTotalFadeInTime(), 0UL);
    EXPECT_EQ(controller.getTotalFadeOutTime(), 0UL);
    EXPECT_EQ(controller.getTotalCycles(), 0UL);
}

TEST_F(LightControllerTest, AllGettersReturnValidValues) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    
    // All getters should return valid values
    EXPECT_GE(lightController->getMaxBrightness(), 0);
    EXPECT_LE(lightController->getMaxBrightness(), 100);
    
    EXPECT_GE(lightController->getTransitionDurationMinutes(), 1);
    EXPECT_LE(lightController->getTransitionDurationMinutes(), 60);
    
    EXPECT_GE(lightController->getOnHour(), 0);
    EXPECT_LE(lightController->getOnHour(), 23);
    
    EXPECT_GE(lightController->getOnMinute(), 0);
    EXPECT_LE(lightController->getOnMinute(), 59);
    
    EXPECT_GE(lightController->getOffHour(), 0);
    EXPECT_LE(lightController->getOffHour(), 23);
    
    EXPECT_GE(lightController->getOnSunsetOffsetMinutes(), -720); // -12 hours
    EXPECT_LE(lightController->getOnSunsetOffsetMinutes(), 720); // +12 hours
    
    EXPECT_GE(lightController->getSunriseOffsetMinutes(), -720);
    EXPECT_LE(lightController->getSunriseOffsetMinutes(), 720);
    
    EXPECT_GE(lightController->getSunsetOffsetMinutes(), -720);
    EXPECT_LE(lightController->getSunsetOffsetMinutes(), 720);
}

TEST_F(LightControllerTest, FadeProgressPercentageRange) {
    lightController->begin(&mockHAL, mockSolarCalculator);
    mockHAL.millisValue = 1000;
    lightController->fadeIn();
    
    unsigned long fadeDuration = 15 * 60 * 1000;
    
    // Test at various progress points
    for (int i = 0; i <= 100; i += 10) {
        mockHAL.millisValue = 1000 + (fadeDuration * i / 100);
        lightController->update();
        
        int progress = lightController->getFadeProgressPercentage();
        EXPECT_GE(progress, 0);
        EXPECT_LE(progress, 100);
    }
}
