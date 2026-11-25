#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include "IHAL.h"
#include "MockHAL.h"
#include "HistoricalDataManager.h"
#include "Logger.h"
#include <math.h>
#include <string.h>

using namespace fakeit;

class HistoricalDataManagerTest : public ::testing::Test {
protected:
    MockHAL mockHal;
    HistoricalDataManager mgr;

    void SetUp() override {
        ArduinoFakeReset();
        mockHal.reset();
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal.millis(); });
        When(Method(ArduinoFake(), micros)).AlwaysReturn(0);
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        logger.begin(&mockHal);
        mgr.begin(true, 200, 60, 10);
    }
};

TEST_F(HistoricalDataManagerTest, InitialStateAndCapacity) {
    EXPECT_TRUE(mgr.isEnabled());
    EXPECT_EQ(mgr.getDataPointCount(), 0u);
    EXPECT_EQ(mgr.getBufferCapacity(), 200u);
    EXPECT_EQ(mgr.getTempMinInterval(), 60u);
    EXPECT_EQ(mgr.getFlowMinInterval(), 10u);
}

TEST_F(HistoricalDataManagerTest, RoundTripsAllFieldsAndEnums) {
    mockHal.setMillis(100000UL);

    // First update records initial point
    mgr.checkAndRecord(35.25f, true, 1.375f, 75,
                       "OPENING", "OPEN",
                       "temp_threshold", "sunrise", "web");

    EXPECT_EQ(mgr.getDataPointCount(), 1u);
    DataPoint dp;
    ASSERT_TRUE(mgr.getOrderedDataPoint(0, dp));
    EXPECT_NEAR(dp.temperature_f, 35.25f, 0.05f);
    EXPECT_TRUE(dp.pump_active);
    EXPECT_NEAR(dp.flow_rate, 1.375f, 0.005f);
    EXPECT_EQ(dp.light_brightness, 75);
    EXPECT_STREQ(dp.door_state, "OPENING");
    EXPECT_STREQ(dp.door_position, "OPEN");
    EXPECT_STREQ(dp.pump_trigger, "temp_threshold");
    EXPECT_STREQ(dp.door_trigger, "sunrise");
    EXPECT_STREQ(dp.light_trigger, "web");
    EXPECT_STREQ(dp.event_type, "temp");
}

TEST_F(HistoricalDataManagerTest, PreservesNaNTemperature) {
    mockHal.setMillis(100000UL);
    mgr.checkAndRecord(NAN, false, 0.0f, 0,
                       "IDLE", "CLOSED",
                       "STARTUP", "STARTUP", "STARTUP");

    DataPoint dp;
    ASSERT_TRUE(mgr.getOrderedDataPoint(0, dp));
    EXPECT_TRUE(isnan(dp.temperature_f));
    EXPECT_FALSE(dp.pump_active);
    EXPECT_STREQ(dp.door_state, "IDLE");
    EXPECT_STREQ(dp.door_position, "CLOSED");
}

TEST_F(HistoricalDataManagerTest, CircularWrapAcrossMultipleChunksPreservesOrder) {
    mgr.setBufferSize(100);
    // Record 250 distinct pump-toggle events spanning >7 32-entry chunks
    for (int i = 0; i < 250; i++) {
        mockHal.setMillis(100000UL + (unsigned long)i * 1000UL);
        bool pumpOn = (i % 2) == 0;
        mgr.checkAndRecord(30.0f + (i % 50) * 0.5f, pumpOn, 0.5f, (uint8_t)(i % 101),
                           "IDLE", pumpOn ? "OPEN" : "CLOSED",
                           "MANUAL", "TIMER", "API");
    }

    EXPECT_EQ(mgr.getDataPointCount(), 100u);

    DataPoint first, last;
    ASSERT_TRUE(mgr.getOrderedDataPoint(0, first));
    ASSERT_TRUE(mgr.getOrderedDataPoint(99, last));
    // Oldest of the last 100 is i=150 (light_brightness = 150 % 101 = 49)
    // Newest is i=249 (light_brightness = 249 % 101 = 47)
    EXPECT_EQ(first.light_brightness, (uint8_t)(150 % 101));
    EXPECT_EQ(last.light_brightness, (uint8_t)(249 % 101));

    // Out of bounds returns false cleanly (no crash / divide-by-zero)
    DataPoint oob;
    EXPECT_FALSE(mgr.getOrderedDataPoint(100, oob));
    EXPECT_FALSE(mgr.getOrderedDataPoint(999, oob));
}

