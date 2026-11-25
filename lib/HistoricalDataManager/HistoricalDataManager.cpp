#include "HistoricalDataManager.h"
#include <ArduinoJson.h>
#include <math.h>
#include <new>
#include <time.h>

namespace {

// Lookup tables for compact enum packing of DataPoint string fields.
// Index 0 is always the default/fallback string.
constexpr const char* kDoorStates[] = {
    "UNKNOWN", "IDLE", "OPENING", "OPEN", "CLOSING", "CLOSED", "FAULT"
};
constexpr size_t kDoorStatesCount = sizeof(kDoorStates) / sizeof(kDoorStates[0]);

constexpr const char* kDoorPositions[] = {
    "UNKNOWN", "OPEN", "CLOSED", "PARTIAL"
};
constexpr size_t kDoorPositionsCount = sizeof(kDoorPositions) / sizeof(kDoorPositions[0]);

constexpr const char* kTriggers[] = {
    "unknown", "button", "web", "api", "sunrise", "sunset", "auto_sunset",
    "timer", "temp_threshold", "temp_cycle", "flow_fault", "maintenance",
    "startup", "fault", "test"
};
constexpr size_t kTriggersCount = sizeof(kTriggers) / sizeof(kTriggers[0]);

constexpr const char* kEventTypes[] = {
    "", "temp", "flow", "pump", "light", "door"
};
constexpr size_t kEventTypesCount = sizeof(kEventTypes) / sizeof(kEventTypes[0]);

uint8_t encodeTable(const char* value, const char* const* table, size_t count) {
    if (value == nullptr) return 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(value, table[i]) == 0) {
            return static_cast<uint8_t>(i);
        }
    }
    return 0;
}

const char* decodeTable(uint8_t idx, const char* const* table, size_t count) {
    return (idx < count) ? table[idx] : table[0];
}

void copySafe(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0) return;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

} // namespace

