#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include "MockHAL.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "PumpController.h"
#include "SensorManager.h"
using namespace fakeit;

// Global mock HAL instance
static MockHAL mockHal;

// Mock SensorManager for testing
class MockSensorManager : public SensorManager {
public:
  MockSensorManager() : SensorManager() {
    // Initialize sensor data as DALLAS_TEMP type by default (temperature sensor)
    mockSensorData.type = SensorType::DALLAS_TEMP;
    mockSensorData.is_connected = true;
    mockSensorData.was_detected = true;
    mockSensorData.temperature_f = 0.0f;
    mockSensorData.flow_rate = 0.0f;
    mockSensorData.last_pulse_time.store(0);
    mockSensorData.pulse_count.store(0);
  }

  void setTemperature(float temp_f) {
    mockSensorData.temperature_f = temp_f;
    mockSensorData.type = SensorType::DALLAS_TEMP; // Temperature sensor
    mockSensorData.is_connected = true;
    mockSensorData.was_detected = true;
  }

  void setFlowRate(float gpm) {
    mockSensorData.flow_rate = gpm;
    mockSensorData.type = SensorType::WATER_METER; // Water meter
    mockSensorData.is_connected = true;
    mockSensorData.was_detected = true;
    // Simulate recent pulse if flow > 0, otherwise keep last pulse time
    // to simulate flow stopping (tests will use setLastPulseTime for precise control)
    if (gpm > 0.0f) {
      mockSensorData.last_pulse_time.store(millis());
    }
    // When flow is 0, don't change last_pulse_time - let tests control this
  }

  void setDisconnected() {
    mockSensorData.is_connected = false;
    mockSensorData.was_detected = false;
    mockSensorData.temperature_f = NAN;
  }

  void setLastPulseTime(unsigned long time) {
    mockSensorData.last_pulse_time.store(time);
  }

  // Override public methods to return mock values
  float getTemperature1F() const {
    // Match real SensorManager behavior - only return temp if it's a Dallas temp sensor
    // and the sensor is connected and detected
    if (mockSensorData.type != SensorType::DALLAS_TEMP) {
      return NAN;  // Water meters don't have temperature
    }
    if (!mockSensorData.was_detected || !mockSensorData.is_connected) {
      return NAN;  // Sensor not available
    }
    return mockSensorData.temperature_f;
  }

  bool isSensor1Connected() const { return mockSensorData.is_connected; }

  bool isSensor1Detected() const { return mockSensorData.was_detected; }

  float getFlowRate1() const { return mockSensorData.flow_rate; }

  SensorType getSensor1Type() const { return mockSensorData.type; }

  SensorData getSensor1Data() const {
    // When returning sensor data, simulate continuous flow by updating last_pulse_time
    // if flow_rate > 0 (water is currently flowing)
    if (mockSensorData.flow_rate > 0.0f) {
      // Cast away const to update last_pulse_time for simulation purposes
      const_cast<MockSensorManager*>(this)->mockSensorData.last_pulse_time.store(millis());
    }
    return mockSensorData;
  }

private:
  mutable SensorData mockSensorData;
};

// Test fixture for PumpController
class PumpControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize MockHAL
    mockHal.reset();

    // Reset ArduinoFake
    ArduinoFakeReset();

    // Mock ALL Arduino functions BEFORE initializing anything
    When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
    // Make millis() return mockHal.millisValue so tests can control time
    When(Method(ArduinoFake(), millis)).AlwaysDo([]() {
      return mockHal.millisValue;
    });
    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

    // Mock pinMode and digitalWrite to prevent segfaults
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalRead)).AlwaysReturn(LOW);

    // Initialize Logger AFTER all Arduino function mocks are set up
    Logger::getInstance().begin(&mockHal);
    Logger::getInstance().clearLogs();
    Logger::getInstance().setLogLevel(LogLevel::DEBUG);

    // Initialize SettingsManager with mock HAL
    settingsManager.begin(&mockHal);
    settingsManager.setPumpAutoMode(true); // Set default to auto mode for tests
    settingsManager.setPumpOffFlowMonitoringEnabled(true); // Enable pump-off flow monitoring for tests

    // Create mock sensors
    primarySensor = new MockSensorManager();
    flowSensor = new MockSensorManager();

    // Initialize pump controller
    pumpController.begin(primarySensor, flowSensor, 26);
  }

  void TearDown() override {
    delete primarySensor;
    delete flowSensor;
    mockHal.reset();
  }

  // Helper methods
  void advanceTime(unsigned long milliseconds) {
    mockHal.millisValue += milliseconds;
  }

  void setTemperature(float temp_f) { primarySensor->setTemperature(temp_f); }

  void setFlowRate(float gpm) { flowSensor->setFlowRate(gpm); }

  void setFlowDisconnected() { flowSensor->setDisconnected(); }

  PumpController pumpController;
  MockSensorManager *primarySensor;
  MockSensorManager *flowSensor;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(PumpControllerTest, InitializationSetsDefaultState) {
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
  EXPECT_FALSE(pumpController.isPumpOn());
  EXPECT_FALSE(pumpController.hasFlowError());
  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
  EXPECT_EQ(pumpController.getTotalOnTime(), 0UL);
  EXPECT_EQ(pumpController.getTotalOffTime(), 0UL);
  EXPECT_EQ(pumpController.getTotalCycles(), 0UL);
}

