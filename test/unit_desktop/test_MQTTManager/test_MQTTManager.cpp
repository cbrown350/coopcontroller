#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "MQTTManager.h"

using namespace fakeit;

class MQTTManagerTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    MQTTManager mqtt;

    void SetUp() override {
        mockHal = new MockHAL();

        ArduinoFakeReset();
        mockHal->reset();

        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();

        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);
    }

    void TearDown() override {
        delete mockHal;
        mockHal = nullptr;
    }

    MQTTConfig makeTestConfig() {
        MQTTConfig config;
        config.server = "mqtt.example.com";
        config.port = 1883;
        config.username = "testuser";
        config.password = "testpass";
        config.device_id = "coop_test_01";
        config.device_name = "Test Coop";
        config.fw_version = "1.0.0";
        return config;
    }

    MQTTStateData makeTestState() {
        MQTTStateData state;
        state.sensor1_temp_f = 72.5;
        state.sensor2_temp_f = 68.3;
        state.sensor1_connected = true;
        state.sensor2_connected = false;
        state.water_flow_rate = 1.25;
        state.water_total_gallons = 150.75;
        state.pump_running = true;
        state.pump_auto_mode = true;
        state.water_flow_error = false;
        state.door_open = true;
        state.door_closed = false;
        state.door_auto_mode = true;
        state.light_on = true;
        state.light_brightness = 80;
        state.light_auto_mode = false;
        state.temp_threshold_on = 32.0;
        state.temp_threshold_off = 38.0;
        state.wifi_rssi = -55;
        state.free_heap = 120000;
        state.uptime_seconds = 3600;
        return state;
    }
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(MQTTManagerTest, BeginSetsConfig) {
    MQTTConfig config = makeTestConfig();
    mqtt.begin(config);

    const MQTTConfig& stored = mqtt.getConfig();
    EXPECT_EQ(stored.server, "mqtt.example.com");
    EXPECT_EQ(stored.port, 1883u);
    EXPECT_EQ(stored.username, "testuser");
    EXPECT_EQ(stored.password, "testpass");
    EXPECT_EQ(stored.device_id, "coop_test_01");
    EXPECT_EQ(stored.device_name, "Test Coop");
    EXPECT_EQ(stored.fw_version, "1.0.0");
}

TEST_F(MQTTManagerTest, SetConfigUpdatesConfig) {
    MQTTConfig config1 = makeTestConfig();
    mqtt.begin(config1);

    MQTTConfig config2;
    config2.server = "mqtt2.example.com";
    config2.port = 8883;
    config2.username = "newuser";
    config2.password = "newpass";
    config2.device_id = "coop_test_02";
    config2.device_name = "New Coop";
    config2.fw_version = "2.0.0";
    mqtt.setConfig(config2);

    const MQTTConfig& stored = mqtt.getConfig();
    EXPECT_EQ(stored.server, "mqtt2.example.com");
    EXPECT_EQ(stored.port, 8883u);
    EXPECT_EQ(stored.username, "newuser");
    EXPECT_EQ(stored.password, "newpass");
    EXPECT_EQ(stored.device_id, "coop_test_02");
    EXPECT_EQ(stored.device_name, "New Coop");
    EXPECT_EQ(stored.fw_version, "2.0.0");
}

// ============================================================================
// ENABLE/DISABLE TESTS
// ============================================================================

TEST_F(MQTTManagerTest, EnableDisable) {
    EXPECT_FALSE(mqtt.isEnabled());

    mqtt.setEnabled(true);
    EXPECT_TRUE(mqtt.isEnabled());

    mqtt.setEnabled(false);
    EXPECT_FALSE(mqtt.isEnabled());
}

// ============================================================================
// CONNECTION TESTS
// ============================================================================

TEST_F(MQTTManagerTest, IsNotConnectedOnDesktop) {
    MQTTConfig config = makeTestConfig();
    mqtt.begin(config);
    mqtt.setEnabled(true);

    EXPECT_FALSE(mqtt.isConnected());
}

// ============================================================================
// JSON SERIALIZATION TESTS
// ============================================================================

