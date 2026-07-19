#include "LlmWeatherDecider.h"
#include "Logger.h"
#include <math.h>

LlmWeatherDecider::LlmWeatherDecider(IHAL* hal, const String& baseUrl, const String& apiKey,
                                     const String& model, LlmProviderWire wire,
                                     unsigned long timeoutMs)
    : hal_(hal), base_url_(baseUrl), api_key_(apiKey), model_(model), wire_(wire) {
    if (timeoutMs < 5000) timeoutMs = 5000;
    if (timeoutMs > 60000) timeoutMs = 60000;
    timeout_ms_ = timeoutMs;
    // Trim a trailing slash so buildUrl() never produces "//v1/..." style paths.
    while (base_url_.endsWith("/")) base_url_.remove(base_url_.length() - 1);
}

String LlmWeatherDecider::formatHM(int minutes) {
    if (minutes < 0) return "unknown";
    int m = ((minutes % 1440) + 1440) % 1440;  // normalize to [0,1440)
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
    return String(buf);
}

String LlmWeatherDecider::buildUrl() const {
    return base_url_ + (wire_ == LlmProviderWire::OLLAMA_NATIVE ? "/api/chat" : "/v1/chat/completions");
}

String LlmWeatherDecider::buildPrompt(const WeatherDecisionInput& input) const {
    String p;
    p += "Door open window today: opens ";
    p += formatHM(input.window_open_minutes);
    p += ", closes ";
    p += formatHM(input.window_close_minutes);
    p += " (local time). Judge conditions for THIS window, not the full forecast.\n";

    p += "Units: " + input.units + "\n";
    p += "Current conditions: ";
    p += input.current.main_description[0] != '\0' ? String(input.current.main_description) : String("unknown");
    if (!isnan(input.current.temp)) { p += ", temp="; p += String(input.current.temp, 1); }
    if (!isnan(input.current.feels_like)) { p += ", feels_like="; p += String(input.current.feels_like, 1); }
    if (input.current.wind_speed >= 0) { p += ", wind="; p += String(input.current.wind_speed, 1); }
    if (input.current.humidity >= 0) { p += ", humidity="; p += String(input.current.humidity); p += "%"; }
    p += "\n";

    if (input.forecast_count > 0) {
        p += "Upcoming forecast (approx. 3-hour blocks from now):\n";
        for (size_t i = 0; i < input.forecast_count; i++) {
            const WeatherForecastEntry& fe = input.forecast[i];
            p += "  block ";
            p += String((unsigned)(i + 1));
            p += ": ";
            p += fe.main_description[0] != '\0' ? String(fe.main_description) : String("unknown");
            if (!isnan(fe.temp)) { p += ", temp="; p += String(fe.temp, 1); }
            if (fe.wind_speed >= 0) { p += ", wind="; p += String(fe.wind_speed, 1); }
            if (fe.precip_prob >= 0) { p += ", precip_prob="; p += String((int)(fe.precip_prob * 100)); p += "%"; }
            p += "\n";
        }
    } else {
        p += "No forecast data available; use current conditions only.\n";
    }

    p += "Is it safe to let the chickens outside for this window? "
         "Consider precipitation, wind, and temperature extremes during the window; "
         "brief conditions well before or after the window matter less.";
    return p;
}

String LlmWeatherDecider::buildRequestBody(const String& systemPrompt, const String& userPrompt) const {
    JsonDocument doc;
    doc["model"] = model_;
    doc["stream"] = false;
    JsonArray messages = doc["messages"].to<JsonArray>();
    JsonObject sys = messages.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = systemPrompt;
    JsonObject usr = messages.add<JsonObject>();
    usr["role"] = "user";
    usr["content"] = userPrompt;

    String body;
    serializeJson(doc, body);
    return body;
}

String LlmWeatherDecider::sendRequest(const String& systemPrompt, const String& userPrompt,
                                     unsigned long timeoutMs) const {
    if (hal_ == nullptr) return "";
    if (hal_->getFreeHeap() < LLM_MIN_FREE_HEAP) {
        logger.logWarning("LlmWeatherDecider: skipping call, free heap too low");
        return "";
    }

    String body = buildRequestBody(systemPrompt, userPrompt);
    String responseBody = hal_->httpPostAuth(buildUrl(), body, api_key_, "", timeoutMs);
    if (responseBody.length() == 0) return "";

    JsonDocument doc;
    if (deserializeJson(doc, responseBody) != DeserializationError::Ok) {
        logger.logWarning("LlmWeatherDecider: provider response was not valid JSON");
        return "";
    }

    // OpenAI-compatible: choices[0].message.content
    // Ollama native:      message.content
    if (wire_ == LlmProviderWire::OLLAMA_NATIVE) {
        const char* content = doc["message"]["content"] | (const char*)nullptr;
        return content != nullptr ? String(content) : String("");
    }
    JsonArray choices = doc["choices"].as<JsonArray>();
    if (choices.isNull() || choices.size() == 0) return "";
    const char* content = choices[0]["message"]["content"] | (const char*)nullptr;
    return content != nullptr ? String(content) : String("");
}