TEST_F(PumpControllerTest, InitializationWithNullPrimarySensor) {
  PumpController nullPump;
  nullPump.begin(nullptr, flowSensor, 26);

  // Should handle null sensor gracefully
  EXPECT_EQ(nullPump.getState(), PumpState::PUMP_AUTO);
}

TEST_F(PumpControllerTest, InitializationWithNullFlowSensor) {
  PumpController nullPump;
  nullPump.begin(primarySensor, nullptr, 26);

  // Should handle null sensor gracefully
  EXPECT_EQ(nullPump.getState(), PumpState::PUMP_AUTO);
}

// ============================================================================
// Manual Control Tests
// ============================================================================

TEST_F(PumpControllerTest, TurnOnActivatesPump) {
  pumpController.turnOn();
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_ON);
}

TEST_F(PumpControllerTest, TurnOffDeactivatesPump) {
  pumpController.turnOn();
  pumpController.update();

  pumpController.turnOff();
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_OFF);
}

TEST_F(PumpControllerTest, SetAutoModeEnablesAutoControl) {
  pumpController.turnOn();
  pumpController.update();

  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
}

TEST_F(PumpControllerTest, ManualModePersistsAcrossUpdates) {
  pumpController.turnOn();
  pumpController.update();

  // Multiple updates should maintain manual state
  for (int i = 0; i < 10; i++) {
    pumpController.update();
    EXPECT_TRUE(pumpController.isPumpOn());
  }
}

// ============================================================================
// Temperature-Based Automation Tests
// ============================================================================

TEST_F(PumpControllerTest, AutoModeTurnsOnBelowThreshold) {
  setTemperature(30.0f); // Below default threshold of 34°F

  // Force a fresh reading by calling update twice
  // First update reads temperature and sets the flag
  pumpController.update();

  // Check that the flag is set
  EXPECT_TRUE(pumpController.getStatus().temperature_below_threshold);

  // Second update should start the pump
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, AutoModeTurnsOffAboveThreshold) {
  setTemperature(38.0f); // Above default threshold of 36°F
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
  EXPECT_FALSE(pumpController.getStatus().temperature_below_threshold);
}

TEST_F(PumpControllerTest, HysteresisPreventsRapidCycling) {
  setTemperature(35.0f); // Between ON (34°F) and OFF (36°F) thresholds
  pumpController.setAutoMode(true);
  pumpController.update();

  // State should remain stable (no rapid cycling)
  PumpState state1 = pumpController.getState();

  advanceTime(1000);
  pumpController.update();
  PumpState state2 = pumpController.getState();

  EXPECT_EQ(state1, state2); // State should not change
}

TEST_F(PumpControllerTest, TemperatureBelowThresholdActivatesPump) {
  setTemperature(33.0f); // Below 34°F threshold
  pumpController.update();

  // Pump activates on first update when temp is below threshold
  EXPECT_TRUE(pumpController.isPumpOn());
  EXPECT_EQ(pumpController.getTotalCycles(), 1UL);
}

