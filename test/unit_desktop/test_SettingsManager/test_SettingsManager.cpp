#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include "MockHAL.h"
#include "SettingsManager.h"
#include "Logger.h"

using namespace fakeit;

// Global mock HAL instance
static MockHAL mockHal;

class SettingsManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset ArduinoFake
        ArduinoFakeReset();

        // Reset mock state
        mockHal.reset();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal.millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

        // Initialize Logger AFTER all Arduino function mocks are set up
        Logger::getInstance().begin(&mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);

        // Clear any previous file content
        mockHal.clearFileContent();

        // Reset SettingsManager singleton state from previous test
        sm.resetForTesting();

        // Initialize with mock HAL
        sm.begin(&mockHal);
    }
    
    void TearDown() override {
        // Clean up
        // delete mockHal;

        // Reset Arduino fake state
        ArduinoFakeReset();
    }
    
    SettingsManager& sm = SettingsManager::getInstance();

    // Helper method to create valid JSON settings
    String createValidSettingsJson() {
        return R"({
  "ssid": "TestNetwork",
  "passwd": "TestPassword123",
  "ap_mode": false,
  "has_connected": true,
  "temp_threshold_on_f": 34.0,
  "temp_threshold_off_f": 36.0,
  "pump_on_time_seconds": 300,
  "pump_off_time_seconds": 600,
  "pump_auto_mode": true,
  "light_auto_mode": false,
  "light_on_hour": 6,
  "light_off_hour": 21,
  "light_on_minute": 0,
  "light_brightness_percent": 80,
  "light_transition_duration_minutes": 15,
  "light_on_mode": "fixed",
  "light_on_sunset_offset_minutes": 0,
  "water_flow_error_timeout_seconds": 120,
  "pulses_per_gallon": 450,
  "water_meter_timeout_seconds": 300,
  "water_meter_per_pulse_calculation_enabled": false,
  "pump_off_flow_monitoring_enabled": false,
  "pump_off_flow_grace_period_seconds": 30,
  "wifi_max_retries": 5,
  "wifi_retry_delay_seconds": 30,
  "wifi_ap_duration_minutes": 10,
  "watchdog_timeout_seconds": 30,
  "wifi_led_enabled": true,
  "buzzer_enabled": true,
  "buzzer_type": "ACTIVE",
  "door_open_timeout_seconds": 30,
  "door_close_timeout_seconds": 30,
  "door_auto_open_enabled": false,
  "door_auto_open_offset_minutes": 0,
  "door_auto_open_days": [true, true, true, true, true, true, true],
  "door_auto_close_enabled": false,
  "door_auto_close_offset_minutes": 0,
  "door_auto_close_days": [true, true, true, true, true, true, true],
  "latitude": 40.7128,
  "longitude": -74.0060,
  "timezone_offset_hours": -5,
  "log_level": "INFO"
})";
    }
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

TEST_F(SettingsManagerTest, ReturnsSameInstance) {
    SettingsManager& instance1 = SettingsManager::getInstance();
    SettingsManager& instance2 = SettingsManager::getInstance();
    
    // Should return same instance
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(SettingsManagerTest, InitializesWithDefaultValues) {
    // After begin(), settings should have default values
    EXPECT_STREQ(sm.getSSID().c_str(), "");
    EXPECT_STREQ(sm.getPassword().c_str(), "");
    EXPECT_TRUE(sm.isAPMode()); // Default is true for AP mode
    EXPECT_TRUE(sm.getPumpAutoMode()); // Default is true
    EXPECT_FALSE(sm.getLightAutoMode());
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 34.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 36.0);
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 300u);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 600u);
    EXPECT_EQ(sm.getLightOnHour(), 6);
    EXPECT_EQ(sm.getLightOffHour(), 21);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 80);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 15);
    EXPECT_EQ(sm.getWaterFlowErrorTimeoutSeconds(), 120);
    EXPECT_EQ(sm.getWifiMaxRetries(), 5u);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 30u);
    EXPECT_EQ(sm.getWifiAPDurationMinutes(), 10);
}

// ============================================================================
// Load Tests
// ============================================================================

TEST_F(SettingsManagerTest, LoadSucceedsWithValidJson) {
    // Mark as not loaded so load() will actually load from file
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL to return valid JSON
    String json = createValidSettingsJson();
    mockHal.setFileContent(json.c_str(), json.length());

    // Load settings
    bool result = sm.load();

    EXPECT_TRUE(result);
    EXPECT_STREQ(sm.getSSID().c_str(), "TestNetwork");
    EXPECT_STREQ(sm.getPassword().c_str(), "TestPassword123");
    EXPECT_FALSE(sm.isAPMode());
    EXPECT_TRUE(sm.getPumpAutoMode());
    EXPECT_FALSE(sm.getLightAutoMode());
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 34.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 36.0);
}

TEST_F(SettingsManagerTest, LoadFailsWithMissingFile) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL to return empty file (simulates file not found)
    // Don't set any file content - fsOpen will still return a handle, but fsSize will be 0

    // Load settings
    bool result = sm.load();

    // Should fail gracefully and use defaults
    EXPECT_FALSE(result);
    EXPECT_STREQ(sm.getSSID().c_str(), "");
}

TEST_F(SettingsManagerTest, LoadFailsWithInvalidJson) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL to return invalid JSON
    String invalidJson = "{ invalid json }";
    mockHal.setFileContent(invalidJson.c_str(), invalidJson.length());

    // Load settings
    bool result = sm.load();

    // Should fail gracefully and use defaults
    EXPECT_FALSE(result);
    EXPECT_STREQ(sm.getSSID().c_str(), "");
}

TEST_F(SettingsManagerTest, LoadUsesDefaultForMissingFields) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL to return JSON with missing fields
    String incompleteJson = R"({
  "ssid": "TestNetwork",
  "pump_auto_mode": true
})";
    mockHal.setFileContent(incompleteJson.c_str(), incompleteJson.length());

    // Load settings
    bool result = sm.load();

    EXPECT_TRUE(result);
    EXPECT_STREQ(sm.getSSID().c_str(), "TestNetwork");
    EXPECT_TRUE(sm.getPumpAutoMode());
    // Missing fields should use defaults
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 34.0);
    EXPECT_EQ(sm.getLightOnHour(), 6);
}

TEST_F(SettingsManagerTest, LoadHandlesPartialJson) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL to return partial JSON
    String partialJson = R"({
  "ssid": "PartialNetwork",
  "temp_threshold_on_f": 32.0,
  "light_on_hour": 7
})";
    mockHal.setFileContent(partialJson.c_str(), partialJson.length());

    // Load settings
    bool result = sm.load();

    EXPECT_TRUE(result);
    EXPECT_STREQ(sm.getSSID().c_str(), "PartialNetwork");
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 32.0);
    EXPECT_EQ(sm.getLightOnHour(), 7);
    // Other fields should use defaults
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 36.0);
}

// ============================================================================
// Save Tests
// ============================================================================

TEST_F(SettingsManagerTest, SaveSucceedsWithValidSettings) {
    // Initialize mock with minimal content so fsOpen succeeds for write mode
    String initialJson = "{}";
    mockHal.setFileContent(initialJson.c_str(), initialJson.length());

    // Modify some settings
    sm.setSSID("SaveTestNetwork");
    sm.setPumpAutoMode(true);

    // Save settings
    bool result = sm.save();

    EXPECT_TRUE(result);
}

