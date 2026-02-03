#include "LogRecorder.h"

// Global instance
LogRecorder logRecorder;

bool LogRecorder::begin() {
  if (_initialized) return true;

  if (!LittleFS.begin(false)) {
    Serial.println("[LogRecorder] LittleFS mount failed");
    return false;
  }

  // Create recordings directory if it doesn't exist
  if (!LittleFS.exists(RECORDINGS_DIR)) {
    LittleFS.mkdir(RECORDINGS_DIR);
    Serial.println("[LogRecorder] Created recordings directory");
  }

  _initialized = true;
  loadIndex();

  Serial.printf("[LogRecorder] Initialized with %u recordings\n",
                static_cast<unsigned>(_recordings.size()));
  return true;
}

void LogRecorder::update(EmulatorStateManager& stateManager) {
  // Handle recording sampling
  if (_recordingState == RecordingState::RECORDING) {
    uint32_t now = millis();

    // Check max duration
    if (getCurrentDuration() >= MAX_RECORDING_DURATION_MS) {
      Serial.println("[LogRecorder] Max duration reached, stopping recording");
      stopRecording();
      return;
    }

    // Sample at configured interval
    if (now - _lastSampleTime >= RECORDING_SAMPLE_INTERVAL_MS) {
      SignalSnapshot snapshot = sampleSignals(stateManager);
      // Calculate relative timestamp accounting for pauses
      snapshot.timestamp_ms = getCurrentDuration();
      _currentSamples.push_back(snapshot);
      _lastSampleTime = now;
    }
  }

  // Handle playback
  if (_playbackState == PlaybackState::PLAYING) {
    // Apply first snapshot on first update after playback start
    if (_pendingFirstSnapshot && !_playbackSamples.empty()) {
      applySnapshot(stateManager, _playbackSamples[0]);
      _pendingFirstSnapshot = false;
    }

    uint32_t effectivePos = getEffectivePosition();

    // Check if playback is complete
    if (effectivePos >= _playbackDuration) {
      Serial.println("[LogRecorder] Playback complete");
      stopPlayback();
      return;
    }

    // Find and apply the next snapshot
    while (_lastPlaybackIndex < _playbackSamples.size() - 1 &&
           _playbackSamples[_lastPlaybackIndex + 1].timestamp_ms <= effectivePos) {
      _lastPlaybackIndex++;
      applySnapshot(stateManager, _playbackSamples[_lastPlaybackIndex]);
    }
  }
}

// ============================================================================
// RECORDING CONTROL
// ============================================================================

bool LogRecorder::startRecording(const char* label) {
  if (_recordingState != RecordingState::IDLE) {
    Serial.println("[LogRecorder] Already recording");
    return false;
  }

  if (_playbackState != PlaybackState::IDLE) {
    Serial.println("[LogRecorder] Cannot record during playback");
    return false;
  }

  _currentSamples.clear();
  _recordingStartTime = millis();
  _lastSampleTime = millis();
  _pausedAccumulatedTime = 0;
  _pauseStartTime = 0;
  _recordingState = RecordingState::RECORDING;
  strncpy(_currentLabel, label, sizeof(_currentLabel) - 1);
  _currentLabel[sizeof(_currentLabel) - 1] = '\0';

  Serial.printf("[LogRecorder] Recording started: %s\n", label);
  return true;
}