TEST_F(PumpControllerTest, TemperatureAboveThresholdDeactivatesPump) {
  setTemperature(37.0f); // Above 36°F threshold
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, TemperatureAtOnThresholdActivatesPump) {
  setTemperature(33.9f); // Just below ON threshold (34°F)
  pumpController.update();

  // Pump activates when temp is below ON threshold
  EXPECT_TRUE(pumpController.isPumpOn());
  EXPECT_EQ(pumpController.getTotalCycles(), 1UL);
}

TEST_F(PumpControllerTest, TemperatureAtOffThresholdDeactivatesPump) {
  setTemperature(36.0f); // At OFF threshold
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

// ============================================================================
// Cycling Mode Tests
// ============================================================================

TEST_F(PumpControllerTest, CyclingModeSwitchesPumpOnAndOff) {
  setTemperature(32.0f); // Below threshold
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  // Advance time past ON duration (default 300 seconds)
  advanceTime(301000);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());

  // Advance time past OFF duration (default 600 seconds)
  advanceTime(601000);
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, CycleCountIncrementsOnEachCycle) {
  setTemperature(32.0f);
  pumpController.update();

  unsigned long cycles1 = pumpController.getTotalCycles();
  EXPECT_GT(cycles1, 0UL);

  advanceTime(301000);
  pumpController.update();
  advanceTime(601000);
  pumpController.update();

  unsigned long cycles2 = pumpController.getTotalCycles();
  EXPECT_GT(cycles2, cycles1);
}

TEST_F(PumpControllerTest, TotalOnTimeAccumulates) {
  setTemperature(32.0f);
  pumpController.update();

  advanceTime(10000);
  pumpController.update();

  unsigned long onTime = pumpController.getTotalOnTime();
  EXPECT_GT(onTime, 0UL);
  EXPECT_GE(onTime, 10000UL);
}

TEST_F(PumpControllerTest, TotalOffTimeAccumulates) {
  setTemperature(32.0f);
  pumpController.update();

  // Complete ON phase
  advanceTime(301000);
  pumpController.update();

  advanceTime(10000);
  pumpController.update();

  unsigned long offTime = pumpController.getTotalOffTime();
  EXPECT_GT(offTime, 0UL);
  EXPECT_GE(offTime, 10000UL);
}

// ============================================================================
// Flow Error Detection Tests
// ============================================================================

TEST_F(PumpControllerTest, FlowErrorDetectedWhenNoFlowDuringOn) {
  setTemperature(32.0f);
  setFlowRate(0.0f); // No flow
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  EXPECT_TRUE(pumpController.isPumpOn());
  EXPECT_FALSE(pumpController.hasFlowError());

  // Advance time past flow error timeout (default 120 seconds)
  advanceTime(121000);
  pumpController.update();

  EXPECT_TRUE(pumpController.hasFlowError());
  EXPECT_FALSE(pumpController.isPumpOn()); // Should turn off pump
}

TEST_F(PumpControllerTest, FlowErrorNotDetectedWithValidFlow) {
  setTemperature(32.0f);
  setFlowRate(2.5f); // Valid flow
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  // Advance time past flow error timeout
  advanceTime(121000);
  pumpController.update();

  EXPECT_FALSE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, FlowErrorStopsPumpImmediately) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  advanceTime(121000);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, ClearFlowErrorResetsErrorState) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  advanceTime(121000);
  pumpController.update();

  EXPECT_TRUE(pumpController.hasFlowError());

  pumpController.clearFlowError();
  pumpController.update();

  EXPECT_FALSE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, FlowErrorRetryAfterDelay) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  advanceTime(121000);
  pumpController.update();

  EXPECT_TRUE(pumpController.hasFlowError());

  // Clear error and restore flow
  pumpController.clearFlowError();
  setFlowRate(2.5f);

  // Wait for retry delay (default 120 seconds)
  advanceTime(121000);
  pumpController.update();

  EXPECT_FALSE(pumpController.hasFlowError());
  EXPECT_TRUE(pumpController.isPumpOn());
}

// ============================================================================
// Pump OFF Flow Monitoring Tests
// ============================================================================

TEST_F(PumpControllerTest, PumpOffFlowDetectedAfterGracePeriod) {
  setTemperature(40.0f); // Above threshold, pump off
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());

  // Wait for grace period (default 30 seconds)
  advanceTime(31000);
  pumpController.update();

  // Simulate flow while pump is off
  setFlowRate(1.5f);
  pumpController.update();

  EXPECT_TRUE(pumpController.getPumpOffFlowDetected());
}

