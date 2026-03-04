#ifndef TRIGGER_SOURCE_H
#define TRIGGER_SOURCE_H

#include <Arduino.h>

/**
 * @brief Trigger source for state changes
 *
 * Identifies what caused a state change in a controller.
 * Used for historical data tracking and debugging.
 * Each value provides specific context about the exact cause.
 */
enum class TriggerSource {
    UNKNOWN,              ///< Unknown or uninitialized
    MANUAL_BUTTON,        ///< Physical button/switch press
    WEB_UI,               ///< Web interface button
    API,                  ///< REST API call (direct, not web UI)
    SUNRISE,              ///< Scheduled: sunrise-based open
    SUNSET,               ///< Scheduled: sunset-based close
    AUTO_CLOSE_SUNSET,    ///< Scheduled: auto-close after sunset delay
    TIMER,                ///< Timer-based action (light timer, etc.)
    TEMP_THRESHOLD,       ///< Temperature crossed threshold
    TEMP_CYCLE,           ///< Temperature-based pump cycling (on/off phase)
    FLOW_FAULT,           ///< Flow error detected
    MAINTENANCE_CYCLE,    ///< Scheduled maintenance pump cycle
    STARTUP,              ///< Initial state on system startup
    FAULT,                ///< State change due to fault/error condition
    TEST                  ///< Test mode operation
};

/**
 * @brief Convert TriggerSource to string representation
 *
 * @param source The trigger source to convert
 * @return String name of the trigger source
 */
inline const char* triggerSourceToCStr(TriggerSource source) {
    switch (source) {
        case TriggerSource::UNKNOWN:            return "unknown";
        case TriggerSource::MANUAL_BUTTON:      return "button";
        case TriggerSource::WEB_UI:             return "web";
        case TriggerSource::API:                return "api";
        case TriggerSource::SUNRISE:            return "sunrise";
        case TriggerSource::SUNSET:             return "sunset";
        case TriggerSource::AUTO_CLOSE_SUNSET:  return "auto_sunset";
        case TriggerSource::TIMER:              return "timer";
        case TriggerSource::TEMP_THRESHOLD:     return "temp_threshold";
        case TriggerSource::TEMP_CYCLE:         return "temp_cycle";
        case TriggerSource::FLOW_FAULT:         return "flow_fault";
        case TriggerSource::MAINTENANCE_CYCLE:  return "maintenance";
        case TriggerSource::STARTUP:            return "startup";
        case TriggerSource::FAULT:              return "fault";
        case TriggerSource::TEST:               return "test";
        default:                                return "unknown";
    }
}

inline String triggerSourceToString(TriggerSource source) {
    return String(triggerSourceToCStr(source));
}

#endif // TRIGGER_SOURCE_H
