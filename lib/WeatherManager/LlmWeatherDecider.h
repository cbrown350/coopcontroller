#ifndef __LLM_WEATHER_DECIDER_H__
#define __LLM_WEATHER_DECIDER_H__

#include "IWeatherDecider.h"  // IWeatherDecider, WeatherDecisionInput, RuleBasedWeatherDecider, types
#include "IHAL.h"

/**
 * @brief Provider wire format for LlmWeatherDecider
 *
 * Selects the request URL path + JSON body schema sent to the provider.
 */
enum class LlmProviderWire : uint8_t {
    OPENAI_COMPATIBLE = 0,  ///< POST {base}/v1/chat/completions (OpenAI schema). Ollama Cloud + Rapid-MLX + most providers.
    OLLAMA_NATIVE = 1       ///< POST {base}/api/chat (native Ollama schema). For a raw local Ollama install without its OpenAI-compat layer.
};

/**
 * @brief LLM-backed door-open weather decider (issue #6)
 *
 * Implements IWeatherDecider. On each decide() it builds a prompt from the
 * current conditions + short forecast, scoped to the door's actual open
 * window for today (chickens-out period, not the whole forecast), POSTs it to
 * an OpenAI-compatible or native-Ollama endpoint, and parses the JSON reply.
 *
 * Crash/failure policy: any HTTP failure, timeout, or unparseable reply falls
 * back to the built-in RuleBasedWeatherDecider's decision for this cycle.
 * That keeps the "never trap the chickens inside" guarantee while still
 * being more informative than a bare open=true when the model is reachable
 * but returns junk. Disabled deciders are never called by WeatherManager.
 *
 * This decider is called at most once per successful weather fetch (i.e. at
 * the fetch interval, default 10 min), never per loop, so the network call
 * is bounded. It runs on the loop task and respects the same heap/TLS guards
 * as the rest of WeatherManager.
 */
class LlmWeatherDecider : public IWeatherDecider {
public:
    /**
     * @brief Configure the decider
     * @param hal HAL (for httpPostAuth + heap guard)
     * @param baseUrl Provider base URL, e.g. "http://192.168.1.5:11434" or "https://api.example.com"
     * @param apiKey Bearer token (empty for LAN Ollama with no auth)
     * @param model Model name, e.g. "llama3.1" or "gpt-4o-mini"
     * @param wire Provider wire format (OpenAI-compatible vs native Ollama)
     * @param timeoutMs Per-request timeout (clamped to [5000, 60000])
     */
    LlmWeatherDecider(IHAL* hal, const String& baseUrl, const String& apiKey,
                      const String& model, LlmProviderWire wire,
                      unsigned long timeoutMs = 15000);

    WeatherDecision decide(const WeatherDecisionInput& input) override;
    const char* name() const override { return "llm"; }

    /**
     * @brief Minimal cheap probe used by the "Test Connection" button
     *
     * Sends a trivial prompt and reports whether the provider replied at all.
     * Does not throw or touch door state. Independent of decide()'s prompt
     * path so a slow model can't make the test button hang on the same prompt.
     *
     * @return Empty string on success (provider reachable); error message otherwise
     */
    String testConnection() const;

    /// Minimum free heap to attempt an LLM call (TLS context + JSON doc + prompt)
    static constexpr uint32_t LLM_MIN_FREE_HEAP = 30000;

private:
    IHAL* hal_;
    String base_url_;
    String api_key_;
    String model_;
    LlmProviderWire wire_;
    unsigned long timeout_ms_;

    RuleBasedWeatherDecider fallback_;  ///< Used when the LLM call fails or returns junk

    String buildUrl() const;
    String buildPrompt(const WeatherDecisionInput& input) const;
    String buildRequestBody(const String& systemPrompt, const String& userPrompt) const;
    String sendRequest(const String& systemPrompt, const String& userPrompt,
                       unsigned long timeoutMs) const;

    /**
     * @brief Parse a model reply into a decision
     *
     * Tries strict JSON {open,reason} first, then best-effort keyword parse
     * ("yes"/"no", "open"/"close"/"safe"/"unsafe"). Returns false if neither
     * yields an answer.
     */
    static bool parseReply(const String& reply, bool& outOpen, String& outReason);

    /**
     * @brief Format HH:MM from minutes-since-midnight (or "unknown" if -1)
     */
    static String formatHM(int minutes);
};

#endif // __LLM_WEATHER_DECIDER_H__