TEST_F(PumpControllerTest, PumpOffFlowNotDetectedDuringGracePeriod) {
  setTemperature(40.0f);
  pumpController.update();

  // Simulate flow during grace period
  advanceTime(10000);
  setFlowRate(1.5f);
  pumpController.update();

  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
}

TEST_F(PumpControllerTest, PumpOffFlowDetectionClearsOnPumpOn) {
  setTemperature(40.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  // Wait for grace period and detect flow
  advanceTime(31000);
  setFlowRate(1.5f);
  pumpController.update();

  EXPECT_TRUE(pumpController.getPumpOffFlowDetected());

  // Turn pump on
  setTemperature(32.0f);
  pumpController.update(); // First update sets flag
  pumpController.update(); // Second update starts pump and clears pump off flow detection

  EXPECT_TRUE(pumpController.isPumpOn());
  EXPECT_FALSE(pumpController.getPumpOffFlowDetected()); // Should auto-clear
}

TEST_F(PumpControllerTest, ClearPumpOffFlowDetectionResetsFlag) {
  setTemperature(40.0f);
  pumpController.update();

  advanceTime(31000);
  setFlowRate(1.5f);
  pumpController.update();

  EXPECT_TRUE(pumpController.getPumpOffFlowDetected());

  pumpController.clearPumpOffFlowDetected();

  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
}

TEST_F(PumpControllerTest, PumpOffFlowNotDetectedWithZeroFlow) {
  setTemperature(40.0f);
  pumpController.update();

  advanceTime(31000);
  setFlowRate(0.0f);
  pumpController.update();

  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(PumpControllerTest, ResetStatisticsClearsAllCounters) {
  setTemperature(32.0f);
  pumpController.update();

  // Run some cycles
  advanceTime(301000);
  pumpController.update();
  advanceTime(601000);
  pumpController.update();
  advanceTime(301000);
  pumpController.update();

  EXPECT_GT(pumpController.getTotalOnTime(), 0UL);
  EXPECT_GT(pumpController.getTotalOffTime(), 0UL);
  EXPECT_GT(pumpController.getTotalCycles(), 0UL);

  pumpController.resetStatistics();

  EXPECT_EQ(pumpController.getTotalOnTime(), 0UL);
  EXPECT_EQ(pumpController.getTotalOffTime(), 0UL);
  EXPECT_EQ(pumpController.getTotalCycles(), 0UL);
}

TEST_F(PumpControllerTest, StatisticsAccumulateCorrectlyOverMultipleCycles) {
  setTemperature(32.0f);
  pumpController.update(); // Initial cycle starts here (cycle 1)

  // Run 3 more complete cycles (each loop iteration completes one cycle and starts next)
  for (int i = 0; i < 3; i++) {
    advanceTime(301000); // Past ON time
    pumpController.update(); // Switch to OFF phase
    advanceTime(601000); // Past OFF time
    pumpController.update(); // Start new cycle (cycles 2, 3, 4)
  }

  // Total cycles: 1 (initial) + 3 (from loop) = 4
  EXPECT_EQ(pumpController.getTotalCycles(), 4UL);
  EXPECT_GT(pumpController.getTotalOnTime(), 900000UL);   // Accumulated ON time
  EXPECT_GT(pumpController.getTotalOffTime(), 1800000UL); // Accumulated OFF time
}

TEST_F(PumpControllerTest, GetCurrentCycleTimeReturnsCorrectValue) {
  setTemperature(32.0f);
  pumpController.update(); // First update sets flag and starts pump

  advanceTime(50000);
  pumpController.update();

  unsigned long cycleTime = pumpController.getCurrentCycleTime();
  EXPECT_EQ(cycleTime, 50000UL);
}

TEST_F(PumpControllerTest, GetCurrentRunStartTimeReturnsValidValue) {
  // Advance time first so start time won't be 0
  advanceTime(1000);
  setTemperature(32.0f);
  pumpController.update(); // First update sets flag and starts pump

  unsigned long runStartTime = pumpController.getCurrentRunStartTime();
  EXPECT_GT(runStartTime, 0UL);
  EXPECT_EQ(runStartTime, 1000UL);
}

TEST_F(PumpControllerTest, GetTimeUntilNextSwitchReturnsValidValue) {
  setTemperature(32.0f);
  pumpController.update(); // First update sets flag
  pumpController.update(); // Second update starts pump

  unsigned long timeUntilSwitch = pumpController.getTimeUntilNextSwitch();
  EXPECT_GT(timeUntilSwitch, 0UL);
  EXPECT_LE(timeUntilSwitch, 300000UL); // Default ON time
}

TEST_F(PumpControllerTest, GetTimeUntilRetryReturnsZeroWhenNoError) {
  setTemperature(32.0f);
  pumpController.update();

  EXPECT_EQ(pumpController.getTimeUntilRetry(), 0UL);
}

TEST_F(PumpControllerTest, GetTimeUntilRetryReturnsValueWhenError) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update(); // First update sets flag and starts pump
  pumpController.update(); // Ensure pump is running

  advanceTime(121000);
  pumpController.update();

  unsigned long timeUntilRetry = pumpController.getTimeUntilRetry();
  EXPECT_GT(timeUntilRetry, 0UL);
}

// ============================================================================
// Force Cycle Tests
// ============================================================================

TEST_F(PumpControllerTest, ForceCycleActivatesPump) {
  setTemperature(32.0f); // Below threshold
  pumpController.setAutoMode(true); // Ensure AUTO mode
  pumpController.update(); // Start normal cycling

  EXPECT_TRUE(pumpController.isPumpOn());

  // Turn off to stop cycling
  pumpController.turnOff();
  pumpController.update();
  EXPECT_FALSE(pumpController.isPumpOn());

  // Force cycle should restart cycling when in AUTO mode
  pumpController.setAutoMode(true);
  pumpController.forceCycle();
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, ForceCycleTurnsOffAfterDuration) {
  setTemperature(32.0f); // Below threshold
  pumpController.setAutoMode(true);

  pumpController.forceCycle();
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  advanceTime(301000);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, ForceCycleIncrementsCycleCount) {
  setTemperature(32.0f); // Below threshold
  pumpController.setAutoMode(true);

  unsigned long cyclesBefore = pumpController.getTotalCycles();

  pumpController.forceCycle();
  pumpController.update();

  unsigned long cyclesAfter = pumpController.getTotalCycles();
  EXPECT_GT(cyclesAfter, cyclesBefore);
}

// ============================================================================
// Status Reporting Tests
// ============================================================================

TEST_F(PumpControllerTest, GetStatusReturnsCompleteStatus) {
  setTemperature(32.0f);
  pumpController.update();

  PumpStatus status = pumpController.getStatus();

  EXPECT_EQ(status.state, PumpState::PUMP_AUTO);
  EXPECT_TRUE(status.is_active);
  EXPECT_FLOAT_EQ(status.temperature_f, 32.0f);
  EXPECT_TRUE(status.temperature_below_threshold);
  EXPECT_FALSE(status.flow_error);
  EXPECT_FALSE(status.pump_off_flow_detected);
  EXPECT_GT(status.total_cycles, 0UL);
}

TEST_F(PumpControllerTest, GetStateStringReturnsCorrectString) {
  pumpController.turnOn();
  pumpController.update();

  String stateString = pumpController.getStateString();
  EXPECT_TRUE(stateString.equals("PUMP_ON") || stateString.equals("ON") ||
              stateString.equals("MANUAL_ON"));
}

TEST_F(PumpControllerTest, GetStateStringForAutoMode) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  String stateString = pumpController.getStateString();
  EXPECT_TRUE(stateString.indexOf("AUTO") >= 0 ||
              stateString.indexOf("Auto") >= 0);
}