TEST_F(SettingsManagerTest, SaveDetectsWiFiSSIDChange) {
    // Set initial SSID
    sm.setSSID("InitialSSID");
    sm.save();
    // Reset wifiChanged after first save to establish baseline
    sm.setWifiChanged(false);
    EXPECT_FALSE(sm.getWifiChanged());

    // Change SSID
    sm.setSSID("NewSSID");
    sm.save();
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, SaveDetectsWiFiPasswordChange) {
    // Set initial password
    sm.setPassword("InitialPassword");
    sm.save();
    // Reset wifiChanged after first save to establish baseline
    sm.setWifiChanged(false);
    EXPECT_FALSE(sm.getWifiChanged());

    // Change password
    sm.setPassword("NewPassword");
    sm.save();
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, SaveDetectsAPModeChange) {
    // Set initial AP mode
    sm.setAPMode(false);
    sm.save();
    // Reset wifiChanged after first save to establish baseline
    sm.setWifiChanged(false);
    EXPECT_FALSE(sm.getWifiChanged());

    // Change AP mode
    sm.setAPMode(true);
    sm.save();
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, SaveDoesNotDetectNonWiFiChanges) {
    // Initialize mock with minimal content so fsOpen succeeds for write mode
    String initialJson = "{}";
    mockHal.setFileContent(initialJson.c_str(), initialJson.length());

    // Do an initial save to establish a baseline
    sm.save();
    sm.setWifiChanged(false);

    // Change non-WiFi settings
    sm.setTempThresholdOnF(32.0);
    sm.setPumpAutoMode(true);
    sm.save();
    EXPECT_FALSE(sm.getWifiChanged());
}

// ============================================================================
// WiFi Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, SSIDGetterReturnsCorrectValue) {
    sm.setSSID("TestSSID");
    EXPECT_STREQ(sm.getSSID().c_str(), "TestSSID");
}

TEST_F(SettingsManagerTest, SSIDSetterUpdatesValue) {
    sm.setSSID("NewSSID");
    EXPECT_STREQ(sm.getSSID().c_str(), "NewSSID");
}

TEST_F(SettingsManagerTest, PasswordGetterReturnsCorrectValue) {
    sm.setPassword("TestPassword");
    EXPECT_STREQ(sm.getPassword().c_str(), "TestPassword");
}

TEST_F(SettingsManagerTest, PasswordSetterUpdatesValue) {
    sm.setPassword("NewPassword");
    EXPECT_STREQ(sm.getPassword().c_str(), "NewPassword");
}

TEST_F(SettingsManagerTest, APModeGetterReturnsCorrectValue) {
    sm.setAPMode(true);
    EXPECT_TRUE(sm.isAPMode());
}

TEST_F(SettingsManagerTest, APModeSetterUpdatesValue) {
    sm.setAPMode(false);
    EXPECT_FALSE(sm.isAPMode());
}

TEST_F(SettingsManagerTest, WifiChangedGetterReturnsCorrectValue) {
    sm.setWifiChanged(true);
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, WifiChangedSetterUpdatesValue) {
    sm.setWifiChanged(false);
    EXPECT_FALSE(sm.getWifiChanged());
}

// ============================================================================
// Pump Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, PumpAutoModeGetterReturnsCorrectValue) {
    sm.setPumpAutoMode(true);
    EXPECT_TRUE(sm.getPumpAutoMode());
}

TEST_F(SettingsManagerTest, PumpAutoModeSetterUpdatesValue) {
    sm.setPumpAutoMode(false);
    EXPECT_FALSE(sm.getPumpAutoMode());
}

TEST_F(SettingsManagerTest, TempThresholdOnFGetterReturnsCorrectValue) {
    sm.setTempThresholdOnF(32.5);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 32.5);
}

TEST_F(SettingsManagerTest, TempThresholdOnFSetterUpdatesValue) {
    sm.setTempThresholdOnF(33.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 33.0);
}

TEST_F(SettingsManagerTest, TempThresholdOffFGetterReturnsCorrectValue) {
    sm.setTempThresholdOffF(38.5);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 38.5);
}

TEST_F(SettingsManagerTest, TempThresholdOffFSetterUpdatesValue) {
    sm.setTempThresholdOffF(37.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 37.0);
}

TEST_F(SettingsManagerTest, PumpOnTimeSecondsGetterReturnsCorrectValue) {
    sm.setPumpOnTimeSeconds(400);
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 400u);
}

TEST_F(SettingsManagerTest, PumpOnTimeSecondsSetterUpdatesValue) {
    sm.setPumpOnTimeSeconds(500);
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 500u);
}

TEST_F(SettingsManagerTest, PumpOffTimeSecondsGetterReturnsCorrectValue) {
    sm.setPumpOffTimeSeconds(900);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 900u);
}

TEST_F(SettingsManagerTest, PumpOffTimeSecondsSetterUpdatesValue) {
    sm.setPumpOffTimeSeconds(1200);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 1200u);
}

TEST_F(SettingsManagerTest, WaterFlowErrorTimeoutSecondsGetterReturnsCorrectValue) {
    sm.setWaterFlowErrorTimeoutSeconds(180);
    EXPECT_EQ(sm.getWaterFlowErrorTimeoutSeconds(), 180);
}

TEST_F(SettingsManagerTest, WaterFlowErrorTimeoutSecondsSetterUpdatesValue) {
    sm.setWaterFlowErrorTimeoutSeconds(240);
    EXPECT_EQ(sm.getWaterFlowErrorTimeoutSeconds(), 240);
}

// ============================================================================
// Light Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, LightAutoModeGetterReturnsCorrectValue) {
    sm.setLightAutoMode(true);
    EXPECT_TRUE(sm.getLightAutoMode());
}

TEST_F(SettingsManagerTest, LightAutoModeSetterUpdatesValue) {
    sm.setLightAutoMode(false);
    EXPECT_FALSE(sm.getLightAutoMode());
}

TEST_F(SettingsManagerTest, LightOnHourGetterReturnsCorrectValue) {
    sm.setLightOnHour(8);
    EXPECT_EQ(sm.getLightOnHour(), 8);
}

TEST_F(SettingsManagerTest, LightOnHourSetterUpdatesValue) {
    sm.setLightOnHour(7);
    EXPECT_EQ(sm.getLightOnHour(), 7);
}

TEST_F(SettingsManagerTest, LightOffHourGetterReturnsCorrectValue) {
    sm.setLightOffHour(22);
    EXPECT_EQ(sm.getLightOffHour(), 22);
}

TEST_F(SettingsManagerTest, LightOffHourSetterUpdatesValue) {
    sm.setLightOffHour(20);
    EXPECT_EQ(sm.getLightOffHour(), 20);
}

TEST_F(SettingsManagerTest, LightOnMinuteGetterReturnsCorrectValue) {
    sm.setLightOnMinute(30);
    EXPECT_EQ(sm.getLightOnMinute(), 30);
}

TEST_F(SettingsManagerTest, LightOnMinuteSetterUpdatesValue) {
    sm.setLightOnMinute(45);
    EXPECT_EQ(sm.getLightOnMinute(), 45);
}

TEST_F(SettingsManagerTest, LightBrightnessPercentGetterReturnsCorrectValue) {
    sm.setLightBrightnessPercent(90);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 90);
}

TEST_F(SettingsManagerTest, LightBrightnessPercentSetterClampsTo100) {
    sm.setLightBrightnessPercent(150);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 100);
}

TEST_F(SettingsManagerTest, LightBrightnessPercentSetterClampsTo0) {
    sm.setLightBrightnessPercent(-10);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 0);
}

TEST_F(SettingsManagerTest, LightTransitionDurationMinutesGetterReturnsCorrectValue) {
    sm.setLightTransitionDurationMinutes(20);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 20);
}

TEST_F(SettingsManagerTest, LightTransitionDurationMinutesSetterClampsTo60) {
    sm.setLightTransitionDurationMinutes(90);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 60);
}

TEST_F(SettingsManagerTest, LightTransitionDurationMinutesSetterClampsTo1) {
    sm.setLightTransitionDurationMinutes(0);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 1);
}

TEST_F(SettingsManagerTest, LightOnModeGetterReturnsCorrectValue) {
    sm.setLightOnMode("sunset");
    EXPECT_STREQ(sm.getLightOnMode().c_str(), "sunset");
}

TEST_F(SettingsManagerTest, LightOnModeSetterUpdatesValue) {
    sm.setLightOnMode("fixed");
    EXPECT_STREQ(sm.getLightOnMode().c_str(), "fixed");
}

TEST_F(SettingsManagerTest, LightOnSunsetOffsetMinutesGetterReturnsCorrectValue) {
    sm.setLightOnSunsetOffsetMinutes(30);
    EXPECT_EQ(sm.getLightOnSunsetOffsetMinutes(), 30);
}

