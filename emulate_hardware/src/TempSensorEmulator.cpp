#include "TempSensorEmulator.h"
#include <math.h>

// Global instance
TempSensorEmulator tempSensorEmulator;

void TempSensorEmulator::begin() {
  // Initialize with sensible defaults
  _sensors[0].enabled = true;
  _sensors[0].temperatureC = 20.0f;
  _sensors[0].temperatureF = celsiusToFahrenheit(20.0f);
  _sensors[0].baseTemperatureC = 20.0f;

  _sensors[1].enabled = true;
  _sensors[1].temperatureC = 22.0f;
  _sensors[1].temperatureF = celsiusToFahrenheit(22.0f);
  _sensors[1].baseTemperatureC = 22.0f;

  _driftStartTime = millis();

  Serial.println("[TempSensor] Initialized with 2 sensors");
}

void TempSensorEmulator::update() {
  for (uint8_t i = 0; i < MAX_TEMP_SENSORS; i++) {
    if (_sensors[i].enabled && _sensors[i].driftEnabled && !_sensors[i].disconnected) {
      updateDrift(i);
    }
  }
}

// ============================================================================
// SENSOR CONTROL
// ============================================================================

void TempSensorEmulator::setSensorEnabled(uint8_t sensorIndex, bool enabled) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  _sensors[sensorIndex].enabled = enabled;
  Serial.printf("[TempSensor] Sensor %u %s\n", sensorIndex, enabled ? "enabled" : "disabled");
}

void TempSensorEmulator::setTemperature(uint8_t sensorIndex, float tempC) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  _sensors[sensorIndex].temperatureC = tempC;
  _sensors[sensorIndex].temperatureF = celsiusToFahrenheit(tempC);
  _sensors[sensorIndex].baseTemperatureC = tempC;
  Serial.printf("[TempSensor] Sensor %u set to %.2f C (%.2f F)\n",
                sensorIndex, tempC, _sensors[sensorIndex].temperatureF);
}

void TempSensorEmulator::setDisconnected(uint8_t sensorIndex, bool disconnected) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  _sensors[sensorIndex].disconnected = disconnected;
  Serial.printf("[TempSensor] Sensor %u %s\n",
                sensorIndex, disconnected ? "disconnected" : "reconnected");
}

void TempSensorEmulator::setDriftEnabled(uint8_t sensorIndex, bool enabled) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  _sensors[sensorIndex].driftEnabled = enabled;
  if (enabled) {
    // Reset drift phase to start from current temperature
    _sensors[sensorIndex].baseTemperatureC = _sensors[sensorIndex].temperatureC;
  }
}

void TempSensorEmulator::setDriftAmplitude(uint8_t sensorIndex, float amplitudeC) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  _sensors[sensorIndex].driftAmplitude = amplitudeC;
}

void TempSensorEmulator::setDriftPeriod(uint8_t sensorIndex, uint32_t periodMs) {
  if (sensorIndex >= MAX_TEMP_SENSORS) return;
  if (periodMs == 0) periodMs = TEMP_DRIFT_PERIOD_MS;
  _sensors[sensorIndex].driftPeriodMs = periodMs;
}

// ============================================================================
// SENSOR STATE ACCESS
// ============================================================================

const TempSensorState* TempSensorEmulator::getSensor(uint8_t sensorIndex) const {
  if (sensorIndex >= MAX_TEMP_SENSORS) return nullptr;
  return &_sensors[sensorIndex];
}

bool TempSensorEmulator::isSensorEnabled(uint8_t sensorIndex) const {
  if (sensorIndex >= MAX_TEMP_SENSORS) return false;
  return _sensors[sensorIndex].enabled;
}

float TempSensorEmulator::getTemperatureC(uint8_t sensorIndex) const {
  if (sensorIndex >= MAX_TEMP_SENSORS) return 0.0f;
  return _sensors[sensorIndex].temperatureC;
}

float TempSensorEmulator::getTemperatureF(uint8_t sensorIndex) const {
  if (sensorIndex >= MAX_TEMP_SENSORS) return 0.0f;
  return _sensors[sensorIndex].temperatureF;
}

bool TempSensorEmulator::isDisconnected(uint8_t sensorIndex) const {
  if (sensorIndex >= MAX_TEMP_SENSORS) return true;
  return _sensors[sensorIndex].disconnected;
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

void TempSensorEmulator::toJson(JsonObject& obj) const {
  for (uint8_t i = 0; i < MAX_TEMP_SENSORS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "sensor%u", i + 1);
    JsonObject sensor = obj[key].to<JsonObject>();

    sensor["enabled"] = _sensors[i].enabled;
    sensor["temperature_c"] = _sensors[i].temperatureC;
    sensor["temperature_f"] = _sensors[i].temperatureF;
    sensor["base_temperature_c"] = _sensors[i].baseTemperatureC;
    sensor["disconnected"] = _sensors[i].disconnected;
    sensor["drift_enabled"] = _sensors[i].driftEnabled;
    sensor["drift_amplitude_c"] = _sensors[i].driftAmplitude;
    sensor["drift_period_ms"] = _sensors[i].driftPeriodMs;
  }
}

void TempSensorEmulator::fromJson(const JsonObject& json) {
  for (uint8_t i = 0; i < MAX_TEMP_SENSORS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "sensor%u", i + 1);

    if (!json[key].is<JsonObject>()) continue;
    JsonObject sensor = json[key].as<JsonObject>();

    if (sensor["enabled"].is<bool>()) {
      _sensors[i].enabled = sensor["enabled"];
    }
    if (sensor["temperature_c"].is<float>() || sensor["temperature_c"].is<int>()) {
      float tempC = sensor["temperature_c"].as<float>();
      _sensors[i].temperatureC = tempC;
      _sensors[i].temperatureF = celsiusToFahrenheit(tempC);
      _sensors[i].baseTemperatureC = tempC;
    }
    if (sensor["disconnected"].is<bool>()) {
      _sensors[i].disconnected = sensor["disconnected"];
    }
    if (sensor["drift_enabled"].is<bool>()) {
      _sensors[i].driftEnabled = sensor["drift_enabled"];
    }
    if (sensor["drift_amplitude_c"].is<float>() || sensor["drift_amplitude_c"].is<int>()) {
      _sensors[i].driftAmplitude = sensor["drift_amplitude_c"].as<float>();
    }
    if (sensor["drift_period_ms"].is<int>()) {
      _sensors[i].driftPeriodMs = sensor["drift_period_ms"].as<uint32_t>();
    }
  }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void TempSensorEmulator::updateDrift(uint8_t sensorIndex) {
  uint32_t elapsed = millis() - _driftStartTime;
  float phase = static_cast<float>(elapsed % _sensors[sensorIndex].driftPeriodMs) /
                static_cast<float>(_sensors[sensorIndex].driftPeriodMs);
  float drift = _sensors[sensorIndex].driftAmplitude * sinf(2.0f * static_cast<float>(M_PI) * phase);

  _sensors[sensorIndex].temperatureC = _sensors[sensorIndex].baseTemperatureC + drift;
  _sensors[sensorIndex].temperatureF = celsiusToFahrenheit(_sensors[sensorIndex].temperatureC);
}
