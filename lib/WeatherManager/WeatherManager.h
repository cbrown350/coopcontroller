#ifndef __WEATHER_MANAGER_H__
#define __WEATHER_MANAGER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <memory>
#include "IHAL.h"
#include "IWeatherDecider.h"

class LlmWeatherDecider;  // Forward decl; full type pulled in via .cpp (avoids circular include)

/**
 * @brief OpenWeatherMap client and weather-gate for door automation
 *
 * Fetches current weather + a few 3-hour forecast blocks from the
 * OpenWeatherMap API at a moderate, configurable interval (default 10
 * minutes — ~144 calls/day, far below the free-tier 1,000/day limit).
 *
 * Provides:
 *  - isWeatherGoodForOpening(): the door's weather gate
 *  - getStatusJson(): current + short forecast for the status page
 *
 * Crash-safety (issue #4): all network/JSON work is skipped when free heap
 * is below NETWORK_LOW_HEAP_FLOOR and wrapped so no uncaught exception
 * escapes to the loop task. The manager never blocks the loop: the HTTPS
 * request reuses the HAL's bounded-timeout httpGet.
 */
class WeatherManager {
public:
    WeatherManager();
    ~WeatherManager();  // Defined in .cpp so unique_ptr<LlmWeatherDecider> sees the full type

    /**
     * @brief Initialize the weather manager
     * @param hal Pointer to hardware abstraction layer (for HTTP + heap + WiFi)
     */
    void begin(IHAL* hal);

    /**
     * @brief Configuration setters — call from main.cpp after settings load
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool getEnabled() const { return enabled_; }
    void setApiKey(const String& key) { api_key_ = key; }
    String getApiKey() const { return api_key_; }
    void setUnits(const String& units) { units_ = units; }
    String getUnits() const { return units_; }
    void setUpdateIntervalMinutes(unsigned int minutes);
    unsigned int getUpdateIntervalMinutes() const { return update_interval_minutes_; }
    void setLocation(float latitude, float longitude);

    /**
     * @brief Tell the weather manager the door's open-window for today
     *
     * Pushed by DoorController whenever it recomputes its schedule (cheap int
     * assignment). Surfaces to deciders via WeatherDecisionInput so an LLM
     * decider can judge weather for the actual period the chickens are out,
     * not the whole forecast. -1 in either field means "unknown" (decider
     * falls back to whole-forecast reasoning).
     */
    void setOpenWindowMinutes(int openMin, int closeMin);

    /**
     * @brief Configure the optional built-in LLM decider and make it active
     *
     * Constructs (or reconfigures) the owned LlmWeatherDecider with the given
     * provider settings and installs it via setDecider() when enabled. When
     * disabled, restores the default rule-based decider. Safe to call at any
     * time from the loop task (e.g. after settings change).
     *
     * @param enabled Activate the LLM decider (false => rule-based fallback)
     * @param baseUrl Provider base URL
     * @param apiKey Bearer token (empty for LAN Ollama with no auth)
     * @param model Model name
     * @param providerType "openai_compatible" | "ollama_native" | "ollama_cloud"
     * @param timeoutSeconds Per-request timeout (clamped internally)
     */
    void configureLlmDecider(bool enabled, const String& baseUrl, const String& apiKey,
                             const String& model, const String& providerType,
                             unsigned int timeoutSeconds);

    /**
     * @brief Probe the LLM provider (used by the "Test Connection" button)
     *
     * Uses the LLM decider if one is configured; otherwise builds a temporary
     * probe from the given override values (so the UI can test unsaved config).
     * @return Empty string on success, error message on failure
     */
    String testLlmConnection(const String& baseUrl, const String& apiKey,
                             const String& model, const String& providerType,
                             unsigned int timeoutSeconds) const;

    /**
     * @brief Install the open/no-open decision strategy
     *
     * Defaults to the built-in RuleBasedWeatherDecider. Pass a different
     * IWeatherDecider (e.g. a future Ollama-backed LLM decider) to change how
     * the open decision is made without touching the fetch/parse pipeline.
     * Passing nullptr restores the built-in rule-based decider. The pointer is
     * borrowed, not owned — the caller keeps it alive for the manager's life.
     *
     * @param decider Decider to use, or nullptr for the built-in default
     */
    void setDecider(IWeatherDecider* decider);

