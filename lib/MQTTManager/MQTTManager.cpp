#include "MQTTManager.h"
#include "Logger.h"
#include <math.h>

// ============================================================================
// Public API
// ============================================================================

void MQTTManager::begin(const MQTTConfig& config) {
    config_ = config;
#ifdef ESP32
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setServer(config_.server.c_str(), config_.port);
    mqttClient_.setBufferSize(2048);
    mqttClient_.setCallback([this](char* topic, byte* payload, unsigned int length) {
        String topicStr(topic);
        String payloadStr;
        payloadStr.reserve(length);
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        handleMessage(topicStr, payloadStr);
    });
#endif
    logger.logInfo("MQTTManager initialized, server: " + config_.server + ":" + String(config_.port));
}

void MQTTManager::update() {
    if (!enabled_) return;

#ifdef ESP32
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqttClient_.connected()) {
        unsigned long now = millis();
        if (now - last_reconnect_attempt_ >= reconnect_interval_) {
            last_reconnect_attempt_ = now;
            if (connect()) {
                reconnect_interval_ = 5000; // Reset backoff on success
            } else {
                // Exponential backoff
                reconnect_interval_ = min(reconnect_interval_ * 2, MAX_RECONNECT_INTERVAL);
            }
        }
        return;
    }

    // Connection just established
    if (!was_connected_) {
        was_connected_ = true;
        logger.logInfo("MQTT connected to " + config_.server);
    }

    mqttClient_.loop();

    // Publish state on meaningful change or periodic interval
    unsigned long now = millis();
    if (state_changed_ || (now - last_state_publish_ >= STATE_PUBLISH_INTERVAL)) {
        publishState(last_state_);
        state_changed_ = false;
    }
#endif
}

void MQTTManager::setState(const MQTTStateData& state) {
    if (!has_published_ || hasMeaningfulChange(last_state_, state)) {
        state_changed_ = true;
    }
    last_state_ = state;
}

bool MQTTManager::hasMeaningfulChange(const MQTTStateData& a, const MQTTStateData& b) const {
    // Compare all fields except heap, uptime, and rssi (these change constantly)
    if (a.sensor1_connected != b.sensor1_connected) return true;
    if (a.sensor2_connected != b.sensor2_connected) return true;
    if (a.pump_running != b.pump_running) return true;
    if (a.pump_auto_mode != b.pump_auto_mode) return true;
    if (a.water_flow_error != b.water_flow_error) return true;
    if (a.door_open != b.door_open) return true;
    if (a.door_closed != b.door_closed) return true;
    if (a.door_auto_mode != b.door_auto_mode) return true;
    if (a.light_on != b.light_on) return true;
    if (a.light_brightness != b.light_brightness) return true;
    if (a.light_auto_mode != b.light_auto_mode) return true;
    // Use threshold for float comparisons (0.1 degree)
    if (fabsf(a.sensor1_temp_f - b.sensor1_temp_f) > 0.1f) return true;
    if (fabsf(a.sensor2_temp_f - b.sensor2_temp_f) > 0.1f) return true;
    if (fabsf(a.water_flow_rate - b.water_flow_rate) > 0.01f) return true;
    if (fabsf(a.water_total_gallons - b.water_total_gallons) > 0.01f) return true;
    if (fabsf(a.temp_threshold_on - b.temp_threshold_on) > 0.01f) return true;
    if (fabsf(a.temp_threshold_off - b.temp_threshold_off) > 0.01f) return true;
    // Handle NaN transitions
    if (isnan(a.sensor1_temp_f) != isnan(b.sensor1_temp_f)) return true;
    if (isnan(a.sensor2_temp_f) != isnan(b.sensor2_temp_f)) return true;
    return false;
}