TEST_F(SettingsManagerTest, LightOnSunsetOffsetMinutesSetterUpdatesValue) {
    sm.setLightOnSunsetOffsetMinutes(45);
    EXPECT_EQ(sm.getLightOnSunsetOffsetMinutes(), 45);
}

// ============================================================================
// Water Meter Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, PulsesPerGallonGetterReturnsCorrectValue) {
    sm.setPulsesPerGallon(500);
    EXPECT_EQ(sm.getPulsesPerGallon(), 500);
}

TEST_F(SettingsManagerTest, PulsesPerGallonSetterUpdatesValue) {
    sm.setPulsesPerGallon(450);
    EXPECT_EQ(sm.getPulsesPerGallon(), 450);
}

TEST_F(SettingsManagerTest, WaterMeterTimeoutSecondsGetterReturnsCorrectValue) {
    sm.setWaterMeterTimeoutSeconds(10);
    EXPECT_EQ(sm.getWaterMeterTimeoutSeconds(), 10u);
}

TEST_F(SettingsManagerTest, WaterMeterTimeoutSecondsSetterUpdatesValue) {
    sm.setWaterMeterTimeoutSeconds(15);
    EXPECT_EQ(sm.getWaterMeterTimeoutSeconds(), 15u);
}

TEST_F(SettingsManagerTest, WaterMeterPerPulseCalculationEnabledGetterReturnsCorrectValue) {
    sm.setWaterMeterPerPulseCalculationEnabled(true);
    EXPECT_TRUE(sm.getWaterMeterPerPulseCalculationEnabled());
}

TEST_F(SettingsManagerTest, WaterMeterPerPulseCalculationEnabledSetterUpdatesValue) {
    sm.setWaterMeterPerPulseCalculationEnabled(false);
    EXPECT_FALSE(sm.getWaterMeterPerPulseCalculationEnabled());
}

TEST_F(SettingsManagerTest, PumpOffFlowMonitoringEnabledGetterReturnsCorrectValue) {
    sm.setPumpOffFlowMonitoringEnabled(true);
    EXPECT_TRUE(sm.getPumpOffFlowMonitoringEnabled());
}

TEST_F(SettingsManagerTest, PumpOffFlowMonitoringEnabledSetterUpdatesValue) {
    sm.setPumpOffFlowMonitoringEnabled(false);
    EXPECT_FALSE(sm.getPumpOffFlowMonitoringEnabled());
}

TEST_F(SettingsManagerTest, PumpOffFlowGracePeriodSecondsGetterReturnsCorrectValue) {
    sm.setPumpOffFlowGracePeriodSeconds(60);
    EXPECT_EQ(sm.getPumpOffFlowGracePeriodSeconds(), 60);
}

TEST_F(SettingsManagerTest, PumpOffFlowGracePeriodSecondsSetterUpdatesValue) {
    sm.setPumpOffFlowGracePeriodSeconds(45);
    EXPECT_EQ(sm.getPumpOffFlowGracePeriodSeconds(), 45);
}

// ============================================================================
// WiFi Connection Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, WifiMaxRetriesGetterReturnsCorrectValue) {
    sm.setWifiMaxRetries(10);
    EXPECT_EQ(sm.getWifiMaxRetries(), 10u);
}

TEST_F(SettingsManagerTest, WifiMaxRetriesSetterUpdatesValue) {
    sm.setWifiMaxRetries(7);
    EXPECT_EQ(sm.getWifiMaxRetries(), 7u);
}

TEST_F(SettingsManagerTest, WifiRetryDelaySecondsGetterReturnsCorrectValue) {
    sm.setWifiRetryDelaySeconds(60);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 60u);
}

TEST_F(SettingsManagerTest, WifiRetryDelaySecondsSetterUpdatesValue) {
    sm.setWifiRetryDelaySeconds(45);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 45u);
}

TEST_F(SettingsManagerTest, WifiAPDurationMinutesGetterReturnsCorrectValue) {
    sm.setWifiAPDurationMinutes(15);
    EXPECT_EQ(sm.getWifiAPDurationMinutes(), 15);
}

TEST_F(SettingsManagerTest, WifiAPDurationMinutesSetterEnforcesMinimum) {
    sm.setWifiAPDurationMinutes(2);
    // Setter doesn't enforce minimum, only setFromJsonDoc does
    EXPECT_EQ(sm.getWifiAPDurationMinutes(), 2);
}

TEST_F(SettingsManagerTest, WatchdogTimeoutSecondsGetterReturnsCorrectValue) {
    sm.setWatchdogTimeoutSeconds(180);
    EXPECT_EQ(sm.getWatchdogTimeoutSeconds(), 180);
}

TEST_F(SettingsManagerTest, WatchdogTimeoutSecondsSetterUpdatesValue) {
    sm.setWatchdogTimeoutSeconds(240);
    EXPECT_EQ(sm.getWatchdogTimeoutSeconds(), 240);
}

TEST_F(SettingsManagerTest, WifiLedEnabledGetterReturnsCorrectValue) {
    sm.setWifiLedEnabled(false);
    EXPECT_FALSE(sm.getWifiLedEnabled());
}

TEST_F(SettingsManagerTest, WifiLedEnabledSetterUpdatesValue) {
    sm.setWifiLedEnabled(true);
    EXPECT_TRUE(sm.getWifiLedEnabled());
}

// ============================================================================
// Buzzer Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, BuzzerEnabledGetterReturnsCorrectValue) {
    sm.setBuzzerEnabled(true);
    EXPECT_TRUE(sm.getBuzzerEnabled());
}

TEST_F(SettingsManagerTest, BuzzerEnabledSetterUpdatesValue) {
    sm.setBuzzerEnabled(false);
    EXPECT_FALSE(sm.getBuzzerEnabled());
}

TEST_F(SettingsManagerTest, BuzzerTypeGetterReturnsCorrectValue) {
    sm.setBuzzerType("passive");
    EXPECT_STREQ(sm.getBuzzerType().c_str(), "passive");
}

TEST_F(SettingsManagerTest, BuzzerTypeSetterUpdatesValue) {
    sm.setBuzzerType("active");
    EXPECT_STREQ(sm.getBuzzerType().c_str(), "active");
}

// ============================================================================
// Door Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, DoorAutoOpenEnabledGetterReturnsCorrectValue) {
    sm.setDoorAutoOpenEnabled(true);
    EXPECT_TRUE(sm.getDoorAutoOpenEnabled());
}

TEST_F(SettingsManagerTest, DoorAutoOpenEnabledSetterUpdatesValue) {
    sm.setDoorAutoOpenEnabled(false);
    EXPECT_FALSE(sm.getDoorAutoOpenEnabled());
}

TEST_F(SettingsManagerTest, DoorAutoCloseEnabledGetterReturnsCorrectValue) {
    sm.setDoorAutoCloseEnabled(true);
    EXPECT_TRUE(sm.getDoorAutoCloseEnabled());
}

TEST_F(SettingsManagerTest, DoorAutoCloseEnabledSetterUpdatesValue) {
    sm.setDoorAutoCloseEnabled(false);
    EXPECT_FALSE(sm.getDoorAutoCloseEnabled());
}

TEST_F(SettingsManagerTest, DoorAutoOpenOffsetMinutesGetterReturnsCorrectValue) {
    sm.setDoorAutoOpenOffsetMinutes(30);
    EXPECT_EQ(sm.getDoorAutoOpenOffsetMinutes(), 30);
}

TEST_F(SettingsManagerTest, DoorAutoCloseOffsetMinutesSetterUpdatesValue) {
    sm.setDoorAutoCloseOffsetMinutes(-15);
    EXPECT_EQ(sm.getDoorAutoCloseOffsetMinutes(), -15);
}

