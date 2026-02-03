#include "CustomScenarioManager.h"

// Global instance
CustomScenarioManager customScenarioManager;

bool CustomScenarioManager::begin() {
  if (_initialized) {
    return true;
  }

  // LittleFS should already be initialized by EmulatorSettings
  if (!LittleFS.begin(false)) {
    Serial.println("[CustomScenario] LittleFS mount failed");
    return false;
  }

  _initialized = true;
  return load();
}

bool CustomScenarioManager::load() {
  if (!LittleFS.exists(CUSTOM_SCENARIOS_FILE)) {
    Serial.println("[CustomScenario] No saved scenarios found");
    _customScenarios.clear();
    return true; // Not an error - just no saved scenarios yet
  }

  File file = LittleFS.open(CUSTOM_SCENARIOS_FILE, "r");
  if (!file) {
    Serial.println("[CustomScenario] Failed to open scenarios file");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[CustomScenario] JSON parse error: %s\n", error.c_str());
    return false;
  }

  _customScenarios.clear();

  JsonArray arr = doc["scenarios"].as<JsonArray>();
  for (JsonObject obj : arr) {
    Scenario scenario;
    if (fromJson(obj, scenario)) {
      scenario.id = ScenarioId::CUSTOM;
      _customScenarios.push_back(scenario);
    }
  }

  Serial.printf("[CustomScenario] Loaded %d custom scenarios\n",
                _customScenarios.size());
  return true;
}

bool CustomScenarioManager::save() {
  JsonDocument doc;
  JsonArray arr = doc["scenarios"].to<JsonArray>();

  for (const auto &scenario : _customScenarios) {
    JsonObject obj = arr.add<JsonObject>();
    scenarioToJson(scenario, obj);
  }

  File file = LittleFS.open(CUSTOM_SCENARIOS_FILE, "w");
  if (!file) {
    Serial.println("[CustomScenario] Failed to create scenarios file");
    return false;
  }

  size_t written = serializeJson(doc, file);
  file.close();

  if (written == 0) {
    Serial.println("[CustomScenario] Failed to write scenarios");
    return false;
  }

  Serial.printf("[CustomScenario] Saved %d custom scenarios\n",
                _customScenarios.size());
  return true;
}

const Scenario *CustomScenarioManager::getByIndex(size_t index) const {
  if (index >= _customScenarios.size()) {
    return nullptr;
  }
  return &_customScenarios[index];
}

const Scenario *CustomScenarioManager::getByName(const char *name) const {
  int idx = findByName(name);
  if (idx < 0) {
    return nullptr;
  }
  return &_customScenarios[idx];
}

