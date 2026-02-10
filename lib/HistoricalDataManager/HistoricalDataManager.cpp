#include "HistoricalDataManager.h"
#include <ArduinoJson.h>
#include <time.h>

HistoricalDataManager::HistoricalDataManager()
    : maxSize(1440)
    , currentIndex(0)
    , lastSampleTime(0)
    , sampleIntervalSeconds(60)
    , enabled(true)
{
    // Don't reserve upfront - let vector grow naturally to avoid large allocation at startup
    // This prevents heap fragmentation on ESP32
}

void HistoricalDataManager::begin(bool enableData, size_t bufferSize, unsigned int intervalSeconds) {
    enabled = enableData;
    maxSize = bufferSize;
    sampleIntervalSeconds = intervalSeconds;

    // Clear buffer but don't reserve to avoid large heap allocation
    buffer.clear();

    lastSampleTime = millis() / 1000; // Initialize to current time in seconds
}

void HistoricalDataManager::addPoint(const DataPoint& point) {
    if (buffer.size() < maxSize) {
        buffer.push_back(point);
    } else {
        buffer[currentIndex] = point;
        currentIndex = (currentIndex + 1) % maxSize;
    }
}

DataPoint HistoricalDataManager::createPoint(float temperature_f, bool pump_active, float flow_rate,
                                              uint8_t light_brightness, const String& door_state,
                                              const String& door_position, const String& pump_trigger,
                                              const String& door_trigger, const String& light_trigger,
                                              bool isEvent) {
    DataPoint point;
    unsigned long currentTime = millis() / 1000;
    time_t now = time(nullptr);
    point.timestamp = (now > 1000000000) ? now : currentTime;
    point.temperature_f = temperature_f;
    point.pump_active = pump_active;
    point.flow_rate = flow_rate;
    point.light_brightness = light_brightness;
    point.is_event = isEvent;

    strncpy(point.door_state, door_state.c_str(), sizeof(point.door_state) - 1);
    point.door_state[sizeof(point.door_state) - 1] = '\0';
    strncpy(point.door_position, door_position.c_str(), sizeof(point.door_position) - 1);
    point.door_position[sizeof(point.door_position) - 1] = '\0';
    strncpy(point.pump_trigger, pump_trigger.c_str(), sizeof(point.pump_trigger) - 1);
    point.pump_trigger[sizeof(point.pump_trigger) - 1] = '\0';
    strncpy(point.door_trigger, door_trigger.c_str(), sizeof(point.door_trigger) - 1);
    point.door_trigger[sizeof(point.door_trigger) - 1] = '\0';
    strncpy(point.light_trigger, light_trigger.c_str(), sizeof(point.light_trigger) - 1);
    point.light_trigger[sizeof(point.light_trigger) - 1] = '\0';

    return point;
}

void HistoricalDataManager::update(float temperature_f, bool pump_active, float flow_rate, uint8_t light_brightness,
                                   const String& door_state, const String& door_position, const String& pump_trigger,
                                   const String& door_trigger, const String& light_trigger) {
    if (!enabled) {
        return;
    }

    unsigned long currentTime = millis() / 1000;

    if (currentTime - lastSampleTime < sampleIntervalSeconds) {
        return;
    }

    DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                   door_state, door_position, pump_trigger, door_trigger,
                                   light_trigger, false);
    addPoint(point);
    lastSampleTime = currentTime;
}

void HistoricalDataManager::recordEvent(float temperature_f, bool pump_active, float flow_rate,
                                         uint8_t light_brightness, const String& door_state,
                                         const String& door_position, const String& pump_trigger,
                                         const String& door_trigger, const String& light_trigger) {
    if (!enabled) {
        return;
    }

    DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                   door_state, door_position, pump_trigger, door_trigger,
                                   light_trigger, true);
    addPoint(point);
}

String HistoricalDataManager::getDataAsJson() const {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    if (buffer.empty()) {
        return "[]";
    }

    // Determine the starting index for oldest data
    size_t startIndex = buffer.size() < maxSize ? 0 : currentIndex;
    size_t count = buffer.size();

    // Add data points in chronological order (oldest first)
    for (size_t i = 0; i < count; i++) {
        size_t index = (startIndex + i) % buffer.size();
        const DataPoint& point = buffer[index];

        JsonObject obj = array.add<JsonObject>();
        obj["timestamp"] = point.timestamp;
        obj["temperature_f"] = point.temperature_f;
        obj["pump_active"] = point.pump_active;
        obj["flow_rate"] = point.flow_rate;
        obj["light_brightness"] = point.light_brightness;
        obj["door_state"] = point.door_state;
        obj["door_position"] = point.door_position;
        obj["pump_trigger"] = point.pump_trigger;
        obj["door_trigger"] = point.door_trigger;
        obj["light_trigger"] = point.light_trigger;
        obj["is_event"] = point.is_event;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

String HistoricalDataManager::getDataAsCsv() const {
    String csv = "timestamp,temperature_f,pump_active,flow_rate,light_brightness,door_state,door_position,pump_trigger,door_trigger,light_trigger,is_event\n";

    if (buffer.empty()) {
        return csv;
    }

    // Determine the starting index for oldest data
    size_t startIndex = buffer.size() < maxSize ? 0 : currentIndex;
    size_t count = buffer.size();

    // Add data points in chronological order (oldest first)
    for (size_t i = 0; i < count; i++) {
        size_t index = (startIndex + i) % buffer.size();
        const DataPoint& point = buffer[index];

        csv += String(point.timestamp) + ",";

        if (isnan(point.temperature_f)) {
            csv += ",";
        } else {
            csv += String(point.temperature_f, 2) + ",";
        }

        csv += point.pump_active ? "true," : "false,";
        csv += String(point.flow_rate, 3) + ",";
        csv += String(point.light_brightness) + ",";
        csv += String(point.door_state) + ",";
        csv += String(point.door_position) + ",";
        csv += String(point.pump_trigger) + ",";
        csv += String(point.door_trigger) + ",";
        csv += String(point.light_trigger) + ",";
        csv += point.is_event ? "true" : "false";
        csv += "\n";
    }

    return csv;
}

void HistoricalDataManager::clear() {
    buffer.clear();
    currentIndex = 0;
    lastSampleTime = millis() / 1000;
}

size_t HistoricalDataManager::getDataPointCount() const {
    return buffer.size();
}
