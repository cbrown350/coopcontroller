#ifndef HISTORICAL_DATA_MANAGER_H
#define HISTORICAL_DATA_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"

/**
 * @brief Single data point in historical data
 *
 * Stores timestamp and sensor/controller values for a single moment in time.
 * Each point is a full snapshot captured when a meaningful state change occurs.
 * Uses fixed-size char arrays instead of String for vector safety.
 */
struct DataPoint {
    unsigned long timestamp;       ///< Unix timestamp (seconds since epoch or boot)
    float temperature_f;           ///< Temperature in Fahrenheit
    bool pump_active;              ///< Pump state (on/off)
    float flow_rate;               ///< Water flow rate (GPM)
    uint8_t light_brightness;      ///< Light brightness (0-100%)
    char door_state[16];           ///< Door state (OPEN, CLOSED, OPENING, CLOSING, etc.)
    char door_position[16];        ///< Door position (OPEN, CLOSED, PARTIAL, UNKNOWN)
    char pump_trigger[16];         ///< What triggered last pump state change
    char door_trigger[16];         ///< What triggered last door state change
    char light_trigger[16];        ///< What triggered last light state change
    char event_type[16];           ///< What triggered this recording (temp, flow, pump, light, door)

    DataPoint()
        : timestamp(0)
        , temperature_f(NAN)
        , pump_active(false)
        , flow_rate(0.0f)
        , light_brightness(0)
    {
        strncpy(door_state, "UNKNOWN", sizeof(door_state) - 1);
        door_state[sizeof(door_state) - 1] = '\0';
        strncpy(door_position, "UNKNOWN", sizeof(door_position) - 1);
        door_position[sizeof(door_position) - 1] = '\0';
        strncpy(pump_trigger, "unknown", sizeof(pump_trigger) - 1);
        pump_trigger[sizeof(pump_trigger) - 1] = '\0';
        strncpy(door_trigger, "unknown", sizeof(door_trigger) - 1);
        door_trigger[sizeof(door_trigger) - 1] = '\0';
        strncpy(light_trigger, "unknown", sizeof(light_trigger) - 1);
        light_trigger[sizeof(light_trigger) - 1] = '\0';
        event_type[0] = '\0';
    }
};

/**
 * @brief Manager for historical sensor and controller data (event-based)
 *
 * Collects and stores historical data in RAM using a circular buffer.
 * Data is captured on state changes rather than at fixed intervals:
 * - Pump, light, door: recorded immediately on any state change
 * - Temperature: recorded when change >= 0.5F with configurable min interval
 * - Flow rate: recorded when change > 0.001 GPM with configurable min interval
 *
 * Features:
 * - Event-based capture dramatically reduces data point count
 * - Configurable minimum intervals for temp and flow recordings
 * - Circular buffer automatically overwrites oldest data
 * - CSV and JSON export methods
 */
class HistoricalDataManager {
private:
    std::vector<DataPoint> buffer;           ///< Circular buffer for data points
    size_t maxSize;                          ///< Maximum buffer size
    size_t currentIndex;                     ///< Current write position
    bool enabled;                            ///< Enable/disable data collection

    // Change detection tracking
    float prevTemperature;
    float prevFlowRate;
    bool prevPumpActive;
    uint8_t prevLightBrightness;
    char prevDoorState[16];
    char prevDoorPosition[16];
    unsigned long lastTempRecordTime;        ///< millis()/1000 of last temp-triggered record
    unsigned long lastFlowRecordTime;        ///< millis()/1000 of last flow-triggered record
    unsigned int tempMinIntervalSeconds;     ///< Min interval for temp recordings (default 60)
    unsigned int flowMinIntervalSeconds;     ///< Min interval for flow recordings (default 10)
    bool firstUpdate;                        ///< Always record on first call

    void addPoint(const DataPoint& point);
    DataPoint createPoint(float temperature_f, bool pump_active, float flow_rate,
                          uint8_t light_brightness, const char* door_state,
                          const char* door_position, const char* pump_trigger,
                          const char* door_trigger, const char* light_trigger,
                          const char* eventType);

public:
    HistoricalDataManager();

    /**
     * @brief Initialize with settings
     *
     * @param enabled Enable data collection
     * @param bufferSize Maximum number of data points to store
     * @param tempMinIntervalSec Minimum seconds between temperature recordings
     * @param flowMinIntervalSec Minimum seconds between flow rate recordings
     */
    void begin(bool enabled, size_t bufferSize, unsigned int tempMinIntervalSec, unsigned int flowMinIntervalSec);

    /**
     * @brief Check current state and record if anything changed
     *
     * Call this frequently (every loop iteration or controller update).
     * Internally detects what changed and records a full snapshot when:
     * - Pump state changed (immediate)
     * - Door state/position changed (immediate)
     * - Light brightness changed (immediate)
     * - Flow rate changed by >0.001 GPM (with min interval)
     * - Temperature changed by >=0.5F (with min interval)
     */
    void checkAndRecord(float temperature_f, bool pump_active, float flow_rate,
                        uint8_t light_brightness, const char* door_state,
                        const char* door_position, const char* pump_trigger,
                        const char* door_trigger, const char* light_trigger);

    String getDataAsJson() const;
    String getDataAsCsv() const;

    /**
     * @brief Get ordered index for iteration (handles circular buffer wrapping)
     * @param i Sequential index (0 = oldest point)
     * @return Raw buffer index
     */
    size_t getOrderedIndex(size_t i) const;

    /**
     * @brief Get const reference to a data point by raw buffer index
     * @param rawIndex Raw buffer index from getOrderedIndex()
     * @return Reference to the data point
     */
    const DataPoint& getDataPointAt(size_t rawIndex) const;

    void clear();
    size_t getDataPointCount() const;
    size_t getBufferCapacity() const { return maxSize; }
    void setBufferSize(size_t newSize);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool enable) { enabled = enable; }
    unsigned int getTempMinInterval() const { return tempMinIntervalSeconds; }
    void setTempMinInterval(unsigned int seconds) { tempMinIntervalSeconds = seconds; }
    unsigned int getFlowMinInterval() const { return flowMinIntervalSeconds; }
    void setFlowMinInterval(unsigned int seconds) { flowMinIntervalSeconds = seconds; }
};

#endif // HISTORICAL_DATA_MANAGER_H
