#include "HistoricalDataManager.h"
#include <ArduinoJson.h>
#include <time.h>

HistoricalDataManager::HistoricalDataManager()
    : maxSize(1440)
    , currentIndex(0)
    , enabled(true)
    , prevTemperature(NAN)
    , prevFlowRate(0.0f)
    , prevPumpActive(false)
    , prevLightBrightness(0)
    , lastTempRecordTime(0)
    , lastFlowRecordTime(0)
    , tempMinIntervalSeconds(60)
    , flowMinIntervalSeconds(10)
    , firstUpdate(true)
{
    prevDoorState[0] = '\0';
    prevDoorPosition[0] = '\0';
}

void HistoricalDataManager::begin(bool enableData, size_t bufferSize,
                                   unsigned int tempMinIntervalSec, unsigned int flowMinIntervalSec) {
    enabled = enableData;
    maxSize = bufferSize;
    tempMinIntervalSeconds = tempMinIntervalSec;
    flowMinIntervalSeconds = flowMinIntervalSec;

    buffer.clear();
    buffer.shrink_to_fit();
    currentIndex = 0;
    firstUpdate = true;
    prevTemperature = NAN;
    prevFlowRate = 0.0f;
    prevPumpActive = false;
    prevLightBrightness = 0;
    prevDoorState[0] = '\0';
    prevDoorPosition[0] = '\0';
    lastTempRecordTime = 0;
    lastFlowRecordTime = 0;
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
                                              uint8_t light_brightness, const char* door_state,
                                              const char* door_position, const char* pump_trigger,
                                              const char* door_trigger, const char* light_trigger,
                                              const char* eventType) {
    DataPoint point;
    unsigned long currentTime = millis() / 1000;
    time_t now = time(nullptr);
    point.timestamp = (now > 1000000000) ? now : currentTime;
    point.temperature_f = temperature_f;
    point.pump_active = pump_active;
    point.flow_rate = flow_rate;
    point.light_brightness = light_brightness;

    strncpy(point.door_state, door_state, sizeof(point.door_state) - 1);
    point.door_state[sizeof(point.door_state) - 1] = '\0';
    strncpy(point.door_position, door_position, sizeof(point.door_position) - 1);
    point.door_position[sizeof(point.door_position) - 1] = '\0';
    strncpy(point.pump_trigger, pump_trigger, sizeof(point.pump_trigger) - 1);
    point.pump_trigger[sizeof(point.pump_trigger) - 1] = '\0';
    strncpy(point.door_trigger, door_trigger, sizeof(point.door_trigger) - 1);
    point.door_trigger[sizeof(point.door_trigger) - 1] = '\0';
    strncpy(point.light_trigger, light_trigger, sizeof(point.light_trigger) - 1);
    point.light_trigger[sizeof(point.light_trigger) - 1] = '\0';
    strncpy(point.event_type, eventType, sizeof(point.event_type) - 1);
    point.event_type[sizeof(point.event_type) - 1] = '\0';

    return point;
}

// Helper to update all previous tracking values after recording
#define UPDATE_ALL_PREV() do { \
    prevTemperature = temperature_f; \
    prevFlowRate = flow_rate; \
    prevPumpActive = pump_active; \
    prevLightBrightness = light_brightness; \
    strncpy(prevDoorState, door_state, sizeof(prevDoorState) - 1); \
    prevDoorState[sizeof(prevDoorState) - 1] = '\0'; \
    strncpy(prevDoorPosition, door_position, sizeof(prevDoorPosition) - 1); \
    prevDoorPosition[sizeof(prevDoorPosition) - 1] = '\0'; \
} while(0)

void HistoricalDataManager::checkAndRecord(float temperature_f, bool pump_active, float flow_rate,
                                            uint8_t light_brightness, const char* door_state,
                                            const char* door_position, const char* pump_trigger,
                                            const char* door_trigger, const char* light_trigger) {
    if (!enabled) return;

    unsigned long currentTime = millis() / 1000;

    // On first call, record initial state and initialize tracking
    if (firstUpdate) {
        firstUpdate = false;
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "temp");
        addPoint(point);
        return;
    }

    // Check for pump state change (immediate)
    if (pump_active != prevPumpActive) {
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "pump");
        addPoint(point);
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for door state/position change (immediate)
    if (strcmp(prevDoorState, door_state) != 0 ||
        strcmp(prevDoorPosition, door_position) != 0) {
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "door");
        addPoint(point);
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for light brightness change (immediate)
    if (light_brightness != prevLightBrightness) {
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "light");
        addPoint(point);
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for flow rate change (with min interval)
    float flowDelta = fabs(flow_rate - prevFlowRate);
    if (flowDelta > 0.001f && (currentTime - lastFlowRecordTime >= flowMinIntervalSeconds)) {
        lastFlowRecordTime = currentTime;
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "flow");
        addPoint(point);
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        return;
    }

    // Check for temperature change (with min interval)
    bool tempChanged = false;
    if (isnan(prevTemperature) && !isnan(temperature_f)) {
        tempChanged = true;
    } else if (!isnan(prevTemperature) && isnan(temperature_f)) {
        tempChanged = true;
    } else if (!isnan(prevTemperature) && !isnan(temperature_f)) {
        tempChanged = fabs(temperature_f - prevTemperature) >= 0.5f;
    }

    if (tempChanged && (currentTime - lastTempRecordTime >= tempMinIntervalSeconds)) {
        lastTempRecordTime = currentTime;
        DataPoint point = createPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                       door_state, door_position, pump_trigger, door_trigger,
                                       light_trigger, "temp");
        addPoint(point);
        UPDATE_ALL_PREV();
        lastFlowRecordTime = currentTime;
        return;
    }
}

#undef UPDATE_ALL_PREV

String HistoricalDataManager::getDataAsJson() const {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    if (buffer.empty()) {
        return "[]";
    }

    size_t startIndex = buffer.size() < maxSize ? 0 : currentIndex;
    size_t count = buffer.size();

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
        obj["event_type"] = point.event_type;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

String HistoricalDataManager::getDataAsCsv() const {
    String csv = "timestamp,temperature_f,pump_active,flow_rate,light_brightness,door_state,door_position,pump_trigger,door_trigger,light_trigger,event_type\n";

    if (buffer.empty()) {
        return csv;
    }

    size_t startIndex = buffer.size() < maxSize ? 0 : currentIndex;
    size_t count = buffer.size();

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
        csv += String(point.event_type);
        csv += "\n";
    }

    return csv;
}

size_t HistoricalDataManager::getOrderedIndex(size_t i) const {
    size_t startIndex = buffer.size() < maxSize ? 0 : currentIndex;
    return (startIndex + i) % buffer.size();
}

const DataPoint& HistoricalDataManager::getDataPointAt(size_t rawIndex) const {
    return buffer[rawIndex];
}

void HistoricalDataManager::clear() {
    buffer.clear();
    currentIndex = 0;
    firstUpdate = true;
}

size_t HistoricalDataManager::getDataPointCount() const {
    return buffer.size();
}