void MQTTManager::publishState(const MQTTStateData& state) {
    last_state_ = state;
    has_published_ = true;

#ifdef ESP32
    if (!mqttClient_.connected()) return;

    // Publish main state JSON
    {
        JsonDocument doc;
        // Use null for NaN values to keep JSON valid
        if (isnan(state.sensor1_temp_f)) doc["sensor1_temp_f"] = nullptr;
        else doc["sensor1_temp_f"] = serialized(String(state.sensor1_temp_f, 1));
        if (isnan(state.sensor2_temp_f)) doc["sensor2_temp_f"] = nullptr;
        else doc["sensor2_temp_f"] = serialized(String(state.sensor2_temp_f, 1));
        doc["sensor1_connected"] = state.sensor1_connected;
        doc["sensor2_connected"] = state.sensor2_connected;
        doc["water_flow_rate"] = serialized(String(state.water_flow_rate, 2));
        doc["water_total_gallons"] = serialized(String(state.water_total_gallons, 2));
        doc["pump_running"] = state.pump_running;
        doc["pump_auto_mode"] = state.pump_auto_mode;
        doc["water_flow_error"] = state.water_flow_error;
        doc["door_open"] = state.door_open;
        doc["door_closed"] = state.door_closed;
        doc["door_auto_mode"] = state.door_auto_mode;
        doc["light_on"] = state.light_on;
        doc["light_brightness"] = state.light_brightness;
        doc["light_auto_mode"] = state.light_auto_mode;
        doc["temp_threshold_on"] = serialized(String(state.temp_threshold_on, 1));
        doc["temp_threshold_off"] = serialized(String(state.temp_threshold_off, 1));
        doc["wifi_rssi"] = state.wifi_rssi;
        doc["free_heap"] = state.free_heap;
        doc["uptime"] = state.uptime_seconds;

        String stateTopic = getBaseTopic() + "/state";
        if (publishJson(stateTopic, doc)) {
            last_state_publish_ = millis();
        }
    }

    // Publish light state separately for JSON schema light entity
    {
        JsonDocument lightDoc;
        lightDoc["state"] = state.light_on ? "ON" : "OFF";
        lightDoc["brightness"] = state.light_brightness;

        String lightStateTopic = getBaseTopic() + "/light/state";
        publishJson(lightStateTopic, lightDoc);
    }
#endif
}

bool MQTTManager::isConnected() const {
#ifdef ESP32
    // PubSubClient::connected() is not const, use const_cast
    return const_cast<PubSubClient&>(mqttClient_).connected();
#else
    return false;
#endif
}

void MQTTManager::setConfig(const MQTTConfig& config) {
    config_ = config;
#ifdef ESP32
    // Disconnect if connected so we reconnect with new settings
    if (mqttClient_.connected()) {
        publishAvailability(false);
        mqttClient_.disconnect();
        was_connected_ = false;
    }
    mqttClient_.setServer(config_.server.c_str(), config_.port);
#endif
}

void MQTTManager::toJson(JsonObject& json) const {
    json["enabled"] = enabled_;
    json["connected"] = isConnected();
    json["server"] = config_.server;
    json["port"] = config_.port;
    json["device_id"] = config_.device_id;
    json["messages_published"] = messages_published_;
    json["messages_failed"] = messages_failed_;
    json["reconnect_count"] = reconnect_count_;
    json["last_error"] = last_error_;
}

// ============================================================================
// Connection
// ============================================================================

bool MQTTManager::connect() {
#ifdef ESP32
    logger.logInfo("MQTT connecting to " + config_.server + ":" + String(config_.port) + "...");
    reconnect_count_++;

    String availabilityTopic = getBaseTopic() + "/availability";

    bool result;
    if (config_.username.length() > 0) {
        result = mqttClient_.connect(
            config_.device_id.c_str(),
            config_.username.c_str(),
            config_.password.c_str(),
            availabilityTopic.c_str(),
            1,     // QoS 1
            true,  // retain
            "offline"
        );
    } else {
        result = mqttClient_.connect(
            config_.device_id.c_str(),
            nullptr,
            nullptr,
            availabilityTopic.c_str(),
            1,     // QoS 1
            true,  // retain
            "offline"
        );
    }

    if (result) {
        logger.logInfo("MQTT connected successfully");

        // Publish discovery configs
        publishDiscovery();

        // Publish online availability
        publishAvailability(true);

        // Subscribe to all command topics
        String commandTopic = getBaseTopic() + "/+/set";
        mqttClient_.subscribe(commandTopic.c_str(), 1);
        logger.logDebug("MQTT subscribed to: " + commandTopic);

        was_connected_ = true;
        return true;
    } else {
        int state = mqttClient_.state();
        last_error_ = "MQTT connect failed, rc=" + String(state);
        logger.logError(last_error_);
        was_connected_ = false;
        return false;
    }
#else
    return false;
#endif
}