bool LogRecorder::stopRecording() {
  if (_recordingState == RecordingState::IDLE) {
    Serial.println("[LogRecorder] Not recording");
    return false;
  }

  // If paused, account for pause time
  if (_recordingState == RecordingState::PAUSED) {
    _pausedAccumulatedTime += millis() - _pauseStartTime;
  }

  _recordingState = RecordingState::IDLE;

  if (_currentSamples.empty()) {
    Serial.println("[LogRecorder] No samples captured, discarding");
    return false;
  }

  // Check max recordings limit
  if (_recordings.size() >= MAX_RECORDINGS) {
    // Remove oldest recording to make space
    if (!_recordings.empty()) {
      const char* oldestId = _recordings[0].id;
      // Delete the file
      char path[64];
      snprintf(path, sizeof(path), "%s/%s.json", RECORDINGS_DIR, oldestId);
      LittleFS.remove(path);
      _recordings.erase(_recordings.begin());
      Serial.printf("[LogRecorder] Removed oldest recording to make space\n");
    }
  }

  // Generate ID and save
  char id[32];
  generateId(id, sizeof(id));

  if (!saveSamples(id)) {
    Serial.println("[LogRecorder] Failed to save recording samples");
    return false;
  }

  // Add to index
  RecordingMetadata meta;
  strncpy(meta.id, id, sizeof(meta.id) - 1);
  meta.id[sizeof(meta.id) - 1] = '\0';
  snprintf(meta.filename, sizeof(meta.filename), "%s/%s.json", RECORDINGS_DIR, id);
  meta.duration_ms = _currentSamples.empty() ? 0 : _currentSamples.back().timestamp_ms;
  meta.sample_count = _currentSamples.size();
  meta.created_at = _recordingStartTime;
  strncpy(meta.label, _currentLabel, sizeof(meta.label) - 1);
  meta.label[sizeof(meta.label) - 1] = '\0';

  _recordings.push_back(meta);
  saveIndex();

  Serial.printf("[LogRecorder] Recording saved: %s (%u samples, %ums)\n",
                id, static_cast<unsigned>(meta.sample_count), meta.duration_ms);

  _currentSamples.clear();
  return true;
}

void LogRecorder::togglePause() {
  if (_recordingState == RecordingState::RECORDING) {
    _recordingState = RecordingState::PAUSED;
    _pauseStartTime = millis();
    Serial.println("[LogRecorder] Recording paused");
  } else if (_recordingState == RecordingState::PAUSED) {
    _pausedAccumulatedTime += millis() - _pauseStartTime;
    _pauseStartTime = 0;
    _lastSampleTime = millis();
    _recordingState = RecordingState::RECORDING;
    Serial.println("[LogRecorder] Recording resumed");
  }
}

uint32_t LogRecorder::getCurrentDuration() const {
  if (_recordingState == RecordingState::IDLE) return 0;
  uint32_t elapsed = millis() - _recordingStartTime;
  uint32_t paused = _pausedAccumulatedTime;
  if (_recordingState == RecordingState::PAUSED) {
    paused += millis() - _pauseStartTime;
  }
  return elapsed - paused;
}

// ============================================================================
// PLAYBACK CONTROL
// ============================================================================

bool LogRecorder::startPlayback(const char* id, uint16_t speedPercent) {
  if (_recordingState != RecordingState::IDLE) {
    Serial.println("[LogRecorder] Cannot play during recording");
    return false;
  }

  const RecordingMetadata* meta = findRecording(id);
  if (!meta) {
    Serial.printf("[LogRecorder] Recording not found: %s\n", id);
    return false;
  }

  // Load samples from file
  _playbackSamples.clear();
  if (!loadSamples(id, _playbackSamples)) {
    Serial.println("[LogRecorder] Failed to load recording samples");
    return false;
  }

  if (_playbackSamples.empty()) {
    Serial.println("[LogRecorder] Recording has no samples");
    return false;
  }

  strncpy(_playbackId, id, sizeof(_playbackId) - 1);
  _playbackId[sizeof(_playbackId) - 1] = '\0';
  _playbackDuration = meta->duration_ms;
  _playbackSpeed = speedPercent > 0 ? speedPercent : 100;
  _playbackStartRealTime = millis();
  _playbackPausedPosition = 0;
  _lastPlaybackIndex = 0;
  _playbackState = PlaybackState::PLAYING;
  _pendingFirstSnapshot = true;  // First snapshot applied on next update() call

  Serial.printf("[LogRecorder] Playback started: %s at %u%% speed\n",
                id, _playbackSpeed);
  return true;
}

void LogRecorder::stopPlayback() {
  _playbackState = PlaybackState::IDLE;
  _playbackSamples.clear();
  _playbackId[0] = '\0';
  Serial.println("[LogRecorder] Playback stopped");
}