TEST_F(HistoricalDataManagerTest, GetOrderedDataPointSafeAfterClear) {
    mockHal.setMillis(100000UL);
    mgr.checkAndRecord(50.0f, true, 1.0f, 50,
                       "IDLE", "OPEN", "MANUAL", "MANUAL", "MANUAL");
    EXPECT_EQ(mgr.getDataPointCount(), 1u);

    mgr.clear();
    EXPECT_EQ(mgr.getDataPointCount(), 0u);

    // Must not divide by zero or return stale data when empty
    DataPoint dp;
    EXPECT_FALSE(mgr.getOrderedDataPoint(0, dp));
    EXPECT_EQ(mgr.getOrderedIndex(0), 0u);
    EXPECT_EQ(mgr.getDataAsJson(), String("[]"));
}

TEST_F(HistoricalDataManagerTest, ResizeShrinkKeepsMostRecentPoints) {
    mgr.setBufferSize(100);
    for (int i = 0; i < 80; i++) {
        mockHal.setMillis(100000UL + (unsigned long)i * 1000UL);
        mgr.checkAndRecord(40.0f, (i % 2) == 0, 0.0f, (uint8_t)i,
                           "IDLE", "CLOSED", "STARTUP", "STARTUP", "STARTUP");
    }
    EXPECT_EQ(mgr.getDataPointCount(), 80u);

    // Shrink buffer to 20 — should keep the 20 most recent (i = 60..79)
    mgr.setBufferSize(20);
    EXPECT_EQ(mgr.getDataPointCount(), 20u);
    DataPoint oldest, newest;
    ASSERT_TRUE(mgr.getOrderedDataPoint(0, oldest));
    ASSERT_TRUE(mgr.getOrderedDataPoint(19, newest));
    EXPECT_EQ(oldest.light_brightness, 60);
    EXPECT_EQ(newest.light_brightness, 79);
}

TEST_F(HistoricalDataManagerTest, EventDetectionSkipsNoOpAndCapturesStateChanges) {
    mockHal.setMillis(100000UL);

    // First call records initial point
    mgr.checkAndRecord(50.0f, false, 0.0f, 0, "IDLE", "CLOSED", "STARTUP", "STARTUP", "STARTUP");
    EXPECT_EQ(mgr.getDataPointCount(), 1u);

    // Sub-threshold temp change with no state change -> not recorded
    mockHal.setMillis(101000UL);
    mgr.checkAndRecord(50.1f, false, 0.0f, 0, "IDLE", "CLOSED", "STARTUP", "STARTUP", "STARTUP");
    EXPECT_EQ(mgr.getDataPointCount(), 1u);

    // Pump state change -> recorded immediately with event_type="pump"
    mockHal.setMillis(102000UL);
    mgr.checkAndRecord(50.1f, true, 0.0f, 0, "IDLE", "CLOSED", "web", "startup", "startup");
    EXPECT_EQ(mgr.getDataPointCount(), 2u);

    DataPoint dp;
    ASSERT_TRUE(mgr.getOrderedDataPoint(1, dp));
    EXPECT_STREQ(dp.event_type, "pump");
    EXPECT_TRUE(dp.pump_active);
    EXPECT_STREQ(dp.pump_trigger, "web");

    // Verify JSON & CSV exports contain the recorded event
    String json = mgr.getDataAsJson();
    EXPECT_NE(json.indexOf("\"pump_trigger\":\"web\""), -1);
    String csv = mgr.getDataAsCsv();
    EXPECT_NE(csv.indexOf(",web,"), -1);
}