// ============================================================================
// Availability
// ============================================================================

void MQTTManager::publishAvailability(bool online) {
#ifdef ESP32
    String topic = getBaseTopic() + "/availability";
    mqttClient_.publish(topic.c_str(), online ? "online" : "offline", true);
#endif
}

// ============================================================================
// Topic Helpers
// ============================================================================

String MQTTManager::getBaseTopic() const {
    return "coop_controller/" + config_.device_id;
}

String MQTTManager::getDiscoveryTopic(const String& component, const String& objectId) const {
    return "homeassistant/" + component + "/" + config_.device_id + "_" + objectId + "/config";
}

// ============================================================================
// Device & Origin Info
// ============================================================================

void MQTTManager::addDeviceInfo(JsonObject& doc) const {
    JsonObject device = doc["device"].to<JsonObject>();
    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(config_.device_id);
    device["name"] = config_.device_name;
    device["manufacturer"] = "DIY";
    device["model"] = "Coop Controller";
    device["sw_version"] = config_.fw_version;
    device["configuration_url"] = "http://" + config_.hostname + ".local";
}

void MQTTManager::addOriginInfo(JsonObject& doc) const {
    JsonObject origin = doc["origin"].to<JsonObject>();
    origin["name"] = "Coop Controller";
    origin["sw_version"] = config_.fw_version;
    origin["support_url"] = "https://github.com/cbrown350/coopcontroller";
}

// ============================================================================
// JSON Publishing Helper
// ============================================================================

bool MQTTManager::publishJson(const String& topic, JsonDocument& doc) {
#ifdef ESP32
    String payload;
    serializeJson(doc, payload);

    if (mqttClient_.publish(topic.c_str(), payload.c_str(), true)) {
        messages_published_++;
        return true;
    } else {
        messages_failed_++;
        last_error_ = "Publish failed: " + topic;
        logger.logError(last_error_);
        return false;
    }
#else
    (void)topic;
    (void)doc;
    return false;
#endif
}

// ============================================================================
// Discovery Publishing
// ============================================================================