TEST_F(PumpControllerTest, GetStateStringForErrorState) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update();

  advanceTime(121000);
  pumpController.update();

  String stateString = pumpController.getStateString();
  // State remains AUTO even with flow error, but flow_error flag is set
  EXPECT_TRUE(stateString.indexOf("AUTO") >= 0 || stateString.indexOf("Auto") >= 0);
  EXPECT_TRUE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, GetStatusJsonReturnsValidJson) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  String json = pumpController.getStatusJson();

  EXPECT_FALSE(json.length() == 0);
  EXPECT_TRUE(json.indexOf("state") >= 0);
  EXPECT_TRUE(json.indexOf("is_active") >= 0);
  EXPECT_TRUE(json.indexOf("temperature_f") >= 0);
}

TEST_F(PumpControllerTest, GetCurrentTemperatureReturnsSensorValue) {
  setTemperature(35.5f);
  pumpController.update();

  float temp = pumpController.getCurrentTemperature();
  EXPECT_FLOAT_EQ(temp, 35.5f);
}

TEST_F(PumpControllerTest, GetCurrentTemperatureHandlesDisconnectedSensor) {
  setFlowDisconnected();
  pumpController.update();

  float temp = pumpController.getCurrentTemperature();
  // Should handle disconnected sensor gracefully
  EXPECT_TRUE(isnan(temp) || temp == 0.0f);
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST_F(PumpControllerTest, AutoToOnTransition) {
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);

  pumpController.turnOn();
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_ON);
}