int CustomScenarioManager::findByName(const char *name) const {
  for (size_t i = 0; i < _customScenarios.size(); i++) {
    if (strcmp(_customScenarios[i].name, name) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool CustomScenarioManager::saveCustomScenario(const Scenario &scenario) {
  // Check if we already have a scenario with this name
  int existingIdx = findByName(scenario.name);

  if (existingIdx >= 0) {
    // Update existing
    _customScenarios[existingIdx] = scenario;
    _customScenarios[existingIdx].id = ScenarioId::CUSTOM;
    Serial.printf("[CustomScenario] Updated scenario: %s\n", scenario.name);
  } else {
    // Check max scenarios limit
    if (_customScenarios.size() >= MAX_CUSTOM_SCENARIOS) {
      Serial.println("[CustomScenario] Maximum custom scenarios reached");
      return false;
    }

    // Add new
    Scenario newScenario = scenario;
    newScenario.id = ScenarioId::CUSTOM;
    _customScenarios.push_back(newScenario);
    Serial.printf("[CustomScenario] Added new scenario: %s\n", scenario.name);
  }

  return save();
}

bool CustomScenarioManager::deleteByName(const char *name) {
  int idx = findByName(name);
  if (idx < 0) {
    Serial.printf("[CustomScenario] Scenario not found: %s\n", name);
    return false;
  }
  return deleteByIndex(static_cast<size_t>(idx));
}

bool CustomScenarioManager::deleteByIndex(size_t index) {
  if (index >= _customScenarios.size()) {
    return false;
  }

  Serial.printf("[CustomScenario] Deleting scenario: %s\n",
                _customScenarios[index].name);
  _customScenarios.erase(_customScenarios.begin() + index);
  return save();
}

void CustomScenarioManager::deleteAll() {
  _customScenarios.clear();
  LittleFS.remove(CUSTOM_SCENARIOS_FILE);
  Serial.println("[CustomScenario] All custom scenarios deleted");
}

void CustomScenarioManager::toJsonArray(JsonArray &arr) const {
  for (size_t i = 0; i < _customScenarios.size(); i++) {
    JsonObject obj = arr.add<JsonObject>();
    scenarioToJson(_customScenarios[i], obj);
    obj["index"] = i; // Include index for deletion
  }
}

bool CustomScenarioManager::fromJson(const JsonObject &json,
                                     Scenario &scenario) {
  // Name is required
  if (!json["name"].is<const char *>()) {
    return false;
  }

  strncpy(scenario.name, json["name"] | "Custom", sizeof(scenario.name) - 1);
  strncpy(scenario.description, json["description"] | "",
          sizeof(scenario.description) - 1);

  scenario.id = ScenarioId::CUSTOM;

  // Door settings
  scenario.autoSimulateDoor = json["auto_simulate_door"] | true;
  scenario.simulateDoorStuck = json["simulate_door_stuck"] | false;
  scenario.doorPosition = json["door_position"] | 0;

  // Parse door state string
  const char *doorStateStr = json["door_state"] | "CLOSED";
  if (strcmp(doorStateStr, "OPEN") == 0) {
    scenario.initialDoorState = DoorState::OPEN;
  } else if (strcmp(doorStateStr, "CLOSED") == 0) {
    scenario.initialDoorState = DoorState::CLOSED;
  } else if (strcmp(doorStateStr, "OPENING") == 0) {
    scenario.initialDoorState = DoorState::OPENING;
  } else if (strcmp(doorStateStr, "CLOSING") == 0) {
    scenario.initialDoorState = DoorState::CLOSING;
  } else if (strcmp(doorStateStr, "STOPPED") == 0) {
    scenario.initialDoorState = DoorState::STOPPED;
  } else {
    scenario.initialDoorState = DoorState::UNKNOWN;
  }

  // Water settings
  scenario.autoGeneratePulses = json["auto_generate_pulses"] | true;
  scenario.simulateFrozenLine = json["simulate_frozen_line"] | false;
  scenario.flowRateGPM = json["flow_rate_gpm"] | DEFAULT_FLOW_RATE_GPM;

  // Fault injection
  scenario.injectDoorFault = json["inject_door_fault"] | false;

  // Override settings
  scenario.enableOverride = json["enable_override"] | false;
  scenario.overrideHallOpen = json["override_hall_open"] | false;
  scenario.overrideHallClose = json["override_hall_close"] | false;
  scenario.overrideDoorFault = json["override_door_fault"] | false;
  scenario.overrideManualSwitch = json["override_manual_switch"] | false;

  return true;
}

void CustomScenarioManager::scenarioToJson(const Scenario &scenario,
                                           JsonObject &obj) {
  obj["id"] = static_cast<int>(scenario.id);
  obj["name"] = scenario.name;
  obj["description"] = scenario.description;
  obj["is_custom"] = true;

  // Door settings
  obj["auto_simulate_door"] = scenario.autoSimulateDoor;
  obj["simulate_door_stuck"] = scenario.simulateDoorStuck;
  obj["door_position"] = scenario.doorPosition;

  // Convert door state to string
  const char *doorStateStr = "UNKNOWN";
  switch (scenario.initialDoorState) {
  case DoorState::OPEN:
    doorStateStr = "OPEN";
    break;
  case DoorState::CLOSED:
    doorStateStr = "CLOSED";
    break;
  case DoorState::OPENING:
    doorStateStr = "OPENING";
    break;
  case DoorState::CLOSING:
    doorStateStr = "CLOSING";
    break;
  case DoorState::STOPPED:
    doorStateStr = "STOPPED";
    break;
  default:
    doorStateStr = "UNKNOWN";
    break;
  }
  obj["door_state"] = doorStateStr;

  // Water settings
  obj["auto_generate_pulses"] = scenario.autoGeneratePulses;
  obj["simulate_frozen_line"] = scenario.simulateFrozenLine;
  obj["flow_rate_gpm"] = scenario.flowRateGPM;

  // Fault injection
  obj["inject_door_fault"] = scenario.injectDoorFault;

  // Override settings
  obj["enable_override"] = scenario.enableOverride;
  obj["override_hall_open"] = scenario.overrideHallOpen;
  obj["override_hall_close"] = scenario.overrideHallClose;
  obj["override_door_fault"] = scenario.overrideDoorFault;
  obj["override_manual_switch"] = scenario.overrideManualSwitch;
}
