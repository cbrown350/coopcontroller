#include "WeatherManager.h"
#include "LlmWeatherDecider.h"  // Complete type for the owned unique_ptr<LlmWeatherDecider>
#include "Logger.h"
#include <math.h>

WeatherManager::WeatherManager() = default;
WeatherManager::~WeatherManager() = default;  // Out-of-line: needs complete LlmWeatherDecider type

void WeatherManager::begin(IHAL* hal) {
    hal_ = hal;
    logger.logInfo("WeatherManager initialized");
}

void WeatherManager::setUpdateIntervalMinutes(unsigned int minutes) {
    // Clamp to keep well within the OpenWeatherMap free tier (1,000 calls/day).
    // Each cycle makes 2 calls (current + forecast). At the 5-minute floor that
    // is ~576 calls/day; at the default 10 minutes ~288/day.
    if (minutes < 5) minutes = 5;
    if (minutes > 360) minutes = 360;
    update_interval_minutes_ = minutes;
}

void WeatherManager::setLocation(float latitude, float longitude) {
    latitude_ = latitude;
    longitude_ = longitude;
}

void WeatherManager::setOpenWindowMinutes(int openMin, int closeMin) {
    window_open_minutes_ = openMin;
    window_close_minutes_ = closeMin;
}

static LlmProviderWire wireFromType(const String& providerType) {
    if (providerType == "ollama_native") return LlmProviderWire::OLLAMA_NATIVE;
    // "openai_compatible" and "ollama_cloud" both speak the OpenAI schema.
    return LlmProviderWire::OPENAI_COMPATIBLE;
}

void WeatherManager::configureLlmDecider(bool enabled, const String& baseUrl, const String& apiKey,
                                         const String& model, const String& providerType,
                                         unsigned int timeoutSeconds) {
    if (!enabled) {
        llm_decider_.reset();
        setDecider(nullptr);  // restores rule-based default
        return;
    }
    LlmProviderWire wire = wireFromType(providerType);
    auto decider = std::make_unique<LlmWeatherDecider>(
        hal_, baseUrl, apiKey, model, wire,
        static_cast<unsigned long>(timeoutSeconds) * 1000UL);
    LlmWeatherDecider* raw = decider.get();
    llm_decider_ = std::move(decider);
    setDecider(raw);
}

String WeatherManager::testLlmConnection(const String& baseUrl, const String& apiKey,
                                         const String& model, const String& providerType,
                                         unsigned int timeoutSeconds) const {
    // Prefer the configured decider when it already matches the requested
    // provider; otherwise build a throwaway probe so the UI can test unsaved
    // config without disturbing the active decider.
    LlmProviderWire wire = wireFromType(providerType);
    LlmWeatherDecider probe(hal_, baseUrl, apiKey, model, wire,
                            static_cast<unsigned long>(timeoutSeconds) * 1000UL);
    return probe.testConnection();
}

void WeatherManager::requestLlmTest(const String& baseUrl, const String& apiKey,
                                    const String& model, const String& providerType,
                                    unsigned int timeoutSeconds) {
    llm_test_base_url_ = baseUrl;
    llm_test_api_key_ = apiKey;
    llm_test_model_ = model;
    llm_test_provider_type_ = providerType;
    llm_test_timeout_seconds_ = timeoutSeconds;
    llm_test_done_ = false;
    llm_test_success_ = false;
    llm_test_error_ = "";
    pending_test_ = TestKind::LLM;
    logger.logInfo("LLM connection test requested (deferred to main loop)");
}

void WeatherManager::requestWeatherTest(const String& apiKeyOverride) {
    weather_test_api_key_override_ = apiKeyOverride;
    weather_test_done_ = false;
    weather_test_success_ = false;
    weather_test_error_ = "";
    weather_test_ok_before_ = successful_fetches_;
    weather_test_fail_before_ = failed_fetches_;
    pending_test_ = TestKind::WEATHER;
    logger.logInfo("Weather fetch test requested (deferred to main loop)");
}

bool WeatherManager::isTestInProgress() const {
    return pending_test_ != TestKind::NONE || running_test_ != TestKind::NONE;
}