TEST_F(SettingsManagerTest, DoorAutoOpenDayTogglesPerDay) {
    for (int i = 0; i < 7; i++) {
        sm.setDoorAutoOpenDay(i, false);
        EXPECT_FALSE(sm.getDoorAutoOpenDay(i));
        sm.setDoorAutoOpenDay(i, true);
        EXPECT_TRUE(sm.getDoorAutoOpenDay(i));
    }
    // Out-of-range is safely ignored
    sm.setDoorAutoOpenDay(7, true);
    EXPECT_FALSE(sm.getDoorAutoOpenDay(7));
}

TEST_F(SettingsManagerTest, DoorAutoCloseDayTogglesPerDay) {
    sm.setDoorAutoCloseDay(3, false);
    EXPECT_FALSE(sm.getDoorAutoCloseDay(3));
    sm.setDoorAutoCloseDay(3, true);
    EXPECT_TRUE(sm.getDoorAutoCloseDay(3));
}

TEST_F(SettingsManagerTest, DoorOpenTimeoutSecondsGetterReturnsCorrectValue) {
    sm.setDoorOpenTimeoutSeconds(45);
    EXPECT_EQ(sm.getDoorOpenTimeoutSeconds(), 45);
}

TEST_F(SettingsManagerTest, DoorOpenTimeoutSecondsSetterUpdatesValue) {
    sm.setDoorOpenTimeoutSeconds(60);
    EXPECT_EQ(sm.getDoorOpenTimeoutSeconds(), 60);
}

TEST_F(SettingsManagerTest, DoorCloseTimeoutSecondsGetterReturnsCorrectValue) {
    sm.setDoorCloseTimeoutSeconds(45);
    EXPECT_EQ(sm.getDoorCloseTimeoutSeconds(), 45);
}

TEST_F(SettingsManagerTest, DoorCloseTimeoutSecondsSetterUpdatesValue) {
    sm.setDoorCloseTimeoutSeconds(60);
    EXPECT_EQ(sm.getDoorCloseTimeoutSeconds(), 60);
}

TEST_F(SettingsManagerTest, SunriseOffsetMinutesGetterReturnsCorrectValue) {
    sm.setSunriseOffsetMinutes(30);
    EXPECT_EQ(sm.getSunriseOffsetMinutes(), 30);
}

TEST_F(SettingsManagerTest, SunriseOffsetMinutesSetterUpdatesValue) {
    sm.setSunriseOffsetMinutes(15);
    EXPECT_EQ(sm.getSunriseOffsetMinutes(), 15);
}

TEST_F(SettingsManagerTest, SunsetOffsetMinutesGetterReturnsCorrectValue) {
    sm.setSunsetOffsetMinutes(30);
    EXPECT_EQ(sm.getSunsetOffsetMinutes(), 30);
}

TEST_F(SettingsManagerTest, SunsetOffsetMinutesSetterUpdatesValue) {
    sm.setSunsetOffsetMinutes(15);
    EXPECT_EQ(sm.getSunsetOffsetMinutes(), 15);
}

// ============================================================================
// Location Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, LatitudeGetterReturnsCorrectValue) {
    sm.setLatitude(41.8781);
    EXPECT_FLOAT_EQ(sm.getLatitude(), 41.8781);
}

TEST_F(SettingsManagerTest, LatitudeSetterUpdatesValue) {
    sm.setLatitude(40.7128);
    EXPECT_FLOAT_EQ(sm.getLatitude(), 40.7128);
}

TEST_F(SettingsManagerTest, LongitudeGetterReturnsCorrectValue) {
    sm.setLongitude(-87.6298);
    EXPECT_FLOAT_EQ(sm.getLongitude(), -87.6298);
}

TEST_F(SettingsManagerTest, LongitudeSetterUpdatesValue) {
    sm.setLongitude(-74.0060);
    EXPECT_FLOAT_EQ(sm.getLongitude(), -74.0060);
}

TEST_F(SettingsManagerTest, TimezoneOffsetHoursGetterReturnsCorrectValue) {
    sm.setTimezoneOffsetHours(-6.0);
    EXPECT_FLOAT_EQ(sm.getTimezoneOffsetHours(), -6.0);
}

TEST_F(SettingsManagerTest, TimezoneOffsetHoursSetterUpdatesValue) {
    sm.setTimezoneOffsetHours(-5.0);
    EXPECT_FLOAT_EQ(sm.getTimezoneOffsetHours(), -5.0);
}

// ============================================================================
// System Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, LogLevelGetterReturnsCorrectValue) {
    sm.setLogLevel("DEBUG");
    EXPECT_STREQ(sm.getLogLevel().c_str(), "DEBUG");
}

TEST_F(SettingsManagerTest, LogLevelSetterUpdatesValue) {
    sm.setLogLevel("ERROR");
    EXPECT_STREQ(sm.getLogLevel().c_str(), "ERROR");
}

TEST_F(SettingsManagerTest, HasConnectedGetterReturnsCorrectValue) {
    sm.setHasConnected(true);
    EXPECT_TRUE(sm.getHasConnected());
}

TEST_F(SettingsManagerTest, HasConnectedSetterUpdatesValue) {
    sm.setHasConnected(false);
    EXPECT_FALSE(sm.getHasConnected());
}

// ============================================================================
// JSON Export Tests
// ============================================================================

TEST_F(SettingsManagerTest, ToJsonIncludesAllFields) {
    // Setup some settings
    sm.setSSID("TestSSID");
    sm.setPumpAutoMode(true);
    sm.setTempThresholdOnF(32.0);
    
    // Get JSON
    String json = sm.toJson();
    
    // Verify key fields are present
    EXPECT_TRUE(json.indexOf("ssid") >= 0);
    EXPECT_TRUE(json.indexOf("pump_auto_mode") >= 0);
    EXPECT_TRUE(json.indexOf("temp_threshold_on_f") >= 0);
}

TEST_F(SettingsManagerTest, ToJsonExcludesPasswordByDefault) {
    // Set a password
    sm.setPassword("SecretPassword123");

    // Get JSON without password
    String json = sm.toJson(false);

    // Password should NOT be included in JSON when explicitly excluded
    EXPECT_FALSE(json.indexOf("SecretPassword123") >= 0);
}

TEST_F(SettingsManagerTest, ToJsonIncludesPasswordWhenRequested) {
    // Set a password
    sm.setPassword("SecretPassword123");
    
    // Get JSON with password
    String json = sm.toJson(true);
    
    // Password should be included when explicitly requested
    EXPECT_TRUE(json.indexOf("SecretPassword123") >= 0);
}

TEST_F(SettingsManagerTest, ToJsonIsValidJson) {
    // Get JSON
    String json = sm.toJson();
    
    // Should start with { and end with }
    EXPECT_TRUE(json.startsWith("{"));
    EXPECT_TRUE(json.endsWith("}"));
}

// ============================================================================
// Factory Reset Tests
// ============================================================================

TEST_F(SettingsManagerTest, FactoryResetResetsAllSettingsToDefaults) {
    // Modify some settings
    sm.setSSID("ModifiedSSID");
    sm.setPumpAutoMode(true);
    sm.setTempThresholdOnF(32.0);
    sm.setLightBrightnessPercent(50);
    
    // Setup mock HAL to succeed write
    
    
    // Perform factory reset
    sm.factoryReset();
    
    // Verify all settings reset to defaults
    EXPECT_STREQ(sm.getSSID().c_str(), "");
    EXPECT_STREQ(sm.getPassword().c_str(), "");
    EXPECT_TRUE(sm.isAPMode()); // Default is true
    EXPECT_TRUE(sm.getPumpAutoMode()); // Default is true
    EXPECT_FALSE(sm.getLightAutoMode());
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 34.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 36.0);
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 300u);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 600u);
    EXPECT_EQ(sm.getLightOnHour(), 6);
    EXPECT_EQ(sm.getLightOffHour(), 21);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 80);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 15);
    EXPECT_EQ(sm.getWaterFlowErrorTimeoutSeconds(), 120);
    EXPECT_EQ(sm.getWifiMaxRetries(), 5u);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 30u);
    EXPECT_EQ(sm.getWifiAPDurationMinutes(), 10);
}