void LogRecorder::togglePlaybackPause() {
  if (_playbackState == PlaybackState::PLAYING) {
    _playbackPausedPosition = getEffectivePosition();
    _playbackState = PlaybackState::PAUSED;
    Serial.println("[LogRecorder] Playback paused");
  } else if (_playbackState == PlaybackState::PAUSED) {
    // Recalculate start time so effective position continues from paused point
    // effectivePos = (now - startRealTime) * speed / 100
    // We want effectivePos = _playbackPausedPosition at current time
    // So: startRealTime = now - (_playbackPausedPosition * 100 / speed)
    uint32_t now = millis();
    _playbackStartRealTime = now - (static_cast<uint64_t>(_playbackPausedPosition) * 100 / _playbackSpeed);
    _playbackState = PlaybackState::PLAYING;
    Serial.println("[LogRecorder] Playback resumed");
  }
}

void LogRecorder::setPlaybackSpeed(uint16_t speedPercent) {
  if (speedPercent == 0) speedPercent = 100;

  if (_playbackState == PlaybackState::PLAYING) {
    // Pause, set speed, resume to recalculate timing
    _playbackPausedPosition = getEffectivePosition();
    _playbackSpeed = speedPercent;
    uint32_t now = millis();
    _playbackStartRealTime = now - (static_cast<uint64_t>(_playbackPausedPosition) * 100 / _playbackSpeed);
  } else {
    _playbackSpeed = speedPercent;
  }
}

uint32_t LogRecorder::getPlaybackPosition() const {
  if (_playbackState == PlaybackState::IDLE) return 0;
  if (_playbackState == PlaybackState::PAUSED) return _playbackPausedPosition;
  return getEffectivePosition();
}

uint32_t LogRecorder::getPlaybackDuration() const {
  return _playbackDuration;
}

// ============================================================================
// RECORDING MANAGEMENT
// ============================================================================

bool LogRecorder::deleteRecording(const char* id) {
  for (size_t i = 0; i < _recordings.size(); i++) {
    if (strcmp(_recordings[i].id, id) == 0) {
      // Delete the file
      LittleFS.remove(_recordings[i].filename);
      _recordings.erase(_recordings.begin() + i);
      saveIndex();
      Serial.printf("[LogRecorder] Deleted recording: %s\n", id);
      return true;
    }
  }
  return false;
}

void LogRecorder::deleteAllRecordings() {
  for (const auto& meta : _recordings) {
    LittleFS.remove(meta.filename);
  }
  _recordings.clear();
  LittleFS.remove(RECORDINGS_INDEX_FILE);
  Serial.println("[LogRecorder] All recordings deleted");
}

bool LogRecorder::getRecordingContent(const char* id, String& outContent) {
  const RecordingMetadata* meta = findRecording(id);
  if (!meta) return false;

  File file = LittleFS.open(meta->filename, "r");
  if (!file) return false;

  outContent = file.readString();
  file.close();
  return !outContent.isEmpty();
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

void LogRecorder::recordingsToJson(JsonArray& arr) const {
  for (const auto& meta : _recordings) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = meta.id;
    obj["label"] = meta.label;
    obj["duration_ms"] = meta.duration_ms;
    obj["sample_count"] = meta.sample_count;
    obj["created_at"] = meta.created_at;

    // Format duration as human-readable
    uint32_t totalSec = meta.duration_ms / 1000;
    char durationStr[32];
    snprintf(durationStr, sizeof(durationStr), "%02u:%02u.%02u",
             totalSec / 60, totalSec % 60, (meta.duration_ms % 1000) / 10);
    obj["duration_formatted"] = durationStr;
  }
}