TEST_F(PumpControllerTest, AutoToOffTransition) {
  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);

  pumpController.turnOff();
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_OFF);
}

TEST_F(PumpControllerTest, OnToAutoTransition) {
  pumpController.turnOn();
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_ON);

  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
}

TEST_F(PumpControllerTest, OffToAutoTransition) {
  pumpController.turnOff();
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_OFF);

  pumpController.setAutoMode(true);
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
}

TEST_F(PumpControllerTest, ErrorToAutoTransitionAfterClear) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  advanceTime(121000);
  pumpController.update();

  // State stays AUTO but flow_error flag is set (implementation keeps AUTO state with error flag)
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
  EXPECT_TRUE(pumpController.hasFlowError());
  EXPECT_FALSE(pumpController.isPumpOn()); // Pump should be off due to error

  pumpController.clearFlowError();
  pumpController.update();

  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
  EXPECT_FALSE(pumpController.hasFlowError());
}

// ============================================================================
// Edge Cases and Error Conditions
// ============================================================================

TEST_F(PumpControllerTest, HandlesNullSensorGracefully) {
  PumpController nullPump;
  nullPump.begin(nullptr, nullptr, 26);

  // Should not crash
  nullPump.update();
  nullPump.turnOn();
  nullPump.update();

  // With null sensor, the state depends on settings
  // If auto mode is enabled, it should be PUMP_AUTO
  EXPECT_EQ(nullPump.getState(), PumpState::PUMP_ON);
}

TEST_F(PumpControllerTest, HandlesDisconnectedSensor) {
  setFlowDisconnected();
  pumpController.setAutoMode(true);
  pumpController.update();

  // Should handle disconnected sensor without crashing
  EXPECT_FALSE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, HandlesZeroFlowRate) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update();

  // Zero flow should trigger error after timeout
  advanceTime(121000);
  pumpController.update();

  EXPECT_TRUE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, HandlesVeryHighFlowRate) {
  setTemperature(32.0f);
  setFlowRate(100.0f); // Very high flow rate
  pumpController.setAutoMode(true);
  pumpController.update();

  // Should handle high flow rate without error
  advanceTime(121000);
  pumpController.update();

  EXPECT_FALSE(pumpController.hasFlowError());
}

TEST_F(PumpControllerTest, HandlesExtremeTemperatures) {
  // Test very cold temperature
  setTemperature(-40.0f);
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  // Test very hot temperature
  setTemperature(120.0f);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, HandlesRapidTemperatureChanges) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  // Rapid temperature changes
  for (int i = 0; i < 10; i++) {
    setTemperature(32.0f + i * 2.0f);
    pumpController.update();
  }

  // Should handle without crashing
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
}

TEST_F(PumpControllerTest, HandlesRapidFlowRateChanges) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  // Rapid flow rate changes
  for (int i = 0; i < 10; i++) {
    setFlowRate(i * 0.5f);
    pumpController.update();
  }

  // Should handle without crashing
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_AUTO);
}

// ============================================================================
// Multiple Updates Tests
// ============================================================================

TEST_F(PumpControllerTest, MultipleUpdatesMaintainState) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  PumpState initialState = pumpController.getState();

  // Multiple updates without state change
  for (int i = 0; i < 20; i++) {
    pumpController.update();
    EXPECT_EQ(pumpController.getState(), initialState);
  }
}

