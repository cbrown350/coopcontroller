#ifndef TEMP_SENSOR_EMULATOR_H
#define TEMP_SENSOR_EMULATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Maximum number of temperature sensors that can be emulated
 */
constexpr uint8_t MAX_TEMP_SENSORS = 2;

/**
 * @brief Temperature drift simulation amplitude in degrees C
 */
constexpr float TEMP_DRIFT_AMPLITUDE_C = 0.5f;

/**
 * @brief Temperature drift simulation period in milliseconds (60 seconds)
 */
constexpr uint32_t TEMP_DRIFT_PERIOD_MS = 60000;

/**
 * @brief Emulated temperature sensor state
 */
struct TempSensorState {
  bool enabled = false;             // Whether this sensor is active
  float temperatureC = 20.0f;       // Current temperature in Celsius
  float temperatureF = 68.0f;       // Current temperature in Fahrenheit
  bool driftEnabled = false;        // Whether temperature drift is simulated
  float driftAmplitude = TEMP_DRIFT_AMPLITUDE_C;  // Drift amplitude in C
  uint32_t driftPeriodMs = TEMP_DRIFT_PERIOD_MS;  // Drift period in ms
  bool disconnected = false;        // Simulate disconnected sensor
  float baseTemperatureC = 20.0f;   // Base temperature (drift oscillates around this)
};

/**
 * @brief Emulates Dallas DS18B20 temperature sensors
 *
 * Since full 1-Wire slave emulation is extremely timing-critical and
 * unreliable on ESP32, this class provides temperature values via the
 * REST API. The emulator outputs configurable temperature readings that
 * the web UI and test harness can query.
 *
 * For physical 1-Wire emulation, an external DS18B20 sensor or a dedicated
 * 1-Wire slave device would be needed. This emulator provides the logical
 * temperature values for scenarios and testing via the API layer.
 *
 * The emulator supports:
 * - Per-sensor enable/disable
 * - Configurable temperature values
 * - Temperature drift simulation (sinusoidal oscillation)
 * - Sensor disconnect simulation
 * - Celsius and Fahrenheit output
 */
class TempSensorEmulator {
public:
  TempSensorEmulator() = default;

  /**
   * @brief Initialize the temperature sensor emulator
   */
  void begin();

  /**
   * @brief Update loop - call from main loop to update drift simulation
   */
  void update();

  // ========================================================================
  // SENSOR CONTROL
  // ========================================================================

  /**
   * @brief Enable or disable a sensor
   * @param sensorIndex 0 or 1
   * @param enabled Whether the sensor should be active
   */
  void setSensorEnabled(uint8_t sensorIndex, bool enabled);

  /**
   * @brief Set the temperature for a sensor
   * @param sensorIndex 0 or 1
   * @param tempC Temperature in Celsius
   */
  void setTemperature(uint8_t sensorIndex, float tempC);

  /**
   * @brief Simulate a disconnected sensor
   * @param sensorIndex 0 or 1
   * @param disconnected True to simulate disconnect
   */
  void setDisconnected(uint8_t sensorIndex, bool disconnected);

  /**
   * @brief Enable or disable temperature drift simulation
   * @param sensorIndex 0 or 1
   * @param enabled Whether drift is active
   */
  void setDriftEnabled(uint8_t sensorIndex, bool enabled);

  /**
   * @brief Set drift amplitude
   * @param sensorIndex 0 or 1
   * @param amplitudeC Drift amplitude in Celsius
   */
  void setDriftAmplitude(uint8_t sensorIndex, float amplitudeC);

  /**
   * @brief Set drift period
   * @param sensorIndex 0 or 1
   * @param periodMs Drift period in milliseconds
   */
  void setDriftPeriod(uint8_t sensorIndex, uint32_t periodMs);

  // ========================================================================
  // SENSOR STATE ACCESS
  // ========================================================================

  /**
   * @brief Get the current state of a sensor
   * @param sensorIndex 0 or 1
   * @return Pointer to sensor state, or nullptr if invalid index
   */
  const TempSensorState* getSensor(uint8_t sensorIndex) const;

  /**
   * @brief Check if a sensor is enabled
   */
  bool isSensorEnabled(uint8_t sensorIndex) const;

  /**
   * @brief Get current temperature in Celsius
   */
  float getTemperatureC(uint8_t sensorIndex) const;

  /**
   * @brief Get current temperature in Fahrenheit
   */
  float getTemperatureF(uint8_t sensorIndex) const;

  /**
   * @brief Check if sensor is simulating disconnect
   */
  bool isDisconnected(uint8_t sensorIndex) const;

  // ========================================================================
  // JSON SERIALIZATION
  // ========================================================================

  /**
   * @brief Serialize all sensor states to JSON
   */
  void toJson(JsonObject& obj) const;

  /**
   * @brief Apply sensor configuration from JSON
   * @param json JSON object with sensor configuration
   */
  void fromJson(const JsonObject& json);

private:
  TempSensorState _sensors[MAX_TEMP_SENSORS];
  uint32_t _driftStartTime = 0;

  /**
   * @brief Convert Celsius to Fahrenheit
   */
  static float celsiusToFahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }

  /**
   * @brief Update drift for a specific sensor
   */
  void updateDrift(uint8_t sensorIndex);
};

// Global instance
extern TempSensorEmulator tempSensorEmulator;

#endif // TEMP_SENSOR_EMULATOR_H