void LogRecorder::statusToJson(JsonObject& obj) const {
  // Recording status
  JsonObject rec = obj["recording"].to<JsonObject>();
  switch (_recordingState) {
    case RecordingState::IDLE:     rec["state"] = "idle"; break;
    case RecordingState::RECORDING: rec["state"] = "recording"; break;
    case RecordingState::PAUSED:   rec["state"] = "paused"; break;
  }
  rec["sample_count"] = static_cast<uint32_t>(_currentSamples.size());
  rec["duration_ms"] = getCurrentDuration();
  rec["label"] = _currentLabel;

  // Playback status
  JsonObject play = obj["playback"].to<JsonObject>();
  switch (_playbackState) {
    case PlaybackState::IDLE:    play["state"] = "idle"; break;
    case PlaybackState::PLAYING: play["state"] = "playing"; break;
    case PlaybackState::PAUSED:  play["state"] = "paused"; break;
  }
  play["id"] = _playbackId;
  play["position_ms"] = getPlaybackPosition();
  play["duration_ms"] = _playbackDuration;
  play["speed_percent"] = _playbackSpeed;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

bool LogRecorder::loadIndex() {
  if (!LittleFS.exists(RECORDINGS_INDEX_FILE)) {
    _recordings.clear();
    return true;
  }

  File file = LittleFS.open(RECORDINGS_INDEX_FILE, "r");
  if (!file) return false;

  JsonDocument doc;
  if (deserializeJson(doc, file)) {
    file.close();
    return false;
  }
  file.close();

  _recordings.clear();
  JsonArray arr = doc["recordings"].as<JsonArray>();
  for (JsonObject obj : arr) {
    RecordingMetadata meta;
    strncpy(meta.id, obj["id"] | "", sizeof(meta.id) - 1);
    strncpy(meta.filename, obj["filename"] | "", sizeof(meta.filename) - 1);
    strncpy(meta.label, obj["label"] | "", sizeof(meta.label) - 1);
    meta.duration_ms = obj["duration_ms"] | 0;
    meta.sample_count = obj["sample_count"] | 0;
    meta.created_at = obj["created_at"] | 0;
    _recordings.push_back(meta);
  }

  return true;
}

bool LogRecorder::saveIndex() {
  JsonDocument doc;
  JsonArray arr = doc["recordings"].to<JsonArray>();

  for (const auto& meta : _recordings) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = meta.id;
    obj["filename"] = meta.filename;
    obj["label"] = meta.label;
    obj["duration_ms"] = meta.duration_ms;
    obj["sample_count"] = meta.sample_count;
    obj["created_at"] = meta.created_at;
  }

  File file = LittleFS.open(RECORDINGS_INDEX_FILE, "w");
  if (!file) return false;

  size_t written = serializeJson(doc, file);
  file.close();
  return written > 0;
}

bool LogRecorder::saveSamples(const char* id) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s.json", RECORDINGS_DIR, id);

  File file = LittleFS.open(path, "w");
  if (!file) return false;

  // Write as streaming JSON array to minimize memory usage
  file.print("{\"samples\":[");
  for (size_t i = 0; i < _currentSamples.size(); i++) {
    if (i > 0) file.print(",");
    const auto& s = _currentSamples[i];
    // Compact JSON format
    file.printf("{\"t\":%u,\"pa\":%d,\"la\":%d,\"lb\":%d,"
                "\"mp\":%d,\"mn\":%d,\"ba\":%d,\"wa\":%d,"
                "\"wf\":%d,\"fr\":%.2f,\"dp\":%u,"
                "\"ho\":%d,\"hc\":%d,\"ms\":%d,\"df\":%d}",
                s.timestamp_ms,
                s.pump_active ? 1 : 0,
                s.light_active ? 1 : 0,
                s.light_brightness,
                s.motor_pos_active ? 1 : 0,
                s.motor_neg_active ? 1 : 0,
                s.buzzer_active ? 1 : 0,
                s.wifi_led_active ? 1 : 0,
                s.water_flow_enabled ? 1 : 0,
                s.flow_rate_gpm,
                s.door_position,
                s.hall_open_active ? 1 : 0,
                s.hall_close_active ? 1 : 0,
                s.manual_switch_pressed ? 1 : 0,
                s.door_fault_active ? 1 : 0);
  }
  file.print("]}");
  file.close();
  return true;
}