void MQTTManager::publishDiscovery() {
    logger.logInfo("MQTT publishing Home Assistant discovery configs...");

    // --- Sensors ---
    publishSensorDiscovery("sensor_1_temperature_f", "Sensor 1 Temperature",
                           "{{ value_json.sensor1_temp_f | default('') }}",
                           "temperature", "\u00b0F", "measurement", "",
                           "", "{{ value_json.sensor1_temp_f is not none }}");

    publishSensorDiscovery("sensor_2_temperature_f", "Sensor 2 Temperature",
                           "{{ value_json.sensor2_temp_f | default('') }}",
                           "temperature", "\u00b0F", "measurement", "",
                           "", "{{ value_json.sensor2_temp_f is not none }}");

    publishSensorDiscovery("water_flow_rate", "Water Flow Rate",
                           "{{ value_json.water_flow_rate }}",
                           "", "GPM", "measurement", "",
                           "mdi:water-pump");

    publishSensorDiscovery("water_total_gallons", "Water Total Gallons",
                           "{{ value_json.water_total_gallons }}",
                           "", "gal", "total_increasing", "",
                           "mdi:water");

    publishSensorDiscovery("wifi_rssi", "WiFi Signal Strength",
                           "{{ value_json.wifi_rssi }}",
                           "signal_strength", "dBm", "measurement");

    publishSensorDiscovery("free_heap", "Free Heap Memory",
                           "{{ value_json.free_heap }}",
                           "", "bytes", "measurement", "diagnostic",
                           "mdi:memory");

    publishSensorDiscovery("uptime", "Uptime",
                           "{{ value_json.uptime }}",
                           "duration", "s", "measurement", "diagnostic");

    // --- Binary Sensors ---
    publishBinarySensorDiscovery("sensor_1_connected", "Sensor 1 Connected",
                                  "{{ value_json.sensor1_connected }}",
                                  "connectivity");

    publishBinarySensorDiscovery("sensor_2_connected", "Sensor 2 Connected",
                                  "{{ value_json.sensor2_connected }}",
                                  "connectivity");

    publishBinarySensorDiscovery("pump_running", "Pump Running",
                                  "{{ value_json.pump_running }}",
                                  "running");

    publishBinarySensorDiscovery("door_open", "Door Open",
                                  "{{ value_json.door_open }}",
                                  "door");

    publishBinarySensorDiscovery("door_closed", "Door Closed",
                                  "{{ value_json.door_closed }}",
                                  "");

    publishBinarySensorDiscovery("water_flow_error", "Water Flow Error",
                                  "{{ value_json.water_flow_error }}",
                                  "problem");

    // --- Switches ---
    publishSwitchDiscovery("pump_auto_mode", "Pump Auto Mode",
                            "{{ value_json.pump_auto_mode }}",
                            "mdi:pump");

    publishSwitchDiscovery("light_auto_mode", "Light Auto Mode",
                            "{{ value_json.light_auto_mode }}",
                            "mdi:lightbulb-auto");

    publishSwitchDiscovery("door_auto_mode", "Door Auto Mode",
                            "{{ value_json.door_auto_mode }}",
                            "mdi:door-sliding");

    // --- Light ---
    publishLightDiscovery();

    // --- Buttons ---
    publishButtonDiscovery("pump_on", "Pump On", "mdi:pump");
    publishButtonDiscovery("pump_off", "Pump Off", "mdi:pump-off");
    publishButtonDiscovery("door_open_cmd", "Door Open", "mdi:door-open");
    publishButtonDiscovery("door_close_cmd", "Door Close", "mdi:door-closed");
    publishButtonDiscovery("door_stop", "Door Stop", "mdi:stop");

    // --- Numbers ---
    publishNumberDiscovery("temp_threshold_on", "Temp Threshold ON",
                            "{{ value_json.temp_threshold_on }}",
                            -10, 80, 0.5f, "\u00b0F", "mdi:thermometer-high", "box");

    publishNumberDiscovery("temp_threshold_off", "Temp Threshold OFF",
                            "{{ value_json.temp_threshold_off }}",
                            -10, 80, 0.5f, "\u00b0F", "mdi:thermometer-low", "box");

    publishNumberDiscovery("light_brightness", "Light Brightness",
                            "{{ value_json.light_brightness }}",
                            0, 100, 1, "%", "mdi:brightness-percent", "slider");

    logger.logInfo("MQTT discovery configs published");
}

// ============================================================================
// Discovery Helpers
// ============================================================================

void MQTTManager::publishSensorDiscovery(const String& objectId, const String& name,
                                          const String& valueTemplate,
                                          const String& deviceClass,
                                          const String& unit,
                                          const String& stateClass,
                                          const String& entityCategory,
                                          const String& icon,
                                          const String& availabilityTemplate) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = name;
    root["unique_id"] = config_.device_id + "_" + objectId;
    root["object_id"] = config_.device_id + "_" + objectId;
    root["state_topic"] = getBaseTopic() + "/state";
    root["value_template"] = valueTemplate;

    if (deviceClass.length() > 0) root["device_class"] = deviceClass;
    if (unit.length() > 0) root["unit_of_measurement"] = unit;
    if (stateClass.length() > 0) root["state_class"] = stateClass;
    if (entityCategory.length() > 0) root["entity_category"] = entityCategory;
    if (icon.length() > 0) root["icon"] = icon;

    // Use availability list to support both LWT and per-value availability
    if (availabilityTemplate.length() > 0) {
        JsonArray avail = root["availability"].to<JsonArray>();
        JsonObject lwt = avail.add<JsonObject>();
        lwt["topic"] = getBaseTopic() + "/availability";
        JsonObject val = avail.add<JsonObject>();
        val["topic"] = getBaseTopic() + "/state";
        val["value_template"] = availabilityTemplate;
        val["payload_available"] = "True";
        val["payload_not_available"] = "False";
        root["availability_mode"] = "all";
    } else {
        root["availability_topic"] = getBaseTopic() + "/availability";
    }

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("sensor", objectId);
    publishJson(topic, doc);
}

