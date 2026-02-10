#ifndef HISTORICAL_DATA_MANAGER_H
#define HISTORICAL_DATA_MANAGER_H

#include <Arduino.h>
#include <vector>

/**
 * @brief Single data point in historical data
 *
 * Stores timestamp and sensor/controller values for a single moment in time.
 * Includes state information and trigger sources for debugging and analysis.
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
    bool is_event;                 ///< true if this is an event capture, false if periodic sample

    DataPoint()
        : timestamp(0)
        , temperature_f(NAN)
        , pump_active(false)
        , flow_rate(0.0f)
        , light_brightness(0)
        , is_event(false)
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
    }
};

/**
 * @brief Manager for historical sensor and controller data
 *
 * Collects and stores historical data in RAM using a circular buffer.
 * Provides JSON export and CSV download functionality.
 *
 * Features:
 * - Configurable sample interval (default 60 seconds)
 * - Configurable buffer size (default 1440 samples = 24 hours at 60s)
 * - Circular buffer automatically overwrites oldest data
 * - CSV export with proper headers
 * - JSON export for web visualization
 * - Minimal RAM footprint (~35KB for 24 hours of data)
 *
 * Memory Usage:
 * - Each DataPoint: ~80 bytes (with fixed char arrays)
 * - Default buffer (1440 samples): ~115KB
 *
 * Future Enhancements:
 * - Remote database storage (InfluxDB, PostgreSQL, etc.)
 * - Compression for longer retention
 * - SD card backup
 */
class HistoricalDataManager {
private:
    std::vector<DataPoint> buffer;           ///< Circular buffer for data points
    size_t maxSize;                          ///< Maximum buffer size
    size_t currentIndex;                     ///< Current write position
    unsigned long lastSampleTime;            ///< Last time a sample was taken
    unsigned int sampleIntervalSeconds;      ///< How often to sample
    bool enabled;                            ///< Enable/disable data collection

    void addPoint(const DataPoint& point);
    DataPoint createPoint(float temperature_f, bool pump_active, float flow_rate,
                          uint8_t light_brightness, const String& door_state,
                          const String& door_position, const String& pump_trigger,
                          const String& door_trigger, const String& light_trigger,
                          bool isEvent);

public:
    /**
     * @brief Constructor
     */
    HistoricalDataManager();

    /**
     * @brief Initialize with settings
     *
     * @param enabled Enable data collection
     * @param bufferSize Maximum number of data points to store
     * @param intervalSeconds Sample interval in seconds
     */
    void begin(bool enabled, size_t bufferSize, unsigned int intervalSeconds);

    /**
     * @brief Update - call this in loop()
     *
     * Checks if it's time to take a new sample and stores it.
     *
     * @param temperature_f Current temperature (Fahrenheit)
     * @param pump_active Current pump state
     * @param flow_rate Current flow rate (GPM)
     * @param light_brightness Current light brightness (0-100)
     * @param door_state Current door state string (OPEN, CLOSED, OPENING, etc.)
     * @param door_position Current door position string (OPEN, CLOSED, PARTIAL, UNKNOWN)
     * @param pump_trigger What triggered last pump state change
     * @param door_trigger What triggered last door state change
     * @param light_trigger What triggered last light state change
     */
    void update(float temperature_f, bool pump_active, float flow_rate, uint8_t light_brightness,
                const String& door_state, const String& door_position, const String& pump_trigger,
                const String& door_trigger, const String& light_trigger);

    /**
     * @brief Record an event immediately (bypasses sample interval)
     *
     * Use this to capture state changes that happen between periodic samples,
     * such as door open/close, pump on/off, or flow changes.
     * Events are stored in the same buffer as samples but marked with is_event=true.
     *
     * @param temperature_f Current temperature (Fahrenheit)
     * @param pump_active Current pump state
     * @param flow_rate Current flow rate (GPM)
     * @param light_brightness Current light brightness (0-100)
     * @param door_state Current door state string
     * @param door_position Current door position string
     * @param pump_trigger What triggered last pump state change
     * @param door_trigger What triggered last door state change
     * @param light_trigger What triggered last light state change
     */
    void recordEvent(float temperature_f, bool pump_active, float flow_rate, uint8_t light_brightness,
                     const String& door_state, const String& door_position, const String& pump_trigger,
                     const String& door_trigger, const String& light_trigger);

    /**
     * @brief Get all data points as JSON array string
     *
     * Returns JSON array of all stored data points, ordered from oldest to newest.
     * Format: [{"timestamp":123,"temperature_f":45.6,"pump_active":true,...},...]
     *
     * @return JSON string
     */
    String getDataAsJson() const;

    /**
     * @brief Get all data points as CSV string
     *
     * Returns CSV formatted data with headers.
     * Format: timestamp,temperature_f,pump_active,flow_rate,light_brightness,door_state,pump_trigger,door_trigger,light_trigger
     *
     * @return CSV string
     */
    String getDataAsCsv() const;

    /**
     * @brief Clear all stored data
     */
    void clear();

    /**
     * @brief Get number of stored data points
     *
     * @return Number of data points currently in buffer
     */
    size_t getDataPointCount() const;

    /**
     * @brief Get buffer capacity
     *
     * @return Maximum number of data points buffer can hold
     */
    size_t getBufferCapacity() const { return maxSize; }

    /**
     * @brief Check if data collection is enabled
     *
     * @return true if enabled
     */
    bool isEnabled() const { return enabled; }

    /**
     * @brief Enable/disable data collection
     *
     * @param enable true to enable, false to disable
     */
    void setEnabled(bool enable) { enabled = enable; }

    /**
     * @brief Get sample interval
     *
     * @return Sample interval in seconds
     */
    unsigned int getSampleInterval() const { return sampleIntervalSeconds; }

    /**
     * @brief Set sample interval
     *
     * @param seconds New sample interval
     */
    void setSampleInterval(unsigned int seconds) { sampleIntervalSeconds = seconds; }
};

#endif // HISTORICAL_DATA_MANAGER_H