bool LlmWeatherDecider::parseReply(const String& reply, bool& outOpen, String& outReason) {
    if (reply.length() == 0) return false;

    // Models sometimes wrap JSON in prose or markdown fences. Extract the
    // outermost {...} slice before parsing so extra text doesn't break it.
    int start = reply.indexOf('{');
    int end = reply.lastIndexOf('}');
    if (start >= 0 && end > start) {
        String jsonSlice = reply.substring(start, end + 1);
        JsonDocument doc;
        if (deserializeJson(doc, jsonSlice) == DeserializationError::Ok) {
            if (doc["open"].is<bool>()) {
                outOpen = doc["open"].as<bool>();
                outReason = doc["reason"].is<const char*>() ? doc["reason"].as<String>() : String("llm decision");
                return true;
            }
            if (doc["open"].is<const char*>()) {
                String v = doc["open"].as<String>();
                v.toLowerCase();
                outOpen = (v == "true" || v == "yes");
                outReason = doc["reason"].is<const char*>() ? doc["reason"].as<String>() : String("llm decision");
                return true;
            }
        }
    }

    // Best-effort keyword fallback for models that ignore the JSON instruction.
    String lower = reply;
    lower.toLowerCase();
    if (lower.indexOf("\"open\": true") >= 0 || lower.indexOf("\"open\":true") >= 0) {
        outOpen = true; outReason = "llm decision (keyword match)"; return true;
    }
    if (lower.indexOf("\"open\": false") >= 0 || lower.indexOf("\"open\":false") >= 0) {
        outOpen = false; outReason = "llm decision (keyword match)"; return true;
    }

    return false;  // Couldn't extract a decision — caller falls back to rule-based
}

WeatherDecision LlmWeatherDecider::decide(const WeatherDecisionInput& input) {
    static const char* kSystemPrompt =
        "You are a chicken coop door safety assistant. Given weather data and the "
        "door's open window for today, decide if it is safe to let chickens outside "
        "during that window. Respond with ONLY a JSON object of the exact form "
        "{\"open\": true, \"reason\": \"short reason\"} or {\"open\": false, \"reason\": \"short reason\"}. "
        "No other text before or after the JSON.";

    String userPrompt = buildPrompt(input);
    String reply = sendRequest(kSystemPrompt, userPrompt, timeout_ms_);

    bool open = false;
    String reason;
    if (reply.length() > 0 && parseReply(reply, open, reason)) {
        logger.logfInfo("LlmWeatherDecider: decision open=%s (%s)", open ? "yes" : "no", reason.c_str());
        return {open, reason};
    }

    // Fall back to the rule-based decision — still safe, more informative than
    // an unconditional open=true, and consistent with "never trap the chickens
    // inside" when the model is unreachable or returns something unusable.
    logger.logWarning("LlmWeatherDecider: no usable reply, falling back to rule-based decision");
    WeatherDecision fallbackDecision = fallback_.decide(input);
    fallbackDecision.reason = String("llm unavailable, rule fallback: ") + fallbackDecision.reason;
    return fallbackDecision;
}

String LlmWeatherDecider::testConnection() const {
    if (hal_ == nullptr) return "HAL not available";
    if (base_url_.length() == 0) return "Base URL is empty";

    const char* sys = "You are a connectivity test endpoint.";
    const char* usr = "Reply with exactly the single word OK and nothing else.";

    // Honor the configured timeout: a real model on consumer hardware (e.g. a
    // 35B MLX model cold-starting) can take 10-15s to first token. The timeout
    // is already clamped to [5000, 60000] ms in the constructor, so the UI
    // button is still bounded.
    String reply = sendRequest(sys, usr, timeout_ms_);
    if (reply.length() == 0) {
        return "No response from provider (check URL, API key, and that the model is loaded)";
    }
    return "";  // Empty = success
}