JsonDocument WeatherManager::getLlmTestResultJson() const {
    JsonDocument doc;
    if (pending_test_ == TestKind::LLM || running_test_ == TestKind::LLM) {
        doc["status"] = "pending";
    } else if (llm_test_done_) {
        doc["status"] = llm_test_success_ ? "success" : "error";
        doc["success"] = llm_test_success_;
        if (!llm_test_success_ && llm_test_error_.length() > 0) {
            doc["error"] = llm_test_error_;
        }
    } else {
        doc["status"] = "idle";
        doc["success"] = false;
    }
    return doc;
}

JsonDocument WeatherManager::getWeatherTestResultJson() const {
    JsonDocument doc;
    if (pending_test_ == TestKind::WEATHER || running_test_ == TestKind::WEATHER) {
        doc["status"] = "pending";
    } else if (weather_test_done_) {
        doc["status"] = weather_test_success_ ? "success" : "error";
        doc["success"] = weather_test_success_;
        if (!weather_test_success_ && weather_test_error_.length() > 0) {
            doc["error"] = weather_test_error_;
        }
        if (weather_test_success_) {
            JsonObject status = doc["status_snapshot"].to<JsonObject>();
            const_cast<WeatherManager*>(this)->toJson(status);
        }
    } else {
        doc["status"] = "idle";
        doc["success"] = false;
    }
    return doc;
}

void WeatherManager::runLlmTest_() {
    // Empty base URL means the user hasn't configured a provider yet.
    if (llm_test_base_url_.length() == 0) {
        llm_test_done_ = true;
        llm_test_success_ = false;
        llm_test_error_ = "Base URL is empty";
        return;
    }
    String err = testLlmConnection(llm_test_base_url_, llm_test_api_key_,
                                   llm_test_model_, llm_test_provider_type_,
                                   llm_test_timeout_seconds_);
    llm_test_done_ = true;
    llm_test_success_ = (err.length() == 0);
    llm_test_error_ = err;
    logger.logInfo(llm_test_success_ ? "LLM connection test: OK"
                                      : ("LLM connection test failed: " + err));
}

void WeatherManager::runWeatherTest_() {
    // Apply the optional one-shot API-key override without persisting it.
    bool appliedOverride = false;
    String savedKey = api_key_;
    if (weather_test_api_key_override_.length() > 0) {
        api_key_ = weather_test_api_key_override_;
        appliedOverride = true;
    }
    forceRefresh();  // guarded internally by isReadyToFetch()
    if (appliedOverride) api_key_ = savedKey;

    bool ok = (successful_fetches_ > weather_test_ok_before_) &&
              (failed_fetches_ == weather_test_fail_before_);
    weather_test_done_ = true;
    weather_test_success_ = ok;
    weather_test_error_ = ok ? "" : (last_error_.length() > 0 ? last_error_
                                                              : String("Weather fetch failed"));
}

void WeatherManager::setDecider(IWeatherDecider* decider) {
    decider_ = decider;  // nullptr => fall back to built-in rule-based decider
    // Recompute immediately so status reflects the new engine without waiting
    // for the next fetch (only meaningful once we have data).
    if (current_.valid) recomputeDecision();
}

IWeatherDecider& WeatherManager::activeDecider() {
    return decider_ != nullptr ? *decider_ : default_decider_;
}

const char* WeatherManager::getDeciderName() const {
    return decider_ != nullptr ? decider_->name() : default_decider_.name();
}

bool WeatherManager::isReadyToFetch() const {
    if (!hal_ || !enabled_) return false;
    if (api_key_.length() == 0) return false;
    if (!hal_->WiFiIsConnected()) return false;
    // Crash-safety (issue #4): never attempt TLS/JSON allocations under heap
    // pressure. Below the floor, an mbedtls context or JsonDocument alloc can
    // throw std::bad_alloc which, with -fexceptions and no catch on the loop
    // task, reboots the device. Skip and wait for heap to recover.
    if (hal_->getFreeHeap() < WEATHER_MIN_FREE_HEAP) return false;
    return true;
}