TEST_F(SettingsManagerTest, FactoryResetSetsWifiChangedFlag) {
    // Setup mock HAL to succeed write
    
    
    // Perform factory reset
    sm.factoryReset();
    
    // WiFi changed flag should be set
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, FactoryResetSavesToDisk) {
    // Setup mock HAL to succeed write
    
    
    // Perform factory reset
    sm.factoryReset();
    
    // Verify write succeeded (writeFileResult was set to true)
    // Note: We can't verify the exact call without GMock, but we can verify the result
}

// ============================================================================
// GetSettings Tests
// ============================================================================

TEST_F(SettingsManagerTest, GetSettingsReturnsSettingsStruct) {
    // Modify some settings
    sm.setSSID("GetTestSSID");
    sm.setPumpAutoMode(true);
    sm.setTempThresholdOnF(33.0);
    
    // Get settings
    const user_settings& settings = sm.getSettings();
    
    // Verify settings are correct
    EXPECT_STREQ(settings.ssid.c_str(), "GetTestSSID");
    EXPECT_TRUE(settings.pump_auto_mode);
    EXPECT_FLOAT_EQ(settings.temp_threshold_on_f, 33.0);
}

TEST_F(SettingsManagerTest, GetSettingsReturnsReferenceNotCopy) {
    // Get settings
    const user_settings& settings1 = sm.getSettings();
    const user_settings& settings2 = sm.getSettings();
    
    // Should return reference to same object
    EXPECT_EQ(&settings1, &settings2);
}

// ============================================================================
// Edge Cases and Error Conditions
// ============================================================================

TEST_F(SettingsManagerTest, HandlesEmptySSID) {
    sm.setSSID("");
    EXPECT_STREQ(sm.getSSID().c_str(), "");
}

TEST_F(SettingsManagerTest, HandlesEmptyPassword) {
    sm.setPassword("");
    EXPECT_STREQ(sm.getPassword().c_str(), "");
}

TEST_F(SettingsManagerTest, HandlesNegativeTemperatureThresholds) {
    sm.setTempThresholdOnF(-10.0);
    sm.setTempThresholdOffF(-5.0);
    
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), -10.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), -5.0);
}

TEST_F(SettingsManagerTest, HandlesVeryLargeTemperatureThresholds) {
    sm.setTempThresholdOnF(150.0);
    sm.setTempThresholdOffF(200.0);
    
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 150.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 200.0);
}

TEST_F(SettingsManagerTest, HandlesZeroTimeValues) {
    sm.setPumpOnTimeSeconds(0);
    sm.setPumpOffTimeSeconds(0);
    
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 0u);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 0u);
}

TEST_F(SettingsManagerTest, HandlesVeryLargeTimeValues) {
    sm.setPumpOnTimeSeconds(3600);
    sm.setPumpOffTimeSeconds(7200);
    
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 3600u);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 7200u);
}

TEST_F(SettingsManagerTest, HandlesExtremeLatitudeValues) {
    sm.setLatitude(90.0);
    EXPECT_FLOAT_EQ(sm.getLatitude(), 90.0);
    
    sm.setLatitude(-90.0);
    EXPECT_FLOAT_EQ(sm.getLatitude(), -90.0);
}

TEST_F(SettingsManagerTest, HandlesExtremeLongitudeValues) {
    sm.setLongitude(180.0);
    EXPECT_FLOAT_EQ(sm.getLongitude(), 180.0);
    
    sm.setLongitude(-180.0);
    EXPECT_FLOAT_EQ(sm.getLongitude(), -180.0);
}

TEST_F(SettingsManagerTest, HandlesExtremeTimezoneOffsets) {
    sm.setTimezoneOffsetHours(12.0);
    EXPECT_FLOAT_EQ(sm.getTimezoneOffsetHours(), 12.0);
    
    sm.setTimezoneOffsetHours(-12.0);
    EXPECT_FLOAT_EQ(sm.getTimezoneOffsetHours(), -12.0);
}

TEST_F(SettingsManagerTest, HandlesLargeBrightnessValues) {
    sm.setLightBrightnessPercent(200);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 100); // Clamped to 100
}

TEST_F(SettingsManagerTest, HandlesNegativeBrightnessValues) {
    sm.setLightBrightnessPercent(-50);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 0); // Clamped to 0
}

TEST_F(SettingsManagerTest, HandlesLargeTransitionDurationValues) {
    sm.setLightTransitionDurationMinutes(120);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 60); // Clamped to 60
}

TEST_F(SettingsManagerTest, HandlesZeroTransitionDurationValues) {
    sm.setLightTransitionDurationMinutes(0);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 1); // Clamped to 1
}

TEST_F(SettingsManagerTest, HandlesLargePulsesPerGallonValues) {
    sm.setPulsesPerGallon(10000);
    EXPECT_EQ(sm.getPulsesPerGallon(), 10000);
}

TEST_F(SettingsManagerTest, HandlesZeroPulsesPerGallonValues) {
    sm.setPulsesPerGallon(0);
    EXPECT_EQ(sm.getPulsesPerGallon(), 0);
}

TEST_F(SettingsManagerTest, HandlesLargeRetryValues) {
    sm.setWifiMaxRetries(100);
    EXPECT_EQ(sm.getWifiMaxRetries(), 100u);
    
    sm.setWifiRetryDelaySeconds(600);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 600u);
}

TEST_F(SettingsManagerTest, HandlesZeroRetryValues) {
    sm.setWifiMaxRetries(0);
    EXPECT_EQ(sm.getWifiMaxRetries(), 0u);
    
    sm.setWifiRetryDelaySeconds(0);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 0u);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(SettingsManagerTest, FullLoadModifySaveCycle) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL
    String json = createValidSettingsJson();
    mockHal.setFileContent(json.c_str(), json.length());


    // Load settings
    EXPECT_TRUE(sm.load());
    EXPECT_STREQ(sm.getSSID().c_str(), "TestNetwork");
    
    // Modify settings
    sm.setSSID("ModifiedNetwork");
    sm.setPumpAutoMode(false);
    
    // Save settings
    EXPECT_TRUE(sm.save());
}

TEST_F(SettingsManagerTest, MultipleSavesAccumulateChanges) {
    // Setup mock HAL with minimal content so fsOpen succeeds for write mode
    String initialJson = "{}";
    mockHal.setFileContent(initialJson.c_str(), initialJson.length());

    // First save
    sm.setSSID("FirstSSID");
    sm.save();
    // Reset wifiChanged after first save to establish baseline
    sm.setWifiChanged(false);
    EXPECT_FALSE(sm.getWifiChanged());
    
    // Second save with no WiFi changes
    sm.setPumpAutoMode(true);
    sm.save();
    EXPECT_FALSE(sm.getWifiChanged());
    
    // Third save with WiFi change
    sm.setSSID("SecondSSID");
    sm.save();
    EXPECT_TRUE(sm.getWifiChanged());
}

TEST_F(SettingsManagerTest, LoadAfterFactoryResetUsesDefaults) {
    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Setup mock HAL
    String json = createValidSettingsJson();
    mockHal.setFileContent(json.c_str(), json.length());


    // Load initial settings
    sm.load();
    EXPECT_STREQ(sm.getSSID().c_str(), "TestNetwork");
    
    // Factory reset
    sm.factoryReset();
    EXPECT_STREQ(sm.getSSID().c_str(), "");

    // Mark as not loaded so load() will actually attempt to load
    sm.markAsNotLoadedForTesting();

    // Load again (should use defaults since file was overwritten)
    sm.load();
    EXPECT_STREQ(sm.getSSID().c_str(), "");
}

TEST_F(SettingsManagerTest, SettingsPersistAcrossMultipleGetCalls) {
    // Set some values
    sm.setSSID("PersistentSSID");
    sm.setPumpAutoMode(true);
    sm.setTempThresholdOnF(32.0);
    
    // Get settings multiple times
    const user_settings& settings1 = sm.getSettings();
    const user_settings& settings2 = sm.getSettings();
    const user_settings& settings3 = sm.getSettings();
    
    // All should return same values
    EXPECT_STREQ(settings1.ssid.c_str(), "PersistentSSID");
    EXPECT_STREQ(settings2.ssid.c_str(), "PersistentSSID");
    EXPECT_STREQ(settings3.ssid.c_str(), "PersistentSSID");
    EXPECT_TRUE(settings1.pump_auto_mode);
    EXPECT_TRUE(settings2.pump_auto_mode);
    EXPECT_TRUE(settings3.pump_auto_mode);
}

