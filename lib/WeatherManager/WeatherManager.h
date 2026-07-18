#ifndef __WEATHER_MANAGER_H__
#define __WEATHER_MANAGER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IHAL.h"

/**
 * @brief Weather condition category for door-opening decisions
 *
 * Distilled from OpenWeatherMap condition codes + wind/temperature.
 * GOOD means safe to open; INCLEMENT means postpone.
 */
enum class WeatherCondition : uint8_t {
    UNKNOWN = 0,    ///< No data yet / fetch failed
    GOOD = 1,       ///< Clear, clouds, or light wind — safe to open
    INCLEMENT = 2   ///< Rain, snow, thunderstorm, extreme temps, high wind
};

/**
 * @brief Lightweight snapshot of current weather for status display + door logic
 *
 * Keeps fixed-size numeric fields plus a short condition string. Avoids
 * persisting large strings to keep RAM footprint stable (issue #4: uncaught
 * allocations under heap pressure reboot the device).
 */
struct WeatherSnapshot {
    WeatherCondition condition = WeatherCondition::UNKNOWN;
    int weather_code = 0;         ///< Raw OpenWeatherMap condition code (e.g. 800)
    float temp = NAN;             ///< Current temperature (in selected units)
    float feels_like = NAN;       ///< "Feels like" temperature (in selected units)
    int humidity = -1;            ///< Relative humidity %
    float wind_speed = -1.0f;     ///< Wind speed (mph or m/s per units)
    int pressure_hpa = -1;        ///< Sea-level pressure
    int cloudiness = -1;          ///< Cloud cover %
    char main_description[24] = {0};  ///< e.g. "Clear", "Rain"
    char icon_code[8] = {0};          ///< OWM icon code, e.g. "10d"
    long fetch_time = 0;          ///< Unix epoch of last successful fetch
    bool valid = false;           ///< True after first successful fetch
};

/**
 * @brief Short-term forecast entry (next few 3-hour blocks)
 */
struct WeatherForecastEntry {
    long dt = 0;                    ///< Forecast time (epoch)
    float temp = NAN;
    float wind_speed = -1.0f;
    float precip_prob = -1.0f;      ///< 0.0 - 1.0
    char main_description[24] = {0};
};

/**
 * @brief Immutable view of the weather data a decider gets to reason over
 *
 * Passed to IWeatherDecider so decision logic (rule-based today, LLM-based in
 * a future session) never touches WeatherManager internals. All fields are in
 * the configured display units; `units` says which system.
 */
struct WeatherDecisionInput {
    const WeatherSnapshot& current;
    const WeatherForecastEntry* forecast;  ///< Array of `forecast_count` entries (may be empty)
    size_t forecast_count;
    String units;                          ///< "imperial" | "metric" | "standard"
};

/**
 * @brief Result of a door open/no-open decision
 */
struct WeatherDecision {
    bool open = true;         ///< True if it is safe to open the door
    String reason;            ///< Short human-readable justification (shown in status)
};

/**
 * @brief Strategy interface for deciding whether weather permits opening
 *
 * WeatherManager owns data fetching; the *decision* is delegated here so it
 * can be swapped without touching the fetch/parse/cache machinery. The
 * built-in RuleBasedWeatherDecider ships today. A future LlmWeatherDecider
 * (e.g. Ollama) will implement this same interface: it receives the weather
 * data, sends it to the model in a prompt, and returns open/no-open.
 *
 * Contract:
 *  - decide() is called at most once per successful weather fetch (i.e. at the
 *    fetch interval, NOT every loop), so an implementation MAY perform a
 *    bounded network call. It runs on the loop task, so it must respect the
 *    same crash-safety rules (no uncaught exceptions; bounded timeouts).
 *  - On any failure an implementation should fall back to `open = true` so a
 *    provider outage never traps the chickens inside.
 */
class IWeatherDecider {
public:
    virtual ~IWeatherDecider() = default;

    /**
     * @brief Decide whether the door may open given current weather
     * @param input Immutable weather snapshot + short forecast
     * @return Decision with open flag and a short reason
     */
    virtual WeatherDecision decide(const WeatherDecisionInput& input) = 0;

    /**
     * @brief Short identifier for the decider (e.g. "rules", "ollama")
     * Surfaced in status JSON so the UI can show which engine decided.
     */
    virtual const char* name() const = 0;
};

/**
 * @brief Default rule-based decider (precipitation, wind, extreme cold)
 *
 * Classifies OpenWeatherMap condition codes plus wind/temperature thresholds.
 * This is the baseline that ships with the weather feature; the LLM decider is
 * a future drop-in replacement implementing IWeatherDecider.
 */
class RuleBasedWeatherDecider : public IWeatherDecider {
public:
    WeatherDecision decide(const WeatherDecisionInput& input) override;
    const char* name() const override { return "rules"; }

    /**
     * @brief Coarse GOOD/INCLEMENT classification of current conditions
     *
     * Public + static so WeatherManager can use it for the status-display
     * label independent of which decider is installed. Considers OWM condition
     * code, wind, and temperature (all in the given units).
     */
    static WeatherCondition classify(int owmWeatherCode, float wind,
                                     float temp, const String& units);
};

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
    WeatherManager() = default;

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

    // Decision strategy (rule-based default; LLM decider is a future drop-in)
    RuleBasedWeatherDecider default_decider_;
    IWeatherDecider* decider_ = nullptr;  ///< Falls back to default_decider_ when null

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