void WeatherManager::update() {
    // Consume a deferred connection test BEFORE the isReadyToFetch() guard:
    // a test request still needs to run (and report failure) when WiFi is
    // down or heap is low, so the UI gets a real error instead of timing out.
    if (pending_test_ == TestKind::LLM) {
        TestKind kind = pending_test_;
        pending_test_ = TestKind::NONE;
        running_test_ = kind;
        runLlmTest_();
        running_test_ = TestKind::NONE;
        return;
    }
    if (pending_test_ == TestKind::WEATHER) {
        TestKind kind = pending_test_;
        pending_test_ = TestKind::NONE;
        running_test_ = kind;
        runWeatherTest_();
        running_test_ = TestKind::NONE;
        return;
    }

    if (!isReadyToFetch()) return;

    unsigned long now = hal_->millis();
    unsigned long intervalMs = static_cast<unsigned long>(update_interval_minutes_) * 60UL * 1000UL;

    // First fetch happens immediately once ready; subsequent fetches wait the interval.
    if (last_fetch_attempt_ms_ != 0 && (now - last_fetch_attempt_ms_) < intervalMs) {
        return;
    }

    fetchWeather();
}

void WeatherManager::forceRefresh() {
    if (!isReadyToFetch()) {
        logger.logDebug("WeatherManager: forceRefresh skipped (not ready)");
        return;
    }
    fetchWeather();
}

void WeatherManager::fetchWeather() {
    last_fetch_attempt_ms_ = hal_->millis();

    // Build current-weather URL. Coordinates come from the shared location
    // settings already used for sunrise/sunset, so no extra config needed.
    char latBuf[16];
    char lonBuf[16];
    snprintf(latBuf, sizeof(latBuf), "%.4f", latitude_);
    snprintf(lonBuf, sizeof(lonBuf), "%.4f", longitude_);

    String base = "https://api.openweathermap.org/data/2.5/";
    String common = "?lat=" + String(latBuf) + "&lon=" + String(lonBuf) +
                    "&units=" + units_ + "&appid=" + api_key_;

    // --- Current weather ---
    String currentUrl = base + "weather" + common;
    String currentBody = hal_->httpGet(currentUrl, 10000);
    if (currentBody.length() == 0) {
        failed_fetches_++;
        last_error_ = "No response from OpenWeatherMap (current)";
        logger.logWarning("WeatherManager: current weather fetch failed (no response)");
        return;
    }

    if (!parseCurrentJson(currentBody)) {
        failed_fetches_++;
        // last_error_ set by parseCurrentJson
        return;
    }

    // --- Short-term forecast (best-effort; failure here doesn't invalidate current) ---
    // cnt=MAX_FORECAST_ENTRIES keeps the response small (a few 3-hour blocks).
    String forecastUrl = base + "forecast" + common + "&cnt=" + String((unsigned)MAX_FORECAST_ENTRIES);
    String forecastBody = hal_->httpGet(forecastUrl, 10000);
    if (forecastBody.length() > 0) {
        parseForecastJson(forecastBody);  // Non-fatal on failure
    }

    successful_fetches_++;
    last_error_ = "";

    // Recompute the open/no-open decision once per successful fetch. The
    // decider (rule-based today, LLM tomorrow) reasons over the fresh snapshot;
    // the result is cached so the door gate and status page read it cheaply
    // without any per-loop network or model calls.
    recomputeDecision();

    logger.logfInfo("WeatherManager: weather updated (%s, %.1f, condition=%s, open=%s [%s])",
                    current_.main_description, current_.temp,
                    current_.condition == WeatherCondition::GOOD ? "GOOD" :
                    current_.condition == WeatherCondition::INCLEMENT ? "INCLEMENT" : "UNKNOWN",
                    decision_open_ ? "yes" : "no", getDeciderName());
}

void WeatherManager::recomputeDecision() {
    if (!current_.valid) {
        decision_open_ = true;
        decision_reason_ = "";
        return;
    }
    WeatherDecisionInput input{current_, forecast_, forecast_count_, units_,
                               window_open_minutes_, window_close_minutes_};
    WeatherDecision decision = activeDecider().decide(input);
    decision_open_ = decision.open;
    decision_reason_ = decision.reason;
}