bool LogRecorder::loadSamples(const char* id, std::vector<SignalSnapshot>& samples) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s.json", RECORDINGS_DIR, id);

  File file = LittleFS.open(path, "r");
  if (!file) return false;

  // Read file content - for ESP32 we need to parse the full content
  String content = file.readString();
  file.close();

  JsonDocument doc;
  if (deserializeJson(doc, content)) {
    return false;
  }

  samples.clear();
  JsonArray arr = doc["samples"].as<JsonArray>();
  for (JsonObject obj : arr) {
    SignalSnapshot s;
    s.timestamp_ms = obj["t"] | 0;
    s.pump_active = (obj["pa"] | 0) != 0;
    s.light_active = (obj["la"] | 0) != 0;
    s.light_brightness = obj["lb"] | 0;
    s.motor_pos_active = (obj["mp"] | 0) != 0;
    s.motor_neg_active = (obj["mn"] | 0) != 0;
    s.buzzer_active = (obj["ba"] | 0) != 0;
    s.wifi_led_active = (obj["wa"] | 0) != 0;
    s.water_flow_enabled = (obj["wf"] | 0) != 0;
    s.flow_rate_gpm = obj["fr"] | 0.0f;
    s.door_position = obj["dp"] | 0;
    s.hall_open_active = (obj["ho"] | 0) != 0;
    s.hall_close_active = (obj["hc"] | 0) != 0;
    s.manual_switch_pressed = (obj["ms"] | 0) != 0;
    s.door_fault_active = (obj["df"] | 0) != 0;
    samples.push_back(s);
  }

  return true;
}

void LogRecorder::generateId(char* buffer, size_t size) {
  // Use millis() as base for unique ID
  snprintf(buffer, size, "rec_%lu_%u", (unsigned long)millis(), static_cast<unsigned>(_recordings.size()));
}

SignalSnapshot LogRecorder::sampleSignals(EmulatorStateManager& stateManager) {
  SignalSnapshot s;
  s.timestamp_ms = 0; // Set by caller

  const auto& mon = stateManager.getMonitoredSignals();
  s.pump_active = mon.pumpActive;
  s.light_active = mon.lightActive;
  s.light_brightness = mon.lightBrightness;
  s.motor_pos_active = mon.doorMotorPosActive;
  s.motor_neg_active = mon.doorMotorNegActive;
  s.buzzer_active = mon.buzzerActive;
  s.wifi_led_active = mon.wifiLedActive;

  const auto& emu = stateManager.getEmulatedOutputs();
  s.water_flow_enabled = emu.waterFlowEnabled;
  s.flow_rate_gpm = emu.flowRateGPM;
  s.door_position = emu.doorPosition;
  s.hall_open_active = emu.hallOpenActive;
  s.hall_close_active = emu.hallCloseActive;
  s.manual_switch_pressed = emu.manualSwitch.isPressed;
  s.door_fault_active = emu.doorFaultActive;

  return s;
}

void LogRecorder::applySnapshot(EmulatorStateManager& stateManager,
                                const SignalSnapshot& snapshot) {
  // Apply emulated outputs (we can only control what the emulator drives)
  stateManager.setDoorPosition(snapshot.door_position);
  stateManager.setWaterFlowEnabled(snapshot.water_flow_enabled);
  stateManager.setFlowRate(snapshot.flow_rate_gpm);
  stateManager.setDoorFault(snapshot.door_fault_active);

  // Hall sensors and manual switch are controlled via override mode
  // We enable override mode during playback for direct control
  stateManager.setManualOverrideEnabled(true);
  stateManager.setOverrideHallOpen(snapshot.hall_open_active);
  stateManager.setOverrideHallClose(snapshot.hall_close_active);
  stateManager.setOverrideDoorFault(snapshot.door_fault_active);
  stateManager.setOverrideManualSwitch(snapshot.manual_switch_pressed);
}

uint32_t LogRecorder::getEffectivePosition() const {
  uint32_t elapsed = millis() - _playbackStartRealTime;
  return static_cast<uint64_t>(elapsed) * _playbackSpeed / 100;
}

const RecordingMetadata* LogRecorder::findRecording(const char* id) const {
  for (const auto& meta : _recordings) {
    if (strcmp(meta.id, id) == 0) {
      return &meta;
    }
  }
  return nullptr;
}
