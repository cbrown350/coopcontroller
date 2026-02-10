#ifndef TRIGGER_SOURCE_H
#define TRIGGER_SOURCE_H

#include <Arduino.h>

/**
 * @brief Trigger source for state changes
 *
 * Identifies what caused a state change in a controller.
 * Used for historical data tracking and debugging.
 */
enum class TriggerSource {
    UNKNOWN,      ///< Unknown or uninitialized
    MANUAL,       ///< Manual control via physical switch/button
    WEB_UI,       ///< Web interface control
    API,          ///< REST API call
    AUTOMATIC,    ///< Automatic scheduled operation (sunrise/sunset, timer, etc.)
    SENSOR,       ///< Triggered by sensor reading (temperature, flow, etc.)
    STARTUP,      ///< Initial state on system startup
    FAULT,        ///< State change due to fault/error condition
    TEST          ///< Test mode operation
};

/**
 * @brief Convert TriggerSource to string representation
 *
 * @param source The trigger source to convert
 * @return String name of the trigger source
 */
inline String triggerSourceToString(TriggerSource source) {
    switch (source) {
        case TriggerSource::UNKNOWN:    return "unknown";
        case TriggerSource::MANUAL:     return "manual";
        case TriggerSource::WEB_UI:     return "web";
        case TriggerSource::API:        return "api";
        case TriggerSource::AUTOMATIC:  return "auto";
        case TriggerSource::SENSOR:     return "sensor";
        case TriggerSource::STARTUP:    return "startup";
        case TriggerSource::FAULT:      return "fault";
        case TriggerSource::TEST:       return "test";
        default:                        return "unknown";
    }
}

#endif // TRIGGER_SOURCE_H