bool WeatherManager::parseCurrentJson(const String& body) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err != DeserializationError::Ok) {
        last_error_ = String("Current JSON parse error: ") + err.c_str();
        logger.logfWarning("WeatherManager: %s", last_error_.c_str());
        return false;
    }

    // OpenWeatherMap returns cod=200 on success; anything else is an API error
    // (e.g. 401 invalid key, 429 rate limited). "cod" can be int or string.
    int cod = 0;
    if (doc["cod"].is<int>()) cod = doc["cod"].as<int>();
    else if (doc["cod"].is<const char*>()) cod = atoi(doc["cod"].as<const char*>());
    if (cod != 200) {
        last_error_ = doc["message"].is<const char*>() ? doc["message"].as<String>()
                                                        : String("API error code ") + cod;
        logger.logfWarning("WeatherManager: API error: %s", last_error_.c_str());
        return false;
    }

    WeatherSnapshot snap;  // Build locally, commit only on success
    JsonObject main = doc["main"].as<JsonObject>();
    snap.temp = main["temp"] | NAN;
    snap.feels_like = main["feels_like"] | NAN;
    snap.humidity = main["humidity"] | -1;
    snap.pressure_hpa = main["pressure"] | -1;
    snap.wind_speed = doc["wind"]["speed"] | -1.0f;
    snap.cloudiness = doc["clouds"]["all"] | -1;
    snap.fetch_time = doc["dt"] | 0L;

    JsonArray weatherArr = doc["weather"].as<JsonArray>();
    if (!weatherArr.isNull() && weatherArr.size() > 0) {
        JsonObject w0 = weatherArr[0].as<JsonObject>();
        snap.weather_code = w0["id"] | 0;
        copyTruncated(snap.main_description, sizeof(snap.main_description),
                      w0["main"] | "");
        copyTruncated(snap.icon_code, sizeof(snap.icon_code), w0["icon"] | "");
    }

    // Provide a coarse GOOD/INCLEMENT label for display. The authoritative
    // open/no-open call is made by the decider in recomputeDecision().
    snap.condition = RuleBasedWeatherDecider::classify(snap.weather_code,
                                                       snap.wind_speed, snap.temp, units_);
    snap.valid = true;

    current_ = snap;
    return true;
}

bool WeatherManager::parseForecastJson(const String& body) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err != DeserializationError::Ok) {
        logger.logfDebug("WeatherManager: forecast JSON parse error: %s", err.c_str());
        return false;
    }

    JsonArray list = doc["list"].as<JsonArray>();
    if (list.isNull()) return false;

    size_t count = 0;
    for (JsonObject entry : list) {
        if (count >= MAX_FORECAST_ENTRIES) break;
        WeatherForecastEntry& fe = forecast_[count];
        fe.dt = entry["dt"] | 0L;
        fe.temp = entry["main"]["temp"] | NAN;
        fe.wind_speed = entry["wind"]["speed"] | -1.0f;
        fe.precip_prob = entry["pop"] | -1.0f;
        JsonArray weatherArr = entry["weather"].as<JsonArray>();
        if (!weatherArr.isNull() && weatherArr.size() > 0) {
            copyTruncated(fe.main_description, sizeof(fe.main_description),
                          weatherArr[0]["main"] | "");
        } else {
            fe.main_description[0] = '\0';
        }
        count++;
    }
    forecast_count_ = count;
    return true;
}

// Classify OpenWeatherMap condition into GOOD / INCLEMENT for door gating.
// OWM weather condition codes: https://openweathermap.org/weather-conditions
//   2xx Thunderstorm, 3xx Drizzle, 5xx Rain, 6xx Snow, 7xx Atmosphere
//   (fog/dust/etc.), 800 Clear, 80x Clouds.
// Anything precipitation-related is inclement. We also gate on high wind and
// extreme cold, which are the coop-relevant hazards for opening the door.
WeatherCondition RuleBasedWeatherDecider::classify(int code, float wind,
                                                   float temp, const String& units) {
    // Precipitation / severe atmosphere by condition code
    if (code >= 200 && code < 800) {
        // 7xx atmosphere: 701 mist, 711 smoke, 721 haze, 731 dust, 741 fog,
        // 751 sand, 761 dust, 762 ash, 771 squall, 781 tornado.
        // Only squall/tornado are truly hazardous; mist/haze/fog are fine for
        // the door. Treat < 771 in the 7xx band as acceptable.
        if (code >= 700 && code < 771) {
            // fall through to wind/temp checks below
        } else {
            return WeatherCondition::INCLEMENT;
        }
    }

    // High wind gate. OWM wind is m/s for metric/standard, mph for imperial.
    if (wind >= 0.0f) {
        float windMph = (units == "imperial") ? wind : wind * 2.23694f;
        if (windMph >= 25.0f) {  // sustained ~25 mph = keep door closed
            return WeatherCondition::INCLEMENT;
        }
    }

    // Extreme cold gate. Convert to °F for a single threshold.
    if (!isnan(temp)) {
        float tempF;
        if (units == "imperial") tempF = temp;
        else if (units == "metric") tempF = temp * 9.0f / 5.0f + 32.0f;
        else tempF = (temp - 273.15f) * 9.0f / 5.0f + 32.0f;  // standard = Kelvin
        if (tempF <= 10.0f) {  // bitter cold — keep chickens in
            return WeatherCondition::INCLEMENT;
        }
    }

    return WeatherCondition::GOOD;
}