TEST_F(MQTTManagerTest, ToJsonPopulatesFields) {
    MQTTConfig config = makeTestConfig();
    mqtt.begin(config);
    mqtt.setEnabled(true);

    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    mqtt.toJson(json);

    EXPECT_TRUE(json.containsKey("enabled"));
    EXPECT_TRUE(json.containsKey("connected"));
    EXPECT_TRUE(json.containsKey("server"));
    EXPECT_TRUE(json.containsKey("port"));
    EXPECT_TRUE(json.containsKey("device_id"));
    EXPECT_TRUE(json.containsKey("messages_published"));
    EXPECT_TRUE(json.containsKey("messages_failed"));
    EXPECT_TRUE(json.containsKey("reconnect_count"));
    EXPECT_TRUE(json.containsKey("last_error"));

    EXPECT_TRUE(json["enabled"].as<bool>());
    EXPECT_FALSE(json["connected"].as<bool>());
    EXPECT_EQ(json["server"].as<String>(), "mqtt.example.com");
    EXPECT_EQ(json["port"].as<uint16_t>(), 1883u);
    EXPECT_EQ(json["device_id"].as<String>(), "coop_test_01");
    EXPECT_EQ(json["messages_published"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["messages_failed"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["reconnect_count"].as<unsigned int>(), 0u);
}

TEST_F(MQTTManagerTest, ToJsonShowsDisabledByDefault) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    mqtt.toJson(json);

    EXPECT_FALSE(json["enabled"].as<bool>());
    EXPECT_FALSE(json["connected"].as<bool>());
    EXPECT_EQ(json["server"].as<String>(), "");
    EXPECT_EQ(json["port"].as<uint16_t>(), 1883u);
    EXPECT_EQ(json["device_id"].as<String>(), "");
    EXPECT_EQ(json["messages_published"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["messages_failed"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["reconnect_count"].as<unsigned int>(), 0u);
}

// ============================================================================
// STATE PUBLISHING TESTS
// ============================================================================

TEST_F(MQTTManagerTest, PublishStateStoresLastState) {
    MQTTConfig config = makeTestConfig();
    mqtt.begin(config);

    MQTTStateData state = makeTestState();
    mqtt.publishState(state);

    // Publish again with different state to confirm last_state_ was updated
    // We can verify indirectly: the object should not crash and the state
    // data should be stored. Since publishState stores last_state_ before
    // the ESP32 guard, we verify by publishing a second time and checking
    // that the manager remains functional.
    MQTTStateData state2;
    state2.sensor1_temp_f = 99.9;
    state2.pump_running = false;
    state2.light_brightness = 50;
    mqtt.publishState(state2);

    // Verify manager is still functional after storing state
    EXPECT_FALSE(mqtt.isConnected());

    // On desktop, no MQTT publish happens, so stats stay at zero
    EXPECT_EQ(mqtt.getMessagesPublished(), 0u);
    EXPECT_EQ(mqtt.getMessagesFailed(), 0u);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(MQTTManagerTest, StatisticsStartAtZero) {
    EXPECT_EQ(mqtt.getMessagesPublished(), 0u);
    EXPECT_EQ(mqtt.getMessagesFailed(), 0u);
    EXPECT_EQ(mqtt.getReconnectCount(), 0u);
    EXPECT_EQ(mqtt.getLastError(), "");
}

// ============================================================================
// CALLBACK REGISTRATION TESTS
// ============================================================================

TEST_F(MQTTManagerTest, OnCommandCallbackRegistered) {
    bool callbackSet = false;
    String receivedEntityId;
    String receivedPayload;

    mqtt.onCommand([&](const String& entityId, const String& payload) {
        callbackSet = true;
        receivedEntityId = entityId;
        receivedPayload = payload;
    });

    // The callback is registered but cannot be triggered on desktop because
    // handleMessage is private and requires ESP32 MQTT. Verify registration
    // did not crash and the manager is still functional.
    EXPECT_FALSE(callbackSet);
    EXPECT_FALSE(mqtt.isConnected());
}

// ============================================================================
// DEFAULT CONFIG TESTS
// ============================================================================

TEST_F(MQTTManagerTest, ConfigDefaultPort) {
    MQTTConfig config;
    EXPECT_EQ(config.port, 1883u);

    // Verify default port is preserved through begin()
    config.server = "test.local";
    config.device_id = "dev1";
    mqtt.begin(config);

    EXPECT_EQ(mqtt.getConfig().port, 1883u);
}
