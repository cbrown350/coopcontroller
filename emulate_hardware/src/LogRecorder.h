#ifndef LOG_RECORDER_H
#define LOG_RECORDER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "EmulatorStateManager.h"

/**
 * @brief Maximum number of recordings that can be stored
 */
constexpr uint8_t MAX_RECORDINGS = 10;

/**
 * @brief Recording sampling interval in milliseconds
 */
constexpr uint32_t RECORDING_SAMPLE_INTERVAL_MS = 100;

/**
 * @brief Maximum recording duration in milliseconds (5 minutes)
 */
constexpr uint32_t MAX_RECORDING_DURATION_MS = 300000;

/**
 * @brief Directory for storing recordings on LittleFS
 */
constexpr const char* RECORDINGS_DIR = "/recordings";

/**
 * @brief Metadata file for recording index
 */
constexpr const char* RECORDINGS_INDEX_FILE = "/recordings/_index.json";

/**
 * @brief A single snapshot of all signal states at a point in time
 */
struct SignalSnapshot {
  uint32_t timestamp_ms;           // Relative timestamp from recording start

  // Monitored signals (from main controller)
  bool pump_active;
  bool light_active;
  uint8_t light_brightness;        // 0-100
  bool motor_pos_active;
  bool motor_neg_active;
  bool buzzer_active;
  bool wifi_led_active;

  // Emulated outputs (sent to main controller)
  bool water_flow_enabled;
  float flow_rate_gpm;
  uint8_t door_position;           // 0-100
  bool hall_open_active;
  bool hall_close_active;
  bool manual_switch_pressed;
  bool door_fault_active;
};

/**
 * @brief Metadata for a stored recording
 */
struct RecordingMetadata {
  char id[32];                     // Unique ID (timestamp-based)
  char filename[64];               // Full path on LittleFS
  uint32_t duration_ms;            // Total recording duration
  uint32_t sample_count;           // Number of samples captured
  uint32_t created_at;             // millis() when recording started
  char label[64];                  // User-friendly label
};

/**
 * @brief Recording state machine
 */
enum class RecordingState {
  IDLE,
  RECORDING,
  PAUSED
};

/**
 * @brief Playback state machine
 */
enum class PlaybackState {
  IDLE,
  PLAYING,
  PAUSED
};

/**
 * @brief Manages recording and playback of signal state sequences
 *
 * Captures all monitored and emulated signal states at configurable intervals,
 * saves them to LittleFS, and supports playback with speed control.
 */
class LogRecorder {
public:
  LogRecorder() = default;

  /**
   * @brief Initialize the recorder, create recordings directory if needed
   */
  bool begin();

  /**
   * @brief Update loop - call from main loop for sampling and playback
   * @param stateManager Reference to state manager for reading/writing signals
   */
  void update(EmulatorStateManager& stateManager);

  // ========================================================================
  // RECORDING CONTROL
  // ========================================================================

  /**
   * @brief Start a new recording
   * @param label Optional label for the recording
   * @return true if recording started successfully
   */
  bool startRecording(const char* label = "Recording");

  /**
   * @brief Stop the current recording and save to file
   * @return true if recording saved successfully
   */
  bool stopRecording();

  /**
   * @brief Pause/resume the current recording
   */
  void togglePause();

  /**
   * @brief Get current recording state
   */
  RecordingState getRecordingState() const { return _recordingState; }

  /**
   * @brief Get number of samples captured in current recording
   */
  uint32_t getCurrentSampleCount() const { return _currentSamples.size(); }

  /**
   * @brief Get elapsed time of current recording in ms
   */
  uint32_t getCurrentDuration() const;

  // ========================================================================
  // PLAYBACK CONTROL
  // ========================================================================

  /**
   * @brief Start playing a recording by ID
   * @param id Recording ID to play
   * @param speedPercent Playback speed as percentage (100 = 1x, 200 = 2x, 50 = 0.5x)
   * @return true if playback started successfully
   */
  bool startPlayback(const char* id, uint16_t speedPercent = 100);