// Default open/no-open decision: block on inclement current conditions or a
// high probability of imminent precipitation.
WeatherDecision RuleBasedWeatherDecider::decide(const WeatherDecisionInput& input) {
    WeatherCondition cond = classify(input.current.weather_code,
                                     input.current.wind_speed,
                                     input.current.temp, input.units);
    if (cond != WeatherCondition::GOOD) {
        String reason = input.current.main_description[0] != '\0'
                            ? String("inclement: ") + input.current.main_description
                            : String("inclement conditions");
        return {false, reason};
    }

    // Block if the immediate forecast (next 3-hour block) shows likely
    // precipitation. This prevents opening during a brief clearing right before
    // rain arrives, which is exactly the case the door should avoid.
    if (input.forecast_count > 0) {
        const WeatherForecastEntry& next = input.forecast[0];
        if (next.precip_prob >= 0.5f) {
            return {false, "rain likely soon"};
        }
    }

    return {true, "clear"};
}

bool WeatherManager::isWeatherGateActive() const {
    return enabled_ && api_key_.length() > 0 && current_.valid;
}

bool WeatherManager::isWeatherGoodForOpening() const {
    // When the gate isn't active (disabled, unconfigured, or no data yet), do
    // not block the door — fall back to schedule-only behavior. This ensures a
    // weather API outage never traps the chickens inside. Otherwise return the
    // cached decision the active decider computed at the last successful fetch.
    if (!isWeatherGateActive()) return true;
    return decision_open_;
}

void WeatherManager::toJson(JsonObject& json) const {
    json["enabled"] = enabled_;
    json["configured"] = (api_key_.length() > 0);
    json["units"] = units_;
    json["gate_active"] = isWeatherGateActive();
    json["good_for_opening"] = isWeatherGoodForOpening();
    json["decider"] = getDeciderName();
    if (decision_reason_.length() > 0) json["decision_reason"] = decision_reason_;
    json["update_interval_minutes"] = update_interval_minutes_;
    json["successful_fetches"] = successful_fetches_;
    json["failed_fetches"] = failed_fetches_;
    if (last_error_.length() > 0) json["last_error"] = last_error_;

    if (current_.valid) {
        JsonObject cur = json["current"].to<JsonObject>();
        cur["condition"] = current_.condition == WeatherCondition::GOOD ? "GOOD" :
                           current_.condition == WeatherCondition::INCLEMENT ? "INCLEMENT" : "UNKNOWN";
        cur["description"] = current_.main_description;
        cur["icon"] = current_.icon_code;
        if (!isnan(current_.temp)) cur["temp"] = current_.temp;
        if (!isnan(current_.feels_like)) cur["feels_like"] = current_.feels_like;
        if (current_.humidity >= 0) cur["humidity"] = current_.humidity;
        if (current_.wind_speed >= 0) cur["wind_speed"] = current_.wind_speed;
        if (current_.pressure_hpa >= 0) cur["pressure"] = current_.pressure_hpa;
        if (current_.cloudiness >= 0) cur["cloudiness"] = current_.cloudiness;
        cur["fetch_time"] = current_.fetch_time;
    }

    if (forecast_count_ > 0) {
        JsonArray fc = json["forecast"].to<JsonArray>();
        for (size_t i = 0; i < forecast_count_; i++) {
            JsonObject fe = fc.add<JsonObject>();
            fe["dt"] = forecast_[i].dt;
            if (!isnan(forecast_[i].temp)) fe["temp"] = forecast_[i].temp;
            if (forecast_[i].wind_speed >= 0) fe["wind_speed"] = forecast_[i].wind_speed;
            if (forecast_[i].precip_prob >= 0) fe["precip_prob"] = forecast_[i].precip_prob;
            fe["description"] = forecast_[i].main_description;
        }
    }
}

void WeatherManager::copyTruncated(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0) return;
    if (src == nullptr) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}