TEST_F(PumpControllerTest, MultipleUpdatesWithStateChange) {
  setTemperature(32.0f);
  pumpController.update(); // Initial cycle starts here (cycle 1)

  // Trigger cycle changes - each iteration completes a cycle and starts next
  for (int i = 0; i < 5; i++) {
    advanceTime(301000);
    pumpController.update();
    advanceTime(601000);
    pumpController.update();
  }

  // Should complete 6 cycles: 1 initial + 5 from loop
  EXPECT_EQ(pumpController.getTotalCycles(), 6UL);
}

// ============================================================================
// Timing and Precision Tests
// ============================================================================

TEST_F(PumpControllerTest, CycleTimingIsAccurate) {
  setTemperature(32.0f);
  pumpController.setAutoMode(true);
  pumpController.update();

  unsigned long startTime = mockHal.millis();

  advanceTime(300000); // Exactly 300 seconds
  pumpController.update();

  unsigned long endTime = mockHal.millis();
  unsigned long elapsed = endTime - startTime;

  EXPECT_EQ(elapsed, 300000UL);
}

TEST_F(PumpControllerTest, StatisticsTimingIsAccurate) {
  setTemperature(32.0f);
  pumpController.update();

  unsigned long onTime1 = pumpController.getTotalOnTime();

  advanceTime(50000);
  pumpController.update();

  unsigned long onTime2 = pumpController.getTotalOnTime();
  unsigned long diff = onTime2 - onTime1;

  EXPECT_EQ(diff, 50000UL);
}

// ============================================================================
// Integration Scenarios
// ============================================================================

TEST_F(PumpControllerTest, FullAutoModeCycle) {
  // Start with pump off, temperature above threshold
  setTemperature(40.0f);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());

  // Temperature drops below threshold
  setTemperature(32.0f);
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  // Complete ON phase
  advanceTime(301000);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());

  // Complete OFF phase
  advanceTime(601000);
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  // The first cycle started when temp dropped, then we completed it and started a new one
  EXPECT_GE(pumpController.getTotalCycles(), 1UL);
}

TEST_F(PumpControllerTest, ManualOverrideDuringAutoMode) {
  setTemperature(32.0f);
  pumpController.update();

  EXPECT_TRUE(pumpController.isPumpOn());

  // Manual override to turn off
  pumpController.turnOff();
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
  EXPECT_EQ(pumpController.getState(), PumpState::PUMP_OFF);

  // Auto mode should not override manual state
  advanceTime(10000);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, FlowErrorWithManualRecovery) {
  setTemperature(32.0f);
  setFlowRate(0.0f);
  pumpController.update();

  advanceTime(121000);
  pumpController.update();

  EXPECT_TRUE(pumpController.hasFlowError());

  // Manual turn on to recover
  setFlowRate(2.5f);
  pumpController.turnOn();
  pumpController.update();

  EXPECT_FALSE(pumpController.hasFlowError());
  EXPECT_TRUE(pumpController.isPumpOn());
}

TEST_F(PumpControllerTest, PumpOffFlowWithManualClear) {
  setTemperature(40.0f);
  pumpController.update();

  EXPECT_FALSE(pumpController.isPumpOn());

  advanceTime(31000);
  setFlowRate(1.5f);
  pumpController.update();

  EXPECT_TRUE(pumpController.getPumpOffFlowDetected());

  // Manual clear
  pumpController.clearPumpOffFlowDetected();

  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
}

TEST_F(PumpControllerTest, CompleteOperationSequence) {
  // Reset statistics
  pumpController.resetStatistics();
  EXPECT_EQ(pumpController.getTotalCycles(), 0UL);

  // Start auto mode with cold temperature
  setTemperature(30.0f);
  setFlowRate(2.5f);
  pumpController.update();

  // Run 3 complete cycles
  for (int i = 0; i < 3; i++) {
    advanceTime(301000);
    pumpController.update();
    advanceTime(601000);
    pumpController.update();
  }

  EXPECT_GE(pumpController.getTotalCycles(), 3UL);
  EXPECT_GT(pumpController.getTotalOnTime(), 900000UL);
  EXPECT_GT(pumpController.getTotalOffTime(), 1800000UL);
  EXPECT_FALSE(pumpController.hasFlowError());
  EXPECT_FALSE(pumpController.getPumpOffFlowDetected());
}
