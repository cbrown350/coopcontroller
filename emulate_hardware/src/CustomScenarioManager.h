#ifndef CUSTOM_SCENARIO_MANAGER_H
#define CUSTOM_SCENARIO_MANAGER_H

#include "EmulatorStateManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>


/**
 * @brief File path for custom scenarios storage
 */
constexpr const char *CUSTOM_SCENARIOS_FILE = "/custom_scenarios.json";

/**
 * @brief Custom ID for saved scenarios (starts after predefined IDs)
 */
constexpr uint8_t CUSTOM_SCENARIO_ID_START = 100;

/**
 * @brief Manages persistent storage of user-created custom scenarios
 */
class CustomScenarioManager {
public:
  CustomScenarioManager() = default;

  /**
   * @brief Initialize and load custom scenarios from LittleFS
   * @return true if initialization successful
   */
  bool begin();

  /**
   * @brief Load custom scenarios from filesystem
   * @return true if loaded successfully
   */
  bool load();

  /**
   * @brief Save custom scenarios to filesystem
   * @return true if saved successfully
   */
  bool save();

  /**
   * @brief Get all custom scenarios
   */
  const std::vector<Scenario> &getCustomScenarios() const {
    return _customScenarios;
  }

  /**
   * @brief Get number of custom scenarios
   */
  size_t getCount() const { return _customScenarios.size(); }

  /**
   * @brief Get a custom scenario by index
   * @param index Index in the custom scenarios list
   * @return Pointer to scenario or nullptr if not found
   */
  const Scenario *getByIndex(size_t index) const;

  /**
   * @brief Get a custom scenario by name
   * @param name Scenario name
   * @return Pointer to scenario or nullptr if not found
   */
  const Scenario *getByName(const char *name) const;

  /**
   * @brief Add or update a custom scenario
   * @param scenario Scenario to save (will use name to check for update)
   * @return true if saved successfully
   */
  bool saveCustomScenario(const Scenario &scenario);

  /**
   * @brief Delete a custom scenario by name
   * @param name Name of scenario to delete
   * @return true if deleted successfully
   */
  bool deleteByName(const char *name);

  /**
   * @brief Delete a custom scenario by index
   * @param index Index of scenario to delete
   * @return true if deleted successfully
   */
  bool deleteByIndex(size_t index);

  /**
   * @brief Delete all custom scenarios
   */
  void deleteAll();

  /**
   * @brief Serialize all custom scenarios to JSON array
   */
  void toJsonArray(JsonArray &arr) const;

  /**
   * @brief Parse a scenario from JSON object
   * @param json JSON object containing scenario data
   * @param scenario Output scenario struct
   * @return true if parsed successfully
   */
  static bool fromJson(const JsonObject &json, Scenario &scenario);

  /**
   * @brief Serialize a single scenario to JSON object
   */
  static void scenarioToJson(const Scenario &scenario, JsonObject &obj);

private:
  std::vector<Scenario> _customScenarios;
  bool _initialized = false;

  /**
   * @brief Find index of scenario by name
   * @return Index or -1 if not found
   */
  int findByName(const char *name) const;
};

// Global instance
extern CustomScenarioManager customScenarioManager;

#endif // CUSTOM_SCENARIO_MANAGER_H