  /**
   * @brief Stop playback
   */
  void stopPlayback();

  /**
   * @brief Pause/resume playback
   */
  void togglePlaybackPause();

  /**
   * @brief Get current playback state
   */
  PlaybackState getPlaybackState() const { return _playbackState; }

  /**
   * @brief Get current playback speed percentage
   */
  uint16_t getPlaybackSpeed() const { return _playbackSpeed; }

  /**
   * @brief Set playback speed
   * @param speedPercent Speed as percentage (50 = 0.5x, 100 = 1x, 200 = 2x)
   */
  void setPlaybackSpeed(uint16_t speedPercent);

  /**
   * @brief Get current playback position in ms
   */
  uint32_t getPlaybackPosition() const;

  /**
   * @brief Get total duration of currently playing recording
   */
  uint32_t getPlaybackDuration() const;

  /**
   * @brief Get ID of currently playing recording
   */
  const char* getPlaybackId() const { return _playbackId; }

  // ========================================================================
  // RECORDING MANAGEMENT
  // ========================================================================

  /**
   * @brief Get list of all stored recordings
   */
  const std::vector<RecordingMetadata>& getRecordings() const { return _recordings; }

  /**
   * @brief Delete a recording by ID
   * @return true if deleted successfully
   */
  bool deleteRecording(const char* id);

  /**
   * @brief Delete all recordings
   */
  void deleteAllRecordings();

  /**
   * @brief Download a recording file (returns raw JSON content)
   * @param id Recording ID
   * @param outContent Output string for file content
   * @return true if file read successfully
   */
  bool getRecordingContent(const char* id, String& outContent);

  // ========================================================================
  // JSON SERIALIZATION
  // ========================================================================

  /**
   * @brief Serialize all recording metadata to JSON array
   */
  void recordingsToJson(JsonArray& arr) const;

  /**
   * @brief Serialize current recording/playback status to JSON
   */
  void statusToJson(JsonObject& obj) const;

private:
  // Recording state
  RecordingState _recordingState = RecordingState::IDLE;
  std::vector<SignalSnapshot> _currentSamples;
  uint32_t _recordingStartTime = 0;
  uint32_t _lastSampleTime = 0;
  uint32_t _pausedAccumulatedTime = 0;
  uint32_t _pauseStartTime = 0;
  char _currentLabel[64] = {0};

  // Playback state
  PlaybackState _playbackState = PlaybackState::IDLE;
  std::vector<SignalSnapshot> _playbackSamples;
  char _playbackId[32] = {0};
  uint32_t _playbackDuration = 0;
  uint16_t _playbackSpeed = 100;           // Percentage
  uint32_t _playbackStartRealTime = 0;     // Real millis() when playback started
  uint32_t _playbackPausedPosition = 0;    // Position in recording when paused
  uint32_t _lastPlaybackIndex = 0;         // Last sample index that was applied
  bool _pendingFirstSnapshot = false;      // First snapshot applied on next update()

  // Stored recordings index
  std::vector<RecordingMetadata> _recordings;

  bool _initialized = false;

  // Load index from LittleFS
  bool loadIndex();

  // Save index to LittleFS
  bool saveIndex();

  // Save current recording samples to a file
  bool saveSamples(const char* id);

  // Load samples from a recording file
  bool loadSamples(const char* id, std::vector<SignalSnapshot>& samples);

  // Generate a unique recording ID
  void generateId(char* buffer, size_t size);

  // Sample current signals from state manager
  SignalSnapshot sampleSignals(EmulatorStateManager& stateManager);

  // Apply a snapshot to the state manager during playback
  void applySnapshot(EmulatorStateManager& stateManager, const SignalSnapshot& snapshot);

  // Get effective playback position (accounting for speed)
  uint32_t getEffectivePosition() const;

  // Find the metadata entry for a given ID
  const RecordingMetadata* findRecording(const char* id) const;
};

// Global instance
extern LogRecorder logRecorder;

#endif // LOG_RECORDER_H