HistoricalDataManager::HistoricalDataManager()
    : count_(0)
    , maxSize(HISTORY_DEFAULT_BUFFER_SIZE)
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
    std::lock_guard<std::mutex> lock(mutex_);
    enabled = enableData;
    maxSize = bufferSize;
    tempMinIntervalSeconds = tempMinIntervalSec;
    flowMinIntervalSeconds = flowMinIntervalSec;

    chunks_.clear();
    chunks_.shrink_to_fit();
    count_ = 0;
    if (maxSize > HISTORY_MAX_BUFFER_SIZE) {
        maxSize = HISTORY_MAX_BUFFER_SIZE;
    }
    // Pre-reserve only the small pointer array (max 16 pointers = 64 bytes) so
    // chunks_ itself never reallocates during runtime.
    size_t maxChunks = (maxSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    try {
        chunks_.reserve(maxChunks);
    } catch (...) {
        // Non-fatal: will grow incrementally if needed.
    }

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

bool HistoricalDataManager::ensureCapacityForSlotLocked(size_t rawIndex) {
    size_t chunkIdx = rawIndex / CHUNK_SIZE;
    while (chunks_.size() <= chunkIdx) {
        auto* block = new (std::nothrow) CompactDataPoint[CHUNK_SIZE];
        if (block == nullptr) {
            return false;
        }
        try {
            chunks_.emplace_back(block);
        } catch (...) {
            delete[] block;
            return false;
        }
    }
    return true;
}

HistoricalDataManager::CompactDataPoint& HistoricalDataManager::slotAtLocked(size_t rawIndex) {
    return chunks_[rawIndex / CHUNK_SIZE][rawIndex % CHUNK_SIZE];
}

const HistoricalDataManager::CompactDataPoint& HistoricalDataManager::slotAtLocked(size_t rawIndex) const {
    return chunks_[rawIndex / CHUNK_SIZE][rawIndex % CHUNK_SIZE];
}

void HistoricalDataManager::addPointLocked(const CompactDataPoint& point) {
    if (maxSize == 0) return;

    if (count_ < maxSize) {
        if (!ensureCapacityForSlotLocked(count_)) {
            // Allocation failed — cap maxSize to current count and switch to
            // circular overwrite instead of throwing or dropping future points.
            maxSize = count_;
            if (maxSize == 0) return;
            slotAtLocked(currentIndex) = point;
            currentIndex = (currentIndex + 1) % maxSize;
            return;
        }
        slotAtLocked(count_) = point;
        count_++;
    } else {
        slotAtLocked(currentIndex) = point;
        currentIndex = (currentIndex + 1) % maxSize;
    }
}

HistoricalDataManager::CompactDataPoint HistoricalDataManager::packPoint(
    unsigned long timestamp, float temperature_f, bool pump_active,
    float flow_rate, uint8_t light_brightness, const char* door_state,
    const char* door_position, const char* pump_trigger,
    const char* door_trigger, const char* light_trigger,
    const char* eventType) {
    CompactDataPoint c;
    c.timestamp = static_cast<uint32_t>(timestamp);
    c.temperature_f = temperature_f;
    c.flow_rate = flow_rate;
    c.light_brightness = light_brightness;
    uint8_t evIdx = encodeTable(eventType, kEventTypes, kEventTypesCount);
    c.flags = static_cast<uint8_t>((pump_active ? 1U : 0U) | ((evIdx & 0x0FU) << 1));
    c.door_state_idx = encodeTable(door_state, kDoorStates, kDoorStatesCount);
    c.door_pos_idx = encodeTable(door_position, kDoorPositions, kDoorPositionsCount);
    c.pump_trig_idx = encodeTable(pump_trigger, kTriggers, kTriggersCount);
    c.door_trig_idx = encodeTable(door_trigger, kTriggers, kTriggersCount);
    c.light_trig_idx = encodeTable(light_trigger, kTriggers, kTriggersCount);
    c.reserved = 0;
    return c;
}

DataPoint HistoricalDataManager::unpackPoint(const CompactDataPoint& c) {
    DataPoint point;
    point.timestamp = c.timestamp;
    point.temperature_f = c.temperature_f;
    point.pump_active = (c.flags & 1U) != 0;
    point.flow_rate = c.flow_rate;
    point.light_brightness = c.light_brightness;
    copySafe(point.door_state, sizeof(point.door_state),
             decodeTable(c.door_state_idx, kDoorStates, kDoorStatesCount));
    copySafe(point.door_position, sizeof(point.door_position),
             decodeTable(c.door_pos_idx, kDoorPositions, kDoorPositionsCount));
    copySafe(point.pump_trigger, sizeof(point.pump_trigger),
             decodeTable(c.pump_trig_idx, kTriggers, kTriggersCount));
    copySafe(point.door_trigger, sizeof(point.door_trigger),
             decodeTable(c.door_trig_idx, kTriggers, kTriggersCount));
    copySafe(point.light_trigger, sizeof(point.light_trigger),
             decodeTable(c.light_trig_idx, kTriggers, kTriggersCount));
    uint8_t evIdx = static_cast<uint8_t>((c.flags >> 1) & 0x0FU);
    copySafe(point.event_type, sizeof(point.event_type),
             decodeTable(evIdx, kEventTypes, kEventTypesCount));
    return point;
}

HistoricalDataManager::CompactDataPoint HistoricalDataManager::createCompactPoint(
    float temperature_f, bool pump_active, float flow_rate,
    uint8_t light_brightness, const char* door_state,
    const char* door_position, const char* pump_trigger,
    const char* door_trigger, const char* light_trigger,
    const char* eventType) {
    unsigned long currentTime = millis() / 1000;
    time_t now = time(nullptr);
    unsigned long ts = (now > 1000000000) ? static_cast<unsigned long>(now) : currentTime;
    return packPoint(ts, temperature_f, pump_active, flow_rate, light_brightness,
                     door_state, door_position, pump_trigger, door_trigger,
                     light_trigger, eventType);
}

// Helper to update all previous tracking values after recording
#define UPDATE_ALL_PREV() do { \
    prevTemperature = temperature_f; \
    prevFlowRate = flow_rate; \
    prevPumpActive = pump_active; \
    prevLightBrightness = light_brightness; \
    copySafe(prevDoorState, sizeof(prevDoorState), door_state); \
    copySafe(prevDoorPosition, sizeof(prevDoorPosition), door_position); \
} while(0)

void HistoricalDataManager::checkAndRecord(float temperature_f, bool pump_active, float flow_rate,
                                            uint8_t light_brightness, const char* door_state,
                                            const char* door_position, const char* pump_trigger,
                                            const char* door_trigger, const char* light_trigger) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled) return;

    const char* safeDoorState = door_state ? door_state : "UNKNOWN";
    const char* safeDoorPos = door_position ? door_position : "UNKNOWN";
    unsigned long currentTime = millis() / 1000;

    // On first call, record initial state and initialize tracking
    if (firstUpdate) {
        firstUpdate = false;
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "temp"));
        return;
    }

    // Check for pump state change (immediate)
    if (pump_active != prevPumpActive) {
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "pump"));
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for door state/position change (immediate)
    if (strcmp(prevDoorState, safeDoorState) != 0 ||
        strcmp(prevDoorPosition, safeDoorPos) != 0) {
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "door"));
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for light brightness change (immediate)
    if (light_brightness != prevLightBrightness) {
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "light"));
        UPDATE_ALL_PREV();
        lastTempRecordTime = currentTime;
        lastFlowRecordTime = currentTime;
        return;
    }

    // Check for flow rate change (with min interval)
    float flowDelta = fabs(flow_rate - prevFlowRate);
    if (flowDelta > 0.001f && (currentTime - lastFlowRecordTime >= flowMinIntervalSeconds)) {
        lastFlowRecordTime = currentTime;
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "flow"));
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
        addPointLocked(createCompactPoint(temperature_f, pump_active, flow_rate, light_brightness,
                                          safeDoorState, safeDoorPos, pump_trigger, door_trigger,
                                          light_trigger, "temp"));
        UPDATE_ALL_PREV();
        lastFlowRecordTime = currentTime;
        return;
    }
}

