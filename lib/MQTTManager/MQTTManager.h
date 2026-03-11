#ifndef __MQTT_MANAGER_H__
#define __MQTT_MANAGER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

// Forward declarations - MQTTManager uses PubSubClient internally but
// we don't include it in the header for desktop test compatibility
#ifdef ESP32
#include <WiFi.h>
#include <PubSubClient.h>
#endif

// Callback types for command handling
using MQTTCommandCallback = std::function<void(const String& entityId, const String& payload)>;

struct MQTTConfig {
    String server;
    uint16_t port = 1883;
    String username;
    String password;
    String device_id;      // Unique device identifier (from MAC or hostname)
    String device_name;    // Human-readable device name
    String hostname;       // Device hostname for configuration_url
    String fw_version;     // Firmware version for device info
};

// State data structure that gets published
struct MQTTStateData {
    // Sensors
    float sensor1_temp_f = 0;
    float sensor2_temp_f = 0;
    bool sensor1_connected = false;
    bool sensor2_connected = false;
    float water_flow_rate = 0;
    float water_total_gallons = 0;

    // Pump
    bool pump_running = false;
    bool pump_auto_mode = false;
    bool water_flow_error = false;

    // Door
    bool door_open = false;
    bool door_closed = false;
    bool door_auto_mode = false;

    // Light
    bool light_on = false;
    int light_brightness = 0;  // 0-100
    bool light_auto_mode = false;

    // Settings (number entities)
    float temp_threshold_on = 34.0;
    float temp_threshold_off = 36.0;

    // System
    int wifi_rssi = -127;
    uint32_t free_heap = 0;
    unsigned long uptime_seconds = 0;
};

class MQTTManager {
public:
    MQTTManager() = default;

    // Initialize with configuration
    void begin(const MQTTConfig& config);

    // Call in main loop - handles connection, reconnection, and message processing
    void update();

    // Update state data - detects meaningful changes and publishes when appropriate
    void setState(const MQTTStateData& state);

    // Force-publish current state immediately (used internally and after connect)
    void publishState(const MQTTStateData& state);

    // Register callback for when commands are received
    void onCommand(MQTTCommandCallback callback) { commandCallback_ = callback; }

    // Connection status
    bool isConnected() const;
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    // Configuration
    void setConfig(const MQTTConfig& config);
    const MQTTConfig& getConfig() const { return config_; }

    // Serialize status to JSON
    void toJson(JsonObject& json) const;

    // Statistics
    unsigned int getMessagesPublished() const { return messages_published_; }
    unsigned int getMessagesFailed() const { return messages_failed_; }
    unsigned int getReconnectCount() const { return reconnect_count_; }
    String getLastError() const { return last_error_; }

private:
    bool enabled_ = false;
    MQTTConfig config_;
    MQTTCommandCallback commandCallback_;

    // Connection state
    bool was_connected_ = false;
    unsigned long last_reconnect_attempt_ = 0;
    unsigned long reconnect_interval_ = 5000;  // Start with 5s, backoff to 60s
    static constexpr unsigned long MAX_RECONNECT_INTERVAL = 60000;

    // State publishing
    unsigned long last_state_publish_ = 0;
    static constexpr unsigned long STATE_PUBLISH_INTERVAL = 30000;  // Publish state every 30s
    MQTTStateData last_state_;
    bool state_changed_ = false;
    bool has_published_ = false;  // True after first publish

    // Check if meaningful state fields changed (excludes heap/uptime/rssi)
    bool hasMeaningfulChange(const MQTTStateData& a, const MQTTStateData& b) const;

    // Statistics
    unsigned int messages_published_ = 0;
    unsigned int messages_failed_ = 0;
    unsigned int reconnect_count_ = 0;
    String last_error_;

#ifdef ESP32
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;
#endif

    // Internal methods
    bool connect();
    void publishDiscovery();
    void publishAvailability(bool online);
    String getBaseTopic() const;
    String getDiscoveryTopic(const String& component, const String& objectId) const;

    // Discovery helpers
    void publishSensorDiscovery(const String& objectId, const String& name,
                                const String& valueTemplate,
                                const String& deviceClass = "",
                                const String& unit = "",
                                const String& stateClass = "",
                                const String& entityCategory = "",
                                const String& icon = "",
                                const String& availabilityTemplate = "");
    void publishBinarySensorDiscovery(const String& objectId, const String& name,
                                      const String& valueTemplate,
                                      const String& deviceClass = "",
                                      const String& icon = "");
    void publishSwitchDiscovery(const String& objectId, const String& name,
                                const String& valueTemplate,
                                const String& icon = "");
    void publishLightDiscovery();
    void publishButtonDiscovery(const String& objectId, const String& name,
                                const String& icon = "");
    void publishNumberDiscovery(const String& objectId, const String& name,
                                const String& valueTemplate,
                                float min, float max, float step,
                                const String& unit = "",
                                const String& icon = "",
                                const String& mode = "auto");

    // Build common device JSON
    void addDeviceInfo(JsonObject& doc) const;

    // Add common origin info
    void addOriginInfo(JsonObject& doc) const;

    // Publish a JSON document to a topic with retain
    bool publishJson(const String& topic, JsonDocument& doc);

    // MQTT message callback
    void handleMessage(const String& topic, const String& payload);
};

#endif // __MQTT_MANAGER_H__