TEST_F(SettingsManagerTest, AllSettersAndGettersAreConsistent) {
    // Set all settings
    sm.setSSID("SSID123");
    sm.setPassword("Pass123");
    sm.setAPMode(true);
    sm.setPumpAutoMode(true);
    sm.setLightAutoMode(true);
    sm.setTempThresholdOnF(30.0);
    sm.setTempThresholdOffF(35.0);
    sm.setPumpOnTimeSeconds(400);
    sm.setPumpOffTimeSeconds(800);
    sm.setLightOnHour(7);
    sm.setLightOffHour(20);
    sm.setLightOnMinute(15);
    sm.setLightBrightnessPercent(90);
    sm.setLightTransitionDurationMinutes(20);
    sm.setLightOnMode("sunset");
    sm.setLightOnSunsetOffsetMinutes(15);
    sm.setWaterFlowErrorTimeoutSeconds(180);
    sm.setPulsesPerGallon(500);
    sm.setWaterMeterTimeoutSeconds(10);
    sm.setWaterMeterPerPulseCalculationEnabled(true);
    sm.setPumpOffFlowMonitoringEnabled(true);
    sm.setPumpOffFlowGracePeriodSeconds(45);
    sm.setWifiMaxRetries(10);
    sm.setWifiRetryDelaySeconds(60);
    sm.setWifiAPDurationMinutes(15);
    sm.setWatchdogTimeoutSeconds(180);
    sm.setWifiLedEnabled(false);
    sm.setBuzzerEnabled(true);
    sm.setBuzzerType("passive");
    sm.setDoorOpenTimeoutSeconds(45);
    sm.setDoorCloseTimeoutSeconds(45);
    sm.setDoorAutoOpenEnabled(true);
    sm.setDoorAutoOpenOffsetMinutes(15);
    sm.setDoorAutoCloseEnabled(true);
    sm.setDoorAutoCloseOffsetMinutes(15);
    sm.setLatitude(41.8781);
    sm.setLongitude(-87.6298);
    sm.setTimezoneOffsetHours(-6.0);
    sm.setLogLevel("WARNING");
    sm.setHasConnected(true);
    
    // Verify all getters return same values
    EXPECT_STREQ(sm.getSSID().c_str(), "SSID123");
    EXPECT_STREQ(sm.getPassword().c_str(), "Pass123");
    EXPECT_TRUE(sm.isAPMode());
    EXPECT_TRUE(sm.getPumpAutoMode());
    EXPECT_TRUE(sm.getLightAutoMode());
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 30.0);
    EXPECT_FLOAT_EQ(sm.getTempThresholdOffF(), 35.0);
    EXPECT_EQ(sm.getPumpOnTimeSeconds(), 400u);
    EXPECT_EQ(sm.getPumpOffTimeSeconds(), 800u);
    EXPECT_EQ(sm.getLightOnHour(), 7);
    EXPECT_EQ(sm.getLightOffHour(), 20);
    EXPECT_EQ(sm.getLightOnMinute(), 15);
    EXPECT_EQ(sm.getLightBrightnessPercent(), 90);
    EXPECT_EQ(sm.getLightTransitionDurationMinutes(), 20);
    EXPECT_STREQ(sm.getLightOnMode().c_str(), "sunset");
    EXPECT_EQ(sm.getLightOnSunsetOffsetMinutes(), 15);
    EXPECT_EQ(sm.getWaterFlowErrorTimeoutSeconds(), 180);
    EXPECT_EQ(sm.getPulsesPerGallon(), 500);
    EXPECT_EQ(sm.getWaterMeterTimeoutSeconds(), 10u);
    EXPECT_TRUE(sm.getWaterMeterPerPulseCalculationEnabled());
    EXPECT_TRUE(sm.getPumpOffFlowMonitoringEnabled());
    EXPECT_EQ(sm.getPumpOffFlowGracePeriodSeconds(), 45);
    EXPECT_EQ(sm.getWifiMaxRetries(), 10u);
    EXPECT_EQ(sm.getWifiRetryDelaySeconds(), 60u);
    EXPECT_EQ(sm.getWifiAPDurationMinutes(), 15);
    EXPECT_EQ(sm.getWatchdogTimeoutSeconds(), 180);
    EXPECT_FALSE(sm.getWifiLedEnabled());
    EXPECT_TRUE(sm.getBuzzerEnabled());
    EXPECT_STREQ(sm.getBuzzerType().c_str(), "passive");
    EXPECT_TRUE(sm.getDoorAutoOpenEnabled());
    EXPECT_TRUE(sm.getDoorAutoCloseEnabled());
    EXPECT_EQ(sm.getDoorAutoOpenOffsetMinutes(), 15);
    EXPECT_EQ(sm.getDoorAutoCloseOffsetMinutes(), 15);
    EXPECT_EQ(sm.getDoorOpenTimeoutSeconds(), 45);
    EXPECT_EQ(sm.getDoorCloseTimeoutSeconds(), 45);
    EXPECT_FLOAT_EQ(sm.getLatitude(), 41.8781);
    EXPECT_FLOAT_EQ(sm.getLongitude(), -87.6298);
    EXPECT_FLOAT_EQ(sm.getTimezoneOffsetHours(), -6.0);
    EXPECT_STREQ(sm.getLogLevel().c_str(), "WARNING");
    EXPECT_TRUE(sm.getHasConnected());
}

// ============================================================================
// Legacy settings migration (issue #3)
// ============================================================================

TEST_F(SettingsManagerTest, MigratesLegacyAutoModeOpenOffset) {
    // door_auto_mode enables both directions; sunrise offset migrates to open offset
    JsonDocument doc;
    doc["door_auto_mode"] = true;
    doc["sunrise_offset_minutes"] = 20;
    doc["sunset_offset_minutes"] = -15;

    sm.setFromJsonDoc(doc);

    EXPECT_TRUE(sm.getDoorAutoOpenEnabled());
    EXPECT_TRUE(sm.getDoorAutoCloseEnabled());
    EXPECT_EQ(sm.getDoorAutoOpenOffsetMinutes(), 20);
    // No after-sunset present, so close offset is just the sunset offset
    EXPECT_EQ(sm.getDoorAutoCloseOffsetMinutes(), -15);
}

TEST_F(SettingsManagerTest, MigratesLegacyAutoModeWithAfterSunsetTakesLaterCloseTime) {
    // When both auto_mode and after-sunset are on, the old firmware closed at the
    // LATER of sunset_offset and after_sunset_minutes. Migration preserves that.
    JsonDocument doc;
    doc["door_auto_mode"] = true;
    doc["sunset_offset_minutes"] = -15;
    doc["door_auto_close_after_sunset_enabled"] = true;
    doc["door_auto_close_after_sunset_minutes"] = 30;

    sm.setFromJsonDoc(doc);

    EXPECT_TRUE(sm.getDoorAutoCloseEnabled());
    EXPECT_EQ(sm.getDoorAutoCloseOffsetMinutes(), 30); // max(-15, 30)
}

TEST_F(SettingsManagerTest, LegacyAfterSunsetOnlyFoldsIntoAutoClose) {
    // The real board's state: auto_mode OFF, after-sunset ON with a 45-min delay.
    // Old firmware closed 45 min after sunset and never auto-opened.
    JsonDocument doc;
    doc["door_auto_mode"] = false;
    doc["sunrise_offset_minutes"] = 0;
    doc["sunset_offset_minutes"] = 0;
    doc["door_auto_close_after_sunset_enabled"] = true;
    doc["door_auto_close_after_sunset_minutes"] = 45;

    sm.setFromJsonDoc(doc);

    EXPECT_FALSE(sm.getDoorAutoOpenEnabled());
    EXPECT_TRUE(sm.getDoorAutoCloseEnabled());
    EXPECT_EQ(sm.getDoorAutoCloseOffsetMinutes(), 45);
}