#undef UPDATE_ALL_PREV

String HistoricalDataManager::getDataAsJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0) {
        return "[]";
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    size_t startIndex = count_ < maxSize ? 0 : currentIndex;

    for (size_t i = 0; i < count_; i++) {
        size_t index = (startIndex + i) % count_;
        DataPoint point = unpackPoint(slotAtLocked(index));

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
    std::lock_guard<std::mutex> lock(mutex_);
    String csv = "timestamp,temperature_f,pump_active,flow_rate,light_brightness,door_state,door_position,pump_trigger,door_trigger,light_trigger,event_type\n";

    if (count_ == 0) {
        return csv;
    }

    size_t startIndex = count_ < maxSize ? 0 : currentIndex;

    for (size_t i = 0; i < count_; i++) {
        size_t index = (startIndex + i) % count_;
        DataPoint point = unpackPoint(slotAtLocked(index));

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

bool HistoricalDataManager::getOrderedDataPoint(size_t orderedIdx, DataPoint& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0 || orderedIdx >= count_) {
        return false;
    }
    size_t startIndex = count_ < maxSize ? 0 : currentIndex;
    size_t rawIdx = (startIndex + orderedIdx) % count_;
    out = unpackPoint(slotAtLocked(rawIdx));
    return true;
}

size_t HistoricalDataManager::getOrderedIndex(size_t i) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0) return 0;
    size_t startIndex = count_ < maxSize ? 0 : currentIndex;
    return (startIndex + i) % count_;
}

const DataPoint& HistoricalDataManager::getDataPointAt(size_t rawIndex) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rawIndex >= count_) {
        legacyReadScratch_ = DataPoint();
        return legacyReadScratch_;
    }
    legacyReadScratch_ = unpackPoint(slotAtLocked(rawIndex));
    return legacyReadScratch_;
}

void HistoricalDataManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    chunks_.clear();
    chunks_.shrink_to_fit();
    count_ = 0;
    currentIndex = 0;
    firstUpdate = true;
}

size_t HistoricalDataManager::getDataPointCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

size_t HistoricalDataManager::getBufferCapacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return maxSize;
}

void HistoricalDataManager::setBufferSize(size_t newSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (newSize > HISTORY_MAX_BUFFER_SIZE) {
        newSize = HISTORY_MAX_BUFFER_SIZE;
    }
    if (newSize == maxSize) return;

    if (count_ > newSize) {
        // Keep the most recent `newSize` entries in compact chunked storage.
        std::vector<std::unique_ptr<CompactDataPoint[]>> newChunks;
        size_t neededChunks = (newSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
        try {
            newChunks.reserve(neededChunks);
            for (size_t c = 0; c < neededChunks; c++) {
                auto* block = new (std::nothrow) CompactDataPoint[CHUNK_SIZE];
                if (!block) {
                    // OOM during shrink: keep existing buffer untouched
                    return;
                }
                newChunks.emplace_back(block);
            }
        } catch (...) {
            return;
        }

        size_t startIndex = count_ < maxSize ? 0 : currentIndex;
        size_t skip = count_ - newSize;
        for (size_t i = 0; i < newSize; i++) {
            size_t oldIdx = (startIndex + skip + i) % count_;
            newChunks[i / CHUNK_SIZE][i % CHUNK_SIZE] = slotAtLocked(oldIdx);
        }
        chunks_ = std::move(newChunks);
        count_ = newSize;
        currentIndex = 0; // Buffer is now linear and full
    }
    maxSize = newSize;
}

bool HistoricalDataManager::isEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled;
}

void HistoricalDataManager::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled = enable;
}

unsigned int HistoricalDataManager::getTempMinInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tempMinIntervalSeconds;
}

void HistoricalDataManager::setTempMinInterval(unsigned int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    tempMinIntervalSeconds = seconds;
}

unsigned int HistoricalDataManager::getFlowMinInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return flowMinIntervalSeconds;
}

void HistoricalDataManager::setFlowMinInterval(unsigned int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    flowMinIntervalSeconds = seconds;
}