void MQTTManager::publishBinarySensorDiscovery(const String& objectId, const String& name,
                                                const String& valueTemplate,
                                                const String& deviceClass,
                                                const String& icon) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = name;
    root["unique_id"] = config_.device_id + "_" + objectId;
    root["object_id"] = config_.device_id + "_" + objectId;
    root["state_topic"] = getBaseTopic() + "/state";
    root["availability_topic"] = getBaseTopic() + "/availability";
    root["value_template"] = valueTemplate;
    root["payload_on"] = "True";
    root["payload_off"] = "False";

    if (deviceClass.length() > 0) root["device_class"] = deviceClass;
    if (icon.length() > 0) root["icon"] = icon;

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("binary_sensor", objectId);
    publishJson(topic, doc);
}

void MQTTManager::publishSwitchDiscovery(const String& objectId, const String& name,
                                          const String& valueTemplate,
                                          const String& icon) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = name;
    root["unique_id"] = config_.device_id + "_" + objectId;
    root["object_id"] = config_.device_id + "_" + objectId;
    root["state_topic"] = getBaseTopic() + "/state";
    root["availability_topic"] = getBaseTopic() + "/availability";
    root["command_topic"] = getBaseTopic() + "/" + objectId + "/set";
    root["value_template"] = valueTemplate;
    root["payload_on"] = "ON";
    root["payload_off"] = "OFF";
    root["state_on"] = "True";
    root["state_off"] = "False";

    if (icon.length() > 0) root["icon"] = icon;

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("switch", objectId);
    publishJson(topic, doc);
}

void MQTTManager::publishLightDiscovery() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = "Coop Light";
    root["unique_id"] = config_.device_id + "_coop_light";
    root["object_id"] = config_.device_id + "_coop_light";
    root["schema"] = "json";
    root["brightness"] = true;
    root["brightness_scale"] = 100;
    root["state_topic"] = getBaseTopic() + "/light/state";
    root["command_topic"] = getBaseTopic() + "/light/set";
    root["availability_topic"] = getBaseTopic() + "/availability";
    root["icon"] = "mdi:lightbulb";

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("light", "coop_light");
    publishJson(topic, doc);
}

void MQTTManager::publishButtonDiscovery(const String& objectId, const String& name,
                                          const String& icon) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = name;
    root["unique_id"] = config_.device_id + "_" + objectId;
    root["object_id"] = config_.device_id + "_" + objectId;
    root["command_topic"] = getBaseTopic() + "/" + objectId + "/set";
    root["availability_topic"] = getBaseTopic() + "/availability";
    root["payload_press"] = "PRESS";

    if (icon.length() > 0) root["icon"] = icon;

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("button", objectId);
    publishJson(topic, doc);
}

void MQTTManager::publishNumberDiscovery(const String& objectId, const String& name,
                                          const String& valueTemplate,
                                          float min, float max, float step,
                                          const String& unit,
                                          const String& icon,
                                          const String& mode) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["name"] = name;
    root["unique_id"] = config_.device_id + "_" + objectId;
    root["object_id"] = config_.device_id + "_" + objectId;
    root["state_topic"] = getBaseTopic() + "/state";
    root["availability_topic"] = getBaseTopic() + "/availability";
    root["command_topic"] = getBaseTopic() + "/" + objectId + "/set";
    root["value_template"] = valueTemplate;
    root["min"] = min;
    root["max"] = max;
    root["step"] = step;

    if (unit.length() > 0) root["unit_of_measurement"] = unit;
    if (icon.length() > 0) root["icon"] = icon;
    if (mode.length() > 0) root["mode"] = mode;

    addDeviceInfo(root);
    addOriginInfo(root);

    String topic = getDiscoveryTopic("number", objectId);
    publishJson(topic, doc);
}

// ============================================================================
// Message Handling
// ============================================================================

void MQTTManager::handleMessage(const String& topic, const String& payload) {
    logger.logDebug("MQTT received: " + topic + " = " + payload);

    // Extract entity_id from topic: coop_controller/{device_id}/{entity_id}/set
    String baseTopic = getBaseTopic() + "/";
    if (!topic.startsWith(baseTopic)) return;

    String remainder = topic.substring(baseTopic.length());
    int setIdx = remainder.lastIndexOf("/set");
    if (setIdx < 0) return;

    String entityId = remainder.substring(0, setIdx);

    if (commandCallback_) {
        commandCallback_(entityId, payload);
    }
}