    /**
     * @brief Name of the active decision engine (e.g. "rules", "ollama")
     */
    const char* getDeciderName() const;

    /**
     * @brief Main loop tick — call every loop iteration
     *
     * Triggers a fetch at the configured interval when enabled, configured,
     * WiFi-connected, and heap is healthy. Never blocks; on failure it just
     * waits for the next interval.
     */
    void update();

    /**
     * @brief Force an immediate refresh (e.g. after settings change)
     *
     * Respects the same WiFi/heap guards as update(). Does not block the
     * caller beyond the HTTPS timeout — callers should only invoke this
     * from the loop task, not an async web handler.
     */
    void forceRefresh();

    /**
     * @brief The door's weather gate
     *
     * @return true if weather is safe for opening the door (or weather
     *         checking is disabled/unavailable, in which case the door
     *         falls back to its normal schedule-only behavior).
     */
    bool isWeatherGoodForOpening() const;

    /**
     * @brief Check if the weather gate is actively in effect
     *
     * True only when enabled + configured + a valid snapshot exists.
     * When false, the door should ignore weather entirely (legacy behavior).
     */
    bool isWeatherGateActive() const;

    /**
     * @brief Serialize current weather + forecast to a JSON object
     *
     * Populates the given JsonObject for the /weather/status endpoint.
     * Includes a small forecast array (next few 3-hour blocks).
     */
    void toJson(JsonObject& json) const;

    // Statistics
    unsigned long getLastFetchTime() const { return last_fetch_attempt_ms_; }
    unsigned int getSuccessfulFetches() const { return successful_fetches_; }
    unsigned int getFailedFetches() const { return failed_fetches_; }
    String getLastError() const { return last_error_; }

    /// Reason string from the most recent decision (e.g. "clear", "rain")
    String getDecisionReason() const { return decision_reason_; }

    /// Minimum free heap to attempt a weather fetch (TLS context + JSON doc)
    static constexpr uint32_t WEATHER_MIN_FREE_HEAP = 30000;

private:
    IHAL* hal_ = nullptr;
    bool enabled_ = false;
    String api_key_;
    String units_ = "imperial";  ///< "imperial" (°F/mph), "metric" (°C/m/s), "standard" (K/m/s)
    unsigned int update_interval_minutes_ = 10;
    float latitude_ = 40.7128f;
    float longitude_ = -74.0060f;
    int window_open_minutes_ = -1;    ///< Door's today open time (local min since midnight)
    int window_close_minutes_ = -1;   ///< Door's today close ceiling (local min since midnight)

    // Decision strategy (rule-based default; LLM decider is a future drop-in)
    RuleBasedWeatherDecider default_decider_;
    IWeatherDecider* decider_ = nullptr;  ///< Falls back to default_decider_ when null
    std::unique_ptr<LlmWeatherDecider> llm_decider_;  ///< Owned LLM decider (nullptr when disabled)

    // Cached state
    WeatherSnapshot current_;
    static constexpr size_t MAX_FORECAST_ENTRIES = 3;
    WeatherForecastEntry forecast_[MAX_FORECAST_ENTRIES];
    size_t forecast_count_ = 0;

    // Cached decision — recomputed once per successful fetch so an LLM-backed
    // decider makes at most one model call per fetch interval, never per loop.
    bool decision_open_ = true;
    String decision_reason_;

    // Timing & stats
    unsigned long last_fetch_attempt_ms_ = 0;
    unsigned int successful_fetches_ = 0;
    unsigned int failed_fetches_ = 0;
    String last_error_;

    // Internal helpers
    bool isReadyToFetch() const;
    void fetchWeather();
    bool parseCurrentJson(const String& body);
    bool parseForecastJson(const String& body);
    void recomputeDecision();
    IWeatherDecider& activeDecider();
    static void copyTruncated(char* dst, size_t dstSize, const char* src);
};

#endif // __WEATHER_MANAGER_H__