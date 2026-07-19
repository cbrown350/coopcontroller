#ifndef __I_WEATHER_DECIDER_H__
#define __I_WEATHER_DECIDER_H__

#include <Arduino.h>
#include <ArduinoJson.h>

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
 *
 * `window_open_minutes` / `window_close_minutes` are the door's actual open
 * window for today, in local minutes-since-midnight (-1 = unknown). An LLM
 * decider uses these to judge weather for the period the chickens will be
 * outside, instead of flagging on any bad weather anywhere in the forecast.
 * The rule-based decider ignores them.
 */
struct WeatherDecisionInput {
    const WeatherSnapshot& current;
    const WeatherForecastEntry* forecast;  ///< Array of `forecast_count` entries (may be empty)
    size_t forecast_count;
    String units;                          ///< "imperial" | "metric" | "standard"
    int window_open_minutes = -1;          ///< Door opens at this local minute today (-1 unknown)
    int window_close_minutes = -1;         ///< Door's hard close ceiling, local minute (-1 unknown)
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
 * built-in RuleBasedWeatherDecider ships today. An LlmWeatherDecider
 * implements this same interface: it receives the weather data, sends it to
 * the model in a prompt, and returns open/no-open.
 *
 * Contract:
 *  - decide() is called at most once per successful weather fetch (i.e. at the
 *    fetch interval, NOT every loop), so an implementation MAY perform a
 *    bounded network call. It runs on the loop task, so it must respect the
 *    same crash-safety rules (no uncaught exceptions; bounded timeouts).
 *  - On any failure an implementation should fall back to `open = true` (or
 *    to the rule-based decision) so a provider outage never traps the
 *    chickens inside.
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
     * @brief Short identifier for the decider (e.g. "rules", "llm")
     * Surfaced in status JSON so the UI can show which engine decided.
     */
    virtual const char* name() const = 0;
};

/**
 * @brief Default rule-based decider (precipitation, wind, extreme cold)
 *
 * Classifies OpenWeatherMap condition codes plus wind/temperature thresholds.
 * This is the baseline that ships with the weather feature; the LLM decider is
 * a drop-in replacement implementing IWeatherDecider.
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

#endif // __I_WEATHER_DECIDER_H__