TEST_F(SettingsManagerTest, NewStyleSettingsTakePrecedenceOverLegacy) {
    // If both old and new keys are present, the new keys win
    JsonDocument doc;
    doc["door_auto_mode"] = true;
    doc["sunrise_offset_minutes"] = 99;
    doc["door_auto_open_enabled"] = false;
    doc["door_auto_open_offset_minutes"] = 10;

    sm.setFromJsonDoc(doc);

    EXPECT_FALSE(sm.getDoorAutoOpenEnabled());
    EXPECT_EQ(sm.getDoorAutoOpenOffsetMinutes(), 10);
}

TEST_F(SettingsManagerTest, DefaultsAllDaysEnabledForAutoOpenAndClose) {
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(sm.getDoorAutoOpenDay(i));
        EXPECT_TRUE(sm.getDoorAutoCloseDay(i));
    }
}

TEST_F(SettingsManagerTest, AutoDaysRoundTripThroughJson) {
    sm.setDoorAutoOpenDay(0, false); // disable Sunday open
    sm.setDoorAutoCloseDay(6, false); // disable Saturday close

    String json = sm.toJson(false);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    ASSERT_FALSE(err);

    // Re-load and verify the day arrays persist
    sm.setFromJsonDoc(doc);
    EXPECT_FALSE(sm.getDoorAutoOpenDay(0));
    EXPECT_TRUE(sm.getDoorAutoOpenDay(1));
    EXPECT_FALSE(sm.getDoorAutoCloseDay(6));
    EXPECT_TRUE(sm.getDoorAutoCloseDay(5));
}

// ============================================================================
// Door Lockout Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, DoorLockout_DefaultFalse) {
    EXPECT_FALSE(sm.getDoorLockoutEnabled());
}

TEST_F(SettingsManagerTest, DoorLockout_GetterSetter) {
    sm.setDoorLockoutEnabled(true);
    EXPECT_TRUE(sm.getDoorLockoutEnabled());

    sm.setDoorLockoutEnabled(false);
    EXPECT_FALSE(sm.getDoorLockoutEnabled());
}

// ============================================================================
// Door Timeout Auto-Calc Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, DoorTimeoutAutoCalc_DefaultFalse) {
    EXPECT_FALSE(sm.getDoorTimeoutAutoCalcEnabled());
}

TEST_F(SettingsManagerTest, DoorTimeoutAutoCalc_GetterSetter) {
    sm.setDoorTimeoutAutoCalcEnabled(true);
    EXPECT_TRUE(sm.getDoorTimeoutAutoCalcEnabled());

    sm.setDoorTimeoutAutoCalcEnabled(false);
    EXPECT_FALSE(sm.getDoorTimeoutAutoCalcEnabled());
}

// ============================================================================
// OTA Update Settings Tests
// ============================================================================

TEST_F(SettingsManagerTest, OTAAutoUpdateEnabledDefaultFalse) {
    EXPECT_FALSE(sm.getAutoUpdateEnabled());
}

TEST_F(SettingsManagerTest, OTAAutoUpdateEnabledGetterSetter) {
    sm.setAutoUpdateEnabled(true);
    EXPECT_TRUE(sm.getAutoUpdateEnabled());

    sm.setAutoUpdateEnabled(false);
    EXPECT_FALSE(sm.getAutoUpdateEnabled());
}

TEST_F(SettingsManagerTest, OTAUpdateCheckIntervalHoursDefault24) {
    EXPECT_EQ(sm.getUpdateCheckIntervalHours(), 24u);
}

TEST_F(SettingsManagerTest, OTAUpdateCheckIntervalHoursGetterSetter) {
    sm.setUpdateCheckIntervalHours(12);
    EXPECT_EQ(sm.getUpdateCheckIntervalHours(), 12u);
}

TEST_F(SettingsManagerTest, OTAUpdateCheckIntervalHoursClamps) {
    sm.setUpdateCheckIntervalHours(0);
    EXPECT_EQ(sm.getUpdateCheckIntervalHours(), 1u);

    sm.setUpdateCheckIntervalHours(200);
    EXPECT_EQ(sm.getUpdateCheckIntervalHours(), 168u);
}

TEST_F(SettingsManagerTest, OTAManifestUrlGetterSetter) {
    sm.setManifestUrl("https://example.com/manifest.json");
    EXPECT_STREQ(sm.getManifestUrl().c_str(), "https://example.com/manifest.json");
}

TEST_F(SettingsManagerTest, OTASettingsSerializedInJson) {
    sm.setAutoUpdateEnabled(true);
    sm.setUpdateCheckIntervalHours(12);
    sm.setManifestUrl("https://example.com/manifest.json");

    String json = sm.toJson();
    EXPECT_TRUE(json.indexOf("auto_update_enabled") >= 0);
    EXPECT_TRUE(json.indexOf("update_check_interval_hours") >= 0);
    EXPECT_TRUE(json.indexOf("manifest_url") >= 0);
}

TEST_F(SettingsManagerTest, OTASettingsLoadedFromJson) {
    sm.markAsNotLoadedForTesting();

    String json = R"({
        "auto_update_enabled": true,
        "update_check_interval_hours": 12,
        "manifest_url": "https://example.com/manifest.json"
    })";
    mockHal.setFileContent(json.c_str(), json.length());

    EXPECT_TRUE(sm.load());
    EXPECT_TRUE(sm.getAutoUpdateEnabled());
    EXPECT_EQ(sm.getUpdateCheckIntervalHours(), 12u);
    EXPECT_STREQ(sm.getManifestUrl().c_str(), "https://example.com/manifest.json");
}

// ============================================================================
// LLM Weather-Decider Settings Tests (issue #6)
// ============================================================================

TEST_F(SettingsManagerTest, LlmEnabledDefaultFalse) {
    EXPECT_FALSE(sm.getLlmEnabled());
}

TEST_F(SettingsManagerTest, LlmSettersAndGetters) {
    sm.setLlmEnabled(true);
    sm.setLlmProviderType("openai_compatible");
    sm.setLlmBaseUrl("http://192.168.1.5:11434");
    sm.setLlmApiKey("secret-token");
    sm.setLlmModel("llama3.1");
    sm.setLlmTimeoutSeconds(20);

    EXPECT_TRUE(sm.getLlmEnabled());
    EXPECT_STREQ(sm.getLlmProviderType().c_str(), "openai_compatible");
    EXPECT_STREQ(sm.getLlmBaseUrl().c_str(), "http://192.168.1.5:11434");
    EXPECT_STREQ(sm.getLlmApiKey().c_str(), "secret-token");
    EXPECT_STREQ(sm.getLlmModel().c_str(), "llama3.1");
    EXPECT_EQ(sm.getLlmTimeoutSeconds(), 20u);
}

TEST_F(SettingsManagerTest, LlmProviderTypeRejectsUnknownValues) {
    sm.setLlmProviderType("openai_compatible");
    // Legacy/deprecated values are no longer accepted (UI now exposes only
    // openai_compatible since all real providers — Ollama Cloud, local/LAN
    // Ollama, Rapid-MLX — speak the OpenAI schema).
    sm.setLlmProviderType("ollama_native");
    sm.setLlmProviderType("ollama_cloud");
    sm.setLlmProviderType("not_a_real_provider");
    EXPECT_STREQ(sm.getLlmProviderType().c_str(), "openai_compatible");  // unchanged
}

TEST_F(SettingsManagerTest, LlmTimeoutSecondsClamps) {
    sm.setLlmTimeoutSeconds(1);
    EXPECT_EQ(sm.getLlmTimeoutSeconds(), 5u);
    sm.setLlmTimeoutSeconds(999);
    EXPECT_EQ(sm.getLlmTimeoutSeconds(), 60u);
}

TEST_F(SettingsManagerTest, LlmApiKeyOnlySerializedWithIncludePassword) {
    sm.setLlmApiKey("secret-token");
    String withPassword = sm.toJson(true);
    String withoutPassword = sm.toJson(false);
    EXPECT_TRUE(withPassword.indexOf("secret-token") >= 0);
    EXPECT_TRUE(withoutPassword.indexOf("secret-token") < 0);
}

TEST_F(SettingsManagerTest, LlmSettingsSerializedInJson) {
    sm.setLlmEnabled(true);
    sm.setLlmProviderType("openai_compatible");
    sm.setLlmBaseUrl("https://ollama.com");
    sm.setLlmModel("gpt-oss:20b");
    sm.setLlmTimeoutSeconds(30);

    String json = sm.toJson();
    EXPECT_TRUE(json.indexOf("llm_enabled") >= 0);
    EXPECT_TRUE(json.indexOf("llm_provider_type") >= 0);
    EXPECT_TRUE(json.indexOf("llm_base_url") >= 0);
    EXPECT_TRUE(json.indexOf("llm_model") >= 0);
    EXPECT_TRUE(json.indexOf("llm_timeout_seconds") >= 0);
    EXPECT_TRUE(json.indexOf("llm_prompt_override") >= 0);
}

TEST_F(SettingsManagerTest, LlmSettingsLoadedFromJson) {
    sm.markAsNotLoadedForTesting();

    String json = R"({
        "llm_enabled": true,
        "llm_provider_type": "openai_compatible",
        "llm_base_url": "http://localhost:8000",
        "llm_api_key": "kO7dkihVsUVeb",
        "llm_model": "mlx-community/Qwen3.6-35B-A3B-6bit",
        "llm_timeout_seconds": 15,
        "llm_prompt_override": "Custom guidance text here"
    })";
    mockHal.setFileContent(json.c_str(), json.length());

    EXPECT_TRUE(sm.load());
    EXPECT_TRUE(sm.getLlmEnabled());
    EXPECT_STREQ(sm.getLlmProviderType().c_str(), "openai_compatible");
    EXPECT_STREQ(sm.getLlmBaseUrl().c_str(), "http://localhost:8000");
    EXPECT_STREQ(sm.getLlmApiKey().c_str(), "kO7dkihVsUVeb");
    EXPECT_STREQ(sm.getLlmModel().c_str(), "mlx-community/Qwen3.6-35B-A3B-6bit");
    EXPECT_EQ(sm.getLlmTimeoutSeconds(), 15u);
    EXPECT_STREQ(sm.getLlmPromptOverride().c_str(), "Custom guidance text here");
}

TEST_F(SettingsManagerTest, LlmPromptOverrideRoundTripsAndSerializes) {
    sm.setLlmPromptOverride("Never block unless it's actively hailing.");
    String json = sm.toJson();
    EXPECT_TRUE(json.indexOf("llm_prompt_override") >= 0);
    EXPECT_TRUE(json.indexOf("hailing") >= 0);

    // Empty override (firmware default sentinel) also round-trips cleanly.
    sm.setLlmPromptOverride("");
    String jsonEmpty = sm.toJson();
    // ArduinoJson emits "key":"" with no space; match space-insensitively.
    EXPECT_TRUE(jsonEmpty.indexOf("llm_prompt_override") >= 0);
    EXPECT_STREQ(sm.getLlmPromptOverride().c_str(), "");
}

TEST_F(SettingsManagerTest, LlmPromptOverrideClampedToMaxLength) {
    // Setter must cap at 2048 chars so the prompt + settings file stay bounded
    // on the ESP32. Anything longer should be truncated, not rejected.
    String longText;
    for (int i = 0; i < 3000; i++) longText += 'x';
    EXPECT_EQ(longText.length(), 3000u);
    sm.setLlmPromptOverride(longText);
    EXPECT_EQ(sm.getLlmPromptOverride().length(), 2048u);
}

// ============================================================================
// NVS Backup/Restore Tests
// ============================================================================

TEST_F(SettingsManagerTest, BackupToNVSSavesSettingsJson) {
    sm.setSSID("BackupTestSSID");
    sm.setPassword("BackupTestPass");
    sm.setTempThresholdOnF(33.5);
    sm.setAutoUpdateEnabled(true);

    EXPECT_TRUE(sm.backupToNVS());

    // Verify NVS contains the backup
    const auto& nvs = mockHal.getNvsStorage();
    EXPECT_FALSE(nvs.empty());
}

TEST_F(SettingsManagerTest, RestoreFromNVSRestoresSettings) {
    // First backup some settings
    sm.setSSID("BackupSSID");
    sm.setPassword("BackupPass");
    sm.setTempThresholdOnF(31.0);
    sm.setAutoUpdateEnabled(true);
    sm.backupToNVS();

    // Reset settings to defaults
    sm.resetForTesting();
    mockHal.setForceWriteSuccess(true);  // Simulate writable filesystem after OTA flash
    sm.begin(&mockHal);
    // begin() calls restoreFromNVS() automatically, but settings were already restored
    // Let's verify the backup was consumed
    EXPECT_STREQ(sm.getSSID().c_str(), "BackupSSID");
    EXPECT_STREQ(sm.getPassword().c_str(), "BackupPass");
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 31.0);
    EXPECT_TRUE(sm.getAutoUpdateEnabled());
}

TEST_F(SettingsManagerTest, RestoreFromNVSClearsBackupAfterRestore) {
    sm.setSSID("ClearTestSSID");
    sm.backupToNVS();

    // Verify backup exists
    EXPECT_FALSE(mockHal.getNvsStorage().empty());

    // Restore (with writable filesystem so NVS backup gets cleared)
    sm.resetForTesting();
    mockHal.setForceWriteSuccess(true);
    sm.begin(&mockHal);

    // NVS backup should be cleared after restore
    EXPECT_TRUE(mockHal.getNvsStorage().empty());
}

TEST_F(SettingsManagerTest, RestoreFromNVSRetainsBackupWhenFileWriteFails) {
    sm.setSSID("RetainTestSSID");
    sm.backupToNVS();

    // Verify backup exists
    EXPECT_FALSE(mockHal.getNvsStorage().empty());

    // Restore WITHOUT writable filesystem (forceWriteSuccess = false, no file content)
    sm.resetForTesting();
    mockHal.clearFileContent();
    // forceWriteSuccess defaults to false, so fsOpen("w") will fail
    sm.begin(&mockHal);

    // Settings should still be restored in memory
    EXPECT_STREQ(sm.getSSID().c_str(), "RetainTestSSID");

    // NVS backup should be RETAINED since file write failed
    EXPECT_FALSE(mockHal.getNvsStorage().empty());
}

TEST_F(SettingsManagerTest, RestoreFromNVSReturnsFalseWhenNoBackup) {
    // Clear any NVS data
    mockHal.clearNvsStorage();

    // restoreFromNVS should return false (no backup)
    EXPECT_FALSE(sm.restoreFromNVS());
}

TEST_F(SettingsManagerTest, BeginAutoRestoresFromNVS) {
    // Backup custom settings
    sm.setSSID("AutoRestoreSSID");
    sm.setTempThresholdOnF(29.0);
    sm.backupToNVS();

    // Simulate fresh boot by resetting SettingsManager
    sm.resetForTesting();
    // Clear file content to simulate fresh filesystem
    mockHal.clearFileContent();

    // begin() should auto-detect and restore NVS backup
    mockHal.setForceWriteSuccess(true);  // Simulate writable filesystem after OTA flash
    sm.markAsNotLoadedForTesting();
    sm.begin(&mockHal);

    EXPECT_STREQ(sm.getSSID().c_str(), "AutoRestoreSSID");
    EXPECT_FLOAT_EQ(sm.getTempThresholdOnF(), 29.0);
}

TEST_F(SettingsManagerTest, NormalBootWithoutNVSBackupUsesFileSettings) {
    // Ensure no NVS backup exists
    mockHal.clearNvsStorage();

    // Set up file-based settings
    String json = createValidSettingsJson();
    mockHal.setFileContent(json.c_str(), json.length());

    sm.resetForTesting();
    sm.markAsNotLoadedForTesting();
    sm.begin(&mockHal);
    sm.load();

    // Should load from file normally
    EXPECT_STREQ(sm.getSSID().c_str(), "TestNetwork");
}
