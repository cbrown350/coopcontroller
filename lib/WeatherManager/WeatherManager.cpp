#include "WeatherManager.h"
#include "LlmWeatherDecider.h"  // Complete type for the owned unique_ptr<LlmWeatherDecider>
#include "Logger.h"
#include <math.h>
#include <time.h>

namespace {
// Brief RAII guard around HAL's sharedStateMutex_ for loop-task mutations and
// snapshots. Never held across blocking HTTP/TLS calls so async_tcp web
// handlers (/weather/status, /update_settings) never stall or read torn Strings.
class ScopedSharedLock {
public:
    explicit ScopedSharedLock(IHAL* hal, unsigned long timeoutMs = 100)
        : hal_(hal), locked_(hal != nullptr && hal->lockSharedState(timeoutMs)) {}
    ~ScopedSharedLock() {
        if (locked_) hal_->unlockSharedState();
    }
    bool ok() const { return hal_ == nullptr || locked_; }
    ScopedSharedLock(const ScopedSharedLock&) = delete;
    ScopedSharedLock& operator=(const ScopedSharedLock&) = delete;

private:
    IHAL* hal_;
    bool locked_;
};
}  // namespace

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

void WeatherManager::setDoorOpenContext(bool autoOpenEligibleToday, bool doorIsOpen,
                                        bool doorFaulted, int currentLocalMinutes) {
    door_context_configured_ = true;
    door_auto_open_eligible_today_ = autoOpenEligibleToday;
    door_is_open_ = doorIsOpen;
    door_faulted_ = doorFaulted;
    door_current_local_minutes_ = currentLocalMinutes;
}

bool WeatherManager::isLlmActive_() const {
    return llm_enabled_;
}

bool WeatherManager::getLocalMinutesNow_(int& outNowMin) const {
    if (door_current_local_minutes_ >= 0) {
        outNowMin = door_current_local_minutes_;
        return true;
    }
    long epoch = -1;
    if (hal_ != nullptr) {
        unsigned long liveEpoch = hal_->getTime();
        if (liveEpoch >= MIN_VALID_EPOCH) {
            epoch = static_cast<long>(liveEpoch);
        }
    }
    if (epoch < static_cast<long>(MIN_VALID_EPOCH) &&
        now_epoch_ >= static_cast<long>(MIN_VALID_EPOCH)) {
        epoch = now_epoch_;
    }
    if (epoch < static_cast<long>(MIN_VALID_EPOCH)) {
        return false;
    }
    long local = epoch + static_cast<long>(tz_offset_minutes_) * 60L;
    outNowMin = static_cast<int>((((local % 86400L) + 86400L) % 86400L) / 60L);
    return true;
}

bool WeatherManager::isLlmEvaluationNeeded() const {
    if (!isLlmActive_()) return false;
    // No DoorController pushed context (standalone use / unit tests): keep the
    // legacy behavior of running the LLM on every successful fetch.
    if (!door_context_configured_) return true;

    // The decision is moot when auto-open cannot happen today, the door is
    // already open (it stays open until closed), or the door is faulted.
    if (!door_auto_open_eligible_today_ || door_is_open_ || door_faulted_) return false;

    // No usable open window -> nothing to decide.
    if (window_open_minutes_ < 0 || window_close_minutes_ < 0 ||
        window_open_minutes_ >= window_close_minutes_) {
        return false;
    }

    int nowMin = -1;
    if (!getLocalMinutesNow_(nowMin)) {
        return false;
    }

    // Actionable window: from LLM_PRE_OPEN_LEAD_MINUTES before the scheduled
    // open (clamped at midnight) up to the close ceiling.
    int leadStart = window_open_minutes_ - LLM_PRE_OPEN_LEAD_MINUTES;
    if (leadStart < 0) leadStart = 0;
    return nowMin >= leadStart && nowMin < window_close_minutes_;
}

bool WeatherManager::isLlmStandby() const {
    return isLlmActive_() && !isLlmEvaluationNeeded();
}

const char* WeatherManager::getLlmStandbyReason() const {
    if (!isLlmStandby()) return "";
    if (!door_auto_open_eligible_today_) return "auto-open disabled today";
    if (door_is_open_) return "door already open";
    if (door_faulted_) return "door faulted";
    // From here the window may or may not be valid; only report an unsynced
    // clock when a valid window actually exists to evaluate.
    bool windowValid = window_open_minutes_ >= 0 && window_close_minutes_ >= 0 &&
                       window_open_minutes_ < window_close_minutes_;
    int unusedNowMin = -1;
    if (windowValid && !getLocalMinutesNow_(unusedNowMin)) return "clock not synced";
    return "outside open window";
}

bool WeatherManager::isLlmDecisionPending() const {
    return isLlmActive_() && current_.valid && !isWeatherStale() &&
           isLlmEvaluationNeeded() &&
           (llm_attempted_fetch_id_ != successful_fetches_) &&
           isReadyToFetch();
}

void WeatherManager::setLocalTempF(float tempF, const String& source) {
    local_temp_f_ = tempF;
    local_temp_source_ = source;
}

static LlmProviderWire wireFromType(const String& providerType) {
    if (providerType == "ollama_native") return LlmProviderWire::OLLAMA_NATIVE;
    // "openai_compatible" and "ollama_cloud" both speak the OpenAI schema.
    return LlmProviderWire::OPENAI_COMPATIBLE;
}

void WeatherManager::configureLlmDecider(bool enabled, const String& baseUrl, const String& apiKey,
                                         const String& model, const String& providerType,
                                         unsigned int timeoutSeconds,
                                         const String& promptOverride) {
    // Stage configuration only (runs on async_tcp under sharedStateMutex_ when
    // triggered from /update_settings). Never construct, destroy, or invoke
    // llm_decider_ here: loopTask may be mid-TLS inside decide(). update() on
    // loopTask calls applyPendingLlmConfig_() to materialize the change safely.
    staged_llm_config_.enabled = enabled;
    staged_llm_config_.base_url = baseUrl;
    staged_llm_config_.api_key = apiKey;
    staged_llm_config_.model = model;
    staged_llm_config_.provider_type = providerType;
    staged_llm_config_.timeout_seconds = timeoutSeconds;
    staged_llm_config_.prompt_override = promptOverride;

    llm_enabled_ = enabled;
    llm_config_dirty_ = true;
    llm_config_generation_++;
    llm_attempted_fetch_id_ = 0;
    llm_last_attempt_ok_ = false;
    llm_last_error_ = "";
    // Clear any external custom decider pointer so the logical strategy matches
    // the configured LLM / rule-based selection immediately.
    if (decider_ != llm_decider_.get()) {
        decider_ = nullptr;
    }
}

void WeatherManager::applyPendingLlmConfig_() {
    StagedLlmConfig cfg;
    uint32_t gen = 0;
    bool hasValid = false;
    std::unique_ptr<LlmWeatherDecider> oldDecider;
    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok() || !llm_config_dirty_) return;
        if (!staged_llm_config_.enabled) {
            // Single-lock disable: clear decider_ before moving llm_decider_ out
            // so decider_ never dangles and no second lock is needed.
            llm_config_dirty_ = false;
            hasValid = current_.valid;
            if (decider_ == llm_decider_.get()) {
                decider_ = nullptr;
            }
            oldDecider = std::move(llm_decider_);
            // Fall through after lock release to destroy oldDecider & recompute.
        } else {
            // Keep llm_config_dirty_ true until the second lock commits so a
            // lock timeout or concurrent reconfigure retries cleanly next tick
            // without ever writing llm_config_dirty_ unlocked.
            cfg = staged_llm_config_;
            gen = llm_config_generation_;
        }
    }

    if (!cfg.enabled) {
        oldDecider.reset();
        if (hasValid) {
            recomputeDecision();
        }
        return;
    }

    LlmProviderWire wire = wireFromType(cfg.provider_type);
    auto newDecider = std::make_unique<LlmWeatherDecider>(
        hal_, cfg.base_url, cfg.api_key, cfg.model, wire,
        static_cast<unsigned long>(cfg.timeout_seconds) * 1000UL);
    newDecider->setPromptOverride(cfg.prompt_override);

    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok() || gen != llm_config_generation_) {
            // Either timed out or a newer configureLlmDecider() arrived while
            // we constructed newDecider; llm_config_dirty_ is still true so the
            // next tick will apply the latest config. Drop newDecider unlocked.
            return;
        }
        llm_config_dirty_ = false;
        oldDecider = std::move(llm_decider_);
        if (decider_ == oldDecider.get()) {
            decider_ = nullptr;
        }
        llm_decider_ = std::move(newDecider);
        if (llm_enabled_) {
            decider_ = llm_decider_.get();
        }
    }
    oldDecider.reset();
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
    String baseUrl, apiKey, model, providerType;
    unsigned int timeoutSeconds = 15;
    {
        ScopedSharedLock lock(hal_, 1000);
        if (!lock.ok()) return;
        baseUrl = llm_test_base_url_;
        apiKey = llm_test_api_key_;
        model = llm_test_model_;
        providerType = llm_test_provider_type_;
        timeoutSeconds = llm_test_timeout_seconds_;
    }

    // Empty base URL means the user hasn't configured a provider yet.
    if (baseUrl.length() == 0) {
        ScopedSharedLock lock(hal_, 1000);
        if (lock.ok()) {
            llm_test_done_ = true;
            llm_test_success_ = false;
            llm_test_error_ = "Base URL is empty";
            running_test_ = TestKind::NONE;
        }
        return;
    }
    String err = testLlmConnection(baseUrl, apiKey, model, providerType, timeoutSeconds);
    bool ok = (err.length() == 0);
    {
        ScopedSharedLock lock(hal_, 1000);
        if (lock.ok()) {
            llm_test_done_ = true;
            llm_test_success_ = ok;
            llm_test_error_ = err;
            running_test_ = TestKind::NONE;
        }
    }
    logger.logInfo(ok ? "LLM connection test: OK"
                      : ("LLM connection test failed: " + err));
}

void WeatherManager::runWeatherTest_() {
    String apiKeyOverride;
    unsigned int okBefore = 0;
    unsigned int failBefore = 0;
    bool ready = false;
    {
        ScopedSharedLock lock(hal_, 1000);
        if (!lock.ok()) return;
        apiKeyOverride = weather_test_api_key_override_;
        okBefore = weather_test_ok_before_;
        failBefore = weather_test_fail_before_;
        ready = hal_ != nullptr && enabled_ &&
                (apiKeyOverride.length() > 0 || api_key_.length() > 0) &&
                hal_->WiFiIsConnected() && hal_->getFreeHeap() >= WEATHER_MIN_FREE_HEAP;
    }

    if (ready) {
        fetchWeather(apiKeyOverride);
    } else {
        logger.logDebug("WeatherManager: runWeatherTest_ skipped (not ready)");
    }

    {
        ScopedSharedLock lock(hal_, 1000);
        if (lock.ok()) {
            bool ok = (successful_fetches_ > okBefore) &&
                      (failed_fetches_ == failBefore);
            weather_test_done_ = true;
            weather_test_success_ = ok;
            weather_test_error_ = ok ? "" : (last_error_.length() > 0 ? last_error_
                                                                      : String("Weather fetch failed"));
            running_test_ = TestKind::NONE;
        }
    }
}

void WeatherManager::setDecider(IWeatherDecider* decider) {
    decider_ = decider;  // nullptr => fall back to built-in rule-based decider
    llm_enabled_ = (decider != nullptr && llm_decider_ != nullptr && decider == llm_decider_.get());
    llm_config_dirty_ = false;
    // Recompute immediately so status reflects the new engine without waiting
    // for the next fetch (only meaningful once we have data). Refresh the time
    // anchor first so a decider swapped in mid-cycle gets real local times.
    if (current_.valid) {
        refreshLocalTime_();
        recomputeDecision();
    }
}

IWeatherDecider& WeatherManager::activeDecider() {
    if (llm_enabled_ && llm_decider_ != nullptr) return *llm_decider_;
    return (decider_ != nullptr && decider_ != llm_decider_.get()) ? *decider_ : default_decider_;
}

const char* WeatherManager::getDeciderName() const {
    if (llm_enabled_) return "llm";
    if (decider_ != nullptr && decider_ != llm_decider_.get()) return decider_->name();
    return default_decider_.name();
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
    // Apply any staged LLM configuration changes on loopTask before any test,
    // fetch, or evaluation runs. Keeps unique_ptr ownership single-threaded.
    applyPendingLlmConfig_();

    // Consume a deferred connection test BEFORE the isReadyToFetch() guard:
    // a test request still needs to run (and report failure) when WiFi is
    // down or heap is low, so the UI gets a real error instead of timing out.
    TestKind testToRun = TestKind::NONE;
    {
        ScopedSharedLock lock(hal_);
        if (lock.ok()) {
            // Only loopTask runs deferred tests; clear any stale running_test_
            // left behind by an earlier commit-lock timeout so it can never wedge.
            if (running_test_ != TestKind::NONE) {
                running_test_ = TestKind::NONE;
            }
            if (pending_test_ != TestKind::NONE) {
                testToRun = pending_test_;
                pending_test_ = TestKind::NONE;
                running_test_ = testToRun;
            }
        }
    }
    if (testToRun == TestKind::LLM) {
        runLlmTest_();
        return;
    }
    if (testToRun == TestKind::WEATHER) {
        runWeatherTest_();
        return;
    }

    bool dueForFetch = false;
    bool needStandbyFallbackCleanup = false;
    bool needOnDemandLlm = false;
    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok()) return;

        pruneExpiredForecast();

        if (current_.valid && isWeatherStale() && !stale_logged_) {
            stale_logged_ = true;
            logger.logfWarning(
                "WeatherManager: cached weather expired (%lu s old, %u consecutive failures); weather gate failing open",
                getWeatherAgeSeconds(), consecutive_failures_);
        }

        // If we entered standby while holding an "llm unavailable..." fallback
        // reason from an earlier failure, refresh cleanly with local rules so
        // the status card doesn't display stale fallback error text in standby.
        if (isLlmStandby() && current_.valid &&
            decision_reason_.startsWith("llm unavailable")) {
            needStandbyFallbackCleanup = true;
        }

        if (isReadyToFetch()) {
            unsigned long now = hal_->millis();
            unsigned long intervalMs = static_cast<unsigned long>(update_interval_minutes_) * 60UL * 1000UL;
            if (last_fetch_attempt_ms_ == 0 || (now - last_fetch_attempt_ms_) >= intervalMs) {
                dueForFetch = true;
            } else if (isLlmDecisionPending()) {
                needOnDemandLlm = true;
            }
        }
    }

    // Periodic fetch: the first one happens immediately once ready; subsequent
    // fetches wait the interval.
    if (dueForFetch) {
        fetchWeather();
        return;
    }

    if (needStandbyFallbackCleanup) {
        recomputeDecision();
    }

    // Between fetches, run a deferred on-demand LLM evaluation when one is owed
    // (e.g. the door closed mid-window after a fetch that skipped the LLM while
    // it was open, or we just crossed into the pre-open lead window). This runs
    // at most once per successful fetch via llm_attempted_fetch_id_.
    if (needOnDemandLlm) {
        bool stillPending = false;
        {
            ScopedSharedLock lock(hal_);
            if (!lock.ok()) return;
            refreshLocalTime_();
            pruneExpiredForecast();
            stillPending = isLlmDecisionPending();
        }
        if (stillPending) {
            recomputeDecision();
            bool attempted = false;
            bool openSnap = true;
            String reasonSnap;
            {
                ScopedSharedLock lock(hal_);
                if (lock.ok() && llm_attempted_fetch_id_ == successful_fetches_) {
                    attempted = true;
                    openSnap = decision_open_;
                    reasonSnap = decision_reason_;
                }
            }
            if (attempted) {
                logger.logfInfo("WeatherManager: on-demand LLM weather evaluation completed (open=%s, reason=%s)",
                                openSnap ? "yes" : "no", reasonSnap.c_str());
            }
        }
    }
}

void WeatherManager::forceRefresh() {
    applyPendingLlmConfig_();
    bool ready = false;
    {
        ScopedSharedLock lock(hal_);
        ready = lock.ok() && isReadyToFetch();
    }
    if (!ready) {
        logger.logDebug("WeatherManager: forceRefresh skipped (not ready)");
        return;
    }
    fetchWeather();
}

void WeatherManager::fetchWeather(const String& apiKeyOverride) {
    float lat = 0.0f;
    float lon = 0.0f;
    String units;
    String apiKey;
    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok() || hal_ == nullptr) return;
        last_fetch_attempt_ms_ = hal_->millis();
        lat = latitude_;
        lon = longitude_;
        units = units_;
        apiKey = (apiKeyOverride.length() > 0) ? apiKeyOverride : api_key_;
    }

    // Build current-weather URL. Coordinates come from the shared location
    // settings already used for sunrise/sunset, so no extra config needed.
    char latBuf[16];
    char lonBuf[16];
    snprintf(latBuf, sizeof(latBuf), "%.4f", lat);
    snprintf(lonBuf, sizeof(lonBuf), "%.4f", lon);

    String base = "https://api.openweathermap.org/data/2.5/";
    String common = "?lat=" + String(latBuf) + "&lon=" + String(lonBuf) +
                    "&units=" + units + "&appid=" + apiKey;

    // --- Current weather (unlocked network I/O) ---
    String currentUrl = base + "weather" + common;
    String currentBody = hal_->httpGet(currentUrl, 10000);
    if (currentBody.length() == 0) {
        String halErr = hal_->getLastHttpError();
        String errMsg = halErr.length() > 0
                            ? ("No response from OpenWeatherMap (" + halErr + ")")
                            : String("No response from OpenWeatherMap (current)");
        {
            ScopedSharedLock lock(hal_);
            if (lock.ok()) {
                failed_fetches_++;
                consecutive_failures_++;
                last_error_ = errMsg;
            }
        }
        logger.logfWarning("WeatherManager: current weather fetch failed (%s, free=%u, max_alloc=%u)",
                           errMsg.c_str(),
                           (unsigned)hal_->getFreeHeap(),
                           (unsigned)hal_->getMaxAllocHeap());
        return;
    }

    WeatherSnapshot snap;
    String parseErr;
    if (!parseCurrentJsonTo(currentBody, units, snap, parseErr)) {
        ScopedSharedLock lock(hal_);
        if (lock.ok()) {
            failed_fetches_++;
            consecutive_failures_++;
            last_error_ = parseErr;
        }
        return;
    }

    // --- Short-term forecast (best-effort; unlocked network I/O) ---
    // cnt=MAX_FORECAST_ENTRIES keeps the response small (a few 3-hour blocks).
    String forecastUrl = base + "forecast" + common + "&cnt=" + String((unsigned)MAX_FORECAST_ENTRIES);
    String forecastBody = hal_->httpGet(forecastUrl, 10000);
    WeatherForecastEntry localForecast[MAX_FORECAST_ENTRIES];
    size_t localForecastCount = 0;
    bool forecastOk = (forecastBody.length() > 0 &&
                       parseForecastJsonTo(forecastBody, localForecast,
                                           MAX_FORECAST_ENTRIES, localForecastCount));
    String forecastErr;
    if (!forecastOk) {
        String halErr = hal_->getLastHttpError();
        forecastErr = halErr.length() > 0
                          ? ("Forecast fetch failed (" + halErr + ")")
                          : String("Forecast could not be updated");
    }

    // Commit parsed weather snapshot + forecast under sharedStateMutex_.
    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok()) return;
        current_ = snap;
        if (forecastOk) {
            for (size_t i = 0; i < localForecastCount; i++) {
                forecast_[i] = localForecast[i];
            }
            forecast_count_ = localForecastCount;
            last_forecast_success_epoch_ = currentWallEpoch_();
            forecast_error_ = "";
            forecast_expired_ = false;
        } else {
            forecast_error_ = forecastErr;
            pruneExpiredForecast();
            if (forecast_count_ == 0) {
                forecast_expired_ = true;
            }
        }

        successful_fetches_++;
        consecutive_failures_ = 0;
        last_successful_fetch_ms_ = hal_->millis();
        last_weather_success_epoch_ = currentWallEpoch_();
        stale_logged_ = false;
        last_error_ = "";

        // Snapshot the current local-time anchor (epoch + DST-aware UTC offset +
        // short tz name) so the decider can render every time in the prompt on the
        // same wall clock the UI shows. Done once per fetch; cheap.
        refreshLocalTime_();

        // Ensure no already-passed forecast blocks are fed into the decision engine
        pruneExpiredForecast();
    }

    // Recompute the open/no-open decision once per successful fetch. The
    // decider (rule-based today, LLM tomorrow) reasons over the fresh snapshot;
    // the result is cached so the door gate and status page read it cheaply
    // without any per-loop network or model calls.
    recomputeDecision();

    char descBuf[sizeof(current_.main_description)] = {0};
    float tempSnap = NAN;
    WeatherCondition condSnap = WeatherCondition::UNKNOWN;
    bool openSnap = true;
    const char* deciderSnap = "rules";
    {
        ScopedSharedLock lock(hal_);
        if (lock.ok()) {
            copyTruncated(descBuf, sizeof(descBuf), current_.main_description);
            tempSnap = current_.temp;
            condSnap = current_.condition;
            openSnap = decision_open_;
            deciderSnap = last_decision_decider_;
        }
    }

    logger.logfInfo("WeatherManager: weather updated (%s, %.1f, condition=%s, open=%s [%s])",
                    descBuf, tempSnap,
                    condSnap == WeatherCondition::GOOD ? "GOOD" :
                    condSnap == WeatherCondition::INCLEMENT ? "INCLEMENT" : "UNKNOWN",
                    openSnap ? "yes" : "no", deciderSnap);
}

unsigned long WeatherManager::currentWallEpoch_() const {
    if (hal_ != nullptr) {
        unsigned long t = hal_->getTime();
        if (t >= MIN_VALID_EPOCH) return t;
    }
    if (current_.valid && current_.fetch_time > 0) {
        return static_cast<unsigned long>(current_.fetch_time);
    }
    return 0;
}

bool WeatherManager::isForecastEntryExpired(const WeatherForecastEntry& fe, unsigned long nowEpoch) const {
    if (nowEpoch < MIN_VALID_EPOCH || fe.dt <= 0) return false;
    return static_cast<unsigned long>(fe.dt) + FORECAST_BLOCK_DURATION_SECONDS <= nowEpoch;
}

void WeatherManager::pruneExpiredForecast() {
    if (forecast_count_ == 0) return;

    // If overall weather data has exceeded its staleness TTL, clear old forecast
    if (isWeatherStale()) {
        forecast_count_ = 0;
        if (!forecast_expired_) {
            forecast_expired_ = true;
            logger.logWarning("WeatherManager: cleared expired forecast (weather data stale)");
        }
        return;
    }

    unsigned long nowEpoch = (hal_ != nullptr) ? hal_->getTime() : 0;
    if (nowEpoch < MIN_VALID_EPOCH) return;

    size_t kept = 0;
    for (size_t i = 0; i < forecast_count_; i++) {
        if (!isForecastEntryExpired(forecast_[i], nowEpoch)) {
            if (kept != i) forecast_[kept] = forecast_[i];
            kept++;
        }
    }
    if (kept != forecast_count_) {
        forecast_count_ = kept;
        if (forecast_count_ == 0 && !forecast_expired_) {
            forecast_expired_ = true;
            logger.logWarning("WeatherManager: all cached forecast blocks have passed; cleared stale forecast");
        }
    }
}

void WeatherManager::recomputeDecision() {
    applyPendingLlmConfig_();

    WeatherSnapshot snap;
    WeatherForecastEntry localForecast[MAX_FORECAST_ENTRIES];
    size_t localForecastCount = 0;
    String units;
    int windowOpen = -1;
    int windowClose = -1;
    long nowEpoch = -1;
    int tzOffset = 0;
    String tzName;
    float localTempF = NAN;
    String localTempSource;
    bool llmActive = false;
    bool evalNeeded = false;
    const char* standbyReason = "";
    unsigned int fetchId = 0;
    uint32_t configGen = 0;
    IWeatherDecider* customDecider = nullptr;

    {
        ScopedSharedLock lock(hal_);
        if (!lock.ok()) return;
        if (!current_.valid) {
            decision_open_ = true;
            decision_reason_ = "";
            last_decision_decider_ = "rules";
            return;
        }
        snap = current_;
        localForecastCount = forecast_count_;
        for (size_t i = 0; i < localForecastCount; i++) {
            localForecast[i] = forecast_[i];
        }
        units = units_;
        windowOpen = window_open_minutes_;
        windowClose = window_close_minutes_;
        nowEpoch = now_epoch_;
        tzOffset = tz_offset_minutes_;
        tzName = tz_name_;
        localTempF = local_temp_f_;
        localTempSource = local_temp_source_;
        llmActive = isLlmActive_();
        evalNeeded = isLlmEvaluationNeeded();
        standbyReason = getLlmStandbyReason();
        fetchId = successful_fetches_;
        configGen = llm_config_generation_;
        if (!llmActive && decider_ != nullptr && decider_ != llm_decider_.get()) {
            customDecider = decider_;
        }
    }

    WeatherDecisionInput input{snap, localForecast, localForecastCount, units,
                               windowOpen, windowClose,
                               nowEpoch, tzOffset, tzName,
                               localTempF, localTempSource};

    if (llmActive && !evalNeeded) {
        // LLM standby: the decision is moot right now (night, auto-open off,
        // door already open, fault, ...). Do NOT spend a model call — keep the
        // rule-based decision current from the same snapshot so the status page
        // and door gate stay consistent. Deliberately leave every LLM health
        // field untouched: standby is not a failure and must not look stale.
        WeatherDecision decision = default_decider_.decide(input);
        {
            ScopedSharedLock lock(hal_);
            if (lock.ok() && configGen == llm_config_generation_) {
                decision_open_ = decision.open;
                decision_reason_ = decision.reason;
                last_decision_decider_ = "rules";
            }
        }
        logger.logfDebug("WeatherManager: LLM in standby (%s); using rule-based decision",
                         standbyReason);
        return;
    }

    if (llmActive && llm_decider_ != nullptr) {
        // Blocking LLM HTTP POST runs completely unlocked on loopTask.
        // llm_decider_ is owned exclusively by loopTask so it cannot be freed
        // mid-call even if /update_settings arrives on async_tcp.
        WeatherDecision decision = llm_decider_->decide(input);
        bool llmOk = llm_decider_->lastCallSucceeded();
        String llmErr = llmOk ? String("") : llm_decider_->getLastError();
        {
            ScopedSharedLock lock(hal_);
            if (!lock.ok()) return;
            // If LLM config changed while this request was in flight, discard
            // the stale result and leave llm_attempted_fetch_id_ untouched so
            // update() evaluates with the new config on the next tick.
            if (configGen != llm_config_generation_) return;

            decision_open_ = decision.open;
            decision_reason_ = decision.reason;
            last_decision_decider_ = llmOk ? "llm" : "rules";

            // Mark THIS snapshot as attempted whether or not the model answered,
            // so update() retries on the next weather fetch instead of every tick.
            llm_attempted_fetch_id_ = fetchId;
            if (llmOk) {
                llm_last_attempt_ok_ = true;
                llm_ever_succeeded_ = true;
                llm_last_success_ms_ = hal_ ? hal_->millis() : 0;
                llm_last_success_epoch_ = currentWallEpoch_();
                llm_last_error_ = "";
            } else {
                llm_last_attempt_ok_ = false;
                llm_last_error_ = llmErr.length() > 0 ? llmErr : String("No usable response from LLM provider");
            }
        }
        return;
    }

    WeatherDecision decision = (customDecider != nullptr)
                                   ? customDecider->decide(input)
                                   : default_decider_.decide(input);
    const char* producer = (customDecider != nullptr) ? customDecider->name() : "rules";
    {
        ScopedSharedLock lock(hal_);
        if (lock.ok() && configGen == llm_config_generation_) {
            decision_open_ = decision.open;
            decision_reason_ = decision.reason;
            last_decision_decider_ = producer;
        }
    }
}

// Derive the current DST-aware UTC offset from the system clock and synthesize
// a short tz label. Compares HAL's local struct tm against gmtime_r(now_epoch_)
// directly rather than via mktime() (mktime converts local tm back to UTC, so
// mktime(&localTm) - now_epoch_ cancels to 0 whenever TZ is configured).
// Best-effort: on any failure the fields stay 0/"" and the prompt falls back
// to "UTC".
void WeatherManager::refreshLocalTime_() {
    now_epoch_ = -1;
    tz_offset_minutes_ = 0;
    tz_name_ = "";
    if (hal_ == nullptr) return;

    unsigned long rawNow = hal_->getTime();
    now_epoch_ = static_cast<long>(rawNow);
    if (rawNow < MIN_VALID_EPOCH) return;

    struct tm localTm = {};
    if (!hal_->getLocalTime(&localTm, 0)) return;

    time_t nowUtc = static_cast<time_t>(rawNow);
    struct tm utcTm = {};
#if defined(_WIN32)
    if (gmtime_s(&utcTm, &nowUtc) != 0) return;
#else
    if (gmtime_r(&nowUtc, &utcTm) == nullptr) return;
#endif

    int dayDelta = localTm.tm_yday - utcTm.tm_yday;
    if (localTm.tm_year > utcTm.tm_year) {
        dayDelta = 1;
    } else if (localTm.tm_year < utcTm.tm_year) {
        dayDelta = -1;
    }
    int diffMinutes = dayDelta * 1440 +
                      (localTm.tm_hour * 60 + localTm.tm_min) -
                      (utcTm.tm_hour * 60 + utcTm.tm_min);
    // Sanity bound: valid civil UTC offsets span [-12h, +14h].
    if (diffMinutes < -720 || diffMinutes > 840) return;
    tz_offset_minutes_ = diffMinutes;

    int h = tz_offset_minutes_ / 60;
    int m = tz_offset_minutes_ % 60;
    char buf[12];
    if (m == 0) {
        snprintf(buf, sizeof(buf), "UTC%+d", h);
    } else {
        snprintf(buf, sizeof(buf), "UTC%+d:%02d", h, m < 0 ? -m : m);
    }
    tz_name_ = String(buf);
}

bool WeatherManager::parseCurrentJsonTo(const String& body, const String& units,
                                        WeatherSnapshot& outSnap,
                                        String& outError) const {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err != DeserializationError::Ok) {
        outError = String("Current JSON parse error: ") + err.c_str();
        logger.logfWarning("WeatherManager: %s", outError.c_str());
        return false;
    }

    // OpenWeatherMap returns cod=200 on success; anything else is an API error
    // (e.g. 401 invalid key, 429 rate limited). "cod" can be int or string.
    int cod = 0;
    if (doc["cod"].is<int>()) cod = doc["cod"].as<int>();
    else if (doc["cod"].is<const char*>()) cod = atoi(doc["cod"].as<const char*>());
    if (cod != 200) {
        outError = doc["message"].is<const char*>() ? doc["message"].as<String>()
                                                    : String("API error code ") + cod;
        logger.logfWarning("WeatherManager: API error: %s", outError.c_str());
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
                                                       snap.wind_speed, snap.temp, units);
    snap.valid = true;

    outSnap = snap;
    return true;
}

bool WeatherManager::parseForecastJsonTo(const String& body, WeatherForecastEntry* outEntries,
                                         size_t maxEntries, size_t& outCount) const {
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
        if (count >= maxEntries) break;
        WeatherForecastEntry& fe = outEntries[count];
        fe = WeatherForecastEntry{};
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
    outCount = count;
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
        // Extreme heat gate. Chickens can die from heat stress above ~105°F
        // ambient in direct sun; the coop is shaded and the yard offers shade,
        // so we only block at genuinely dangerous heat. 108°F catches lethal
        // desert-southwest afternoons without false-blocking normal hot days.
        if (tempF >= 108.0f) {
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

    // Block if a forecast block that STARTS inside the open window shows likely
    // precipitation. This prevents opening during a brief clearing right before
    // rain arrives, which is exactly the case the door should avoid — but a
    // high pop on a block that begins after the door has already closed for
    // the night is irrelevant (the chickens are locked in).
    if (input.forecast_count > 0) {
        const int closeMin = input.window_close_minutes;  // local min, -1 unknown
        for (size_t i = 0; i < input.forecast_count; i++) {
            const WeatherForecastEntry& fe = input.forecast[i];
            if (fe.precip_prob < 0.5f) continue;
            // When we don't know the window close, fall back to the original
            // behavior of checking only the imminent block.
            if (closeMin < 0 || fe.dt <= 0) {
                if (i == 0) return {false, "rain likely soon"};
                continue;
            }
            // Convert the block's start epoch to local minutes-of-day using the
            // DST-aware offset, then compare against the close time.
            long local = fe.dt + static_cast<long>(input.tz_offset_minutes) * 60L;
            int blockStartMin = static_cast<int>(((local % 86400L) + 86400L) % 86400L / 60L);
            if (blockStartMin < closeMin) {
                return {false, "rain likely during open window"};
            }
        }
    }

    return {true, "clear"};
}

unsigned long WeatherManager::getStaleThresholdMs() const {
    unsigned long intervalBased = static_cast<unsigned long>(update_interval_minutes_) * 3UL * 60UL * 1000UL;
    return intervalBased > WEATHER_MIN_STALE_MS ? intervalBased : WEATHER_MIN_STALE_MS;
}

unsigned long WeatherManager::getWeatherAgeSeconds() const {
    if (!current_.valid || hal_ == nullptr) return 0;
    return (hal_->millis() - last_successful_fetch_ms_) / 1000UL;
}

bool WeatherManager::isWeatherStale() const {
    if (!current_.valid || hal_ == nullptr) return false;
    return (hal_->millis() - last_successful_fetch_ms_) >= getStaleThresholdMs();
}

bool WeatherManager::isForecastExpired() const {
    if (forecast_expired_) return true;
    if (forecast_count_ == 0) return false;
    if (isWeatherStale()) return true;
    unsigned long nowEpoch = (hal_ != nullptr) ? hal_->getTime() : 0;
    if (nowEpoch < MIN_VALID_EPOCH) return false;
    for (size_t i = 0; i < forecast_count_; i++) {
        if (!isForecastEntryExpired(forecast_[i], nowEpoch)) {
            return false;
        }
    }
    return true;
}

bool WeatherManager::isLlmStale() const {
    if (!isLlmActive_()) return false;
    // Standby is intentional (outside the actionable window), not a failure:
    // an idle LLM must never present as broken/stale in the UI.
    if (!isLlmEvaluationNeeded()) return false;
    // Before the first weather snapshot arrives, or while an on-demand LLM
    // evaluation is queued for the current snapshot (morning lead-window entry,
    // door closing mid-window, or fresh settings save), do not flag as stale.
    if (!current_.valid) return false;
    if (isLlmDecisionPending()) return false;
    if (!llm_last_attempt_ok_) return true;
    if (!llm_ever_succeeded_ || hal_ == nullptr) return true;
    return (hal_->millis() - llm_last_success_ms_) >= getStaleThresholdMs();
}

bool WeatherManager::isWeatherGateActive() const {
    return enabled_ && api_key_.length() > 0 && current_.valid && !isWeatherStale();
}

bool WeatherManager::isWeatherGoodForOpening() const {
    // When the gate isn't active (disabled, unconfigured, no data yet, or
    // cached data expired past its staleness TTL), do not block the door —
    // fall back to schedule-only behavior. This ensures a weather API outage
    // never traps the chickens inside. Otherwise return the cached decision
    // the active decider computed at the last successful fetch.
    if (!isWeatherGateActive()) return true;
    return decision_open_;
}

void WeatherManager::toJson(JsonObject& json) const {
    bool stale = isWeatherStale();
    bool llmActive = isLlmActive_();
    bool llmStandby = isLlmStandby();
    bool llmPending = isLlmDecisionPending();
    bool llmStale = isLlmStale();
    unsigned long nowEpoch = (hal_ != nullptr) ? hal_->getTime() : 0;

    json["enabled"] = enabled_;
    json["configured"] = (api_key_.length() > 0);
    json["units"] = units_;
    json["gate_active"] = isWeatherGateActive();
    json["good_for_opening"] = isWeatherGoodForOpening();
    json["stale"] = stale;
    // Report the engine that actually produced the cached decision once we have
    // a snapshot, avoiding misattribution across Active <-> Standby transitions.
    json["decider"] = current_.valid
                          ? last_decision_decider_
                          : ((llmActive && llmStandby) ? "rules" : getDeciderName());
    if (decision_reason_.length() > 0) json["decision_reason"] = decision_reason_;
    json["update_interval_minutes"] = update_interval_minutes_;
    json["successful_fetches"] = successful_fetches_;
    json["failed_fetches"] = failed_fetches_;
    json["consecutive_failures"] = consecutive_failures_;
    if (current_.valid) {
        json["data_age_seconds"] = getWeatherAgeSeconds();
    }
    if (last_weather_success_epoch_ > 0) {
        json["last_weather_success_epoch"] = last_weather_success_epoch_;
    }
    if (last_forecast_success_epoch_ > 0) {
        json["last_forecast_success_epoch"] = last_forecast_success_epoch_;
    }
    if (stale) {
        json["stale_threshold_seconds"] = getStaleThresholdMs() / 1000UL;
    }
    if (last_error_.length() > 0) json["last_error"] = last_error_;
    if (forecast_error_.length() > 0) json["forecast_error"] = forecast_error_;

    json["llm_enabled"] = llmActive;
    if (llmActive) {
        json["llm_standby"] = llmStandby;
        if (llmStandby) {
            json["llm_standby_reason"] = getLlmStandbyReason();
        }
        json["llm_pending"] = llmPending;
        // Standby / pending are normal states, not failures.
        json["llm_ok"] = (llmStandby || llmPending) ? true : (llm_last_attempt_ok_ && !llmStale);
        json["llm_stale"] = llmStale;
        json["llm_last_success_epoch"] = llm_last_success_epoch_;
        if (!llmStandby && !llmPending && llm_last_error_.length() > 0) {
            json["llm_last_error"] = llm_last_error_;
        }
    }

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

    // Only serialize non-expired forecast blocks; flag forecast_expired when
    // cached blocks have passed or weather went stale.
    size_t emittedForecast = 0;
    if (!stale && forecast_count_ > 0) {
        for (size_t i = 0; i < forecast_count_; i++) {
            if (!isForecastEntryExpired(forecast_[i], nowEpoch)) {
                emittedForecast++;
            }
        }
        if (emittedForecast > 0) {
            JsonArray fc = json["forecast"].to<JsonArray>();
            for (size_t i = 0; i < forecast_count_; i++) {
                if (isForecastEntryExpired(forecast_[i], nowEpoch)) continue;
                JsonObject fe = fc.add<JsonObject>();
                fe["dt"] = forecast_[i].dt;
                if (!isnan(forecast_[i].temp)) fe["temp"] = forecast_[i].temp;
                if (forecast_[i].wind_speed >= 0) fe["wind_speed"] = forecast_[i].wind_speed;
                if (forecast_[i].precip_prob >= 0) fe["precip_prob"] = forecast_[i].precip_prob;
                fe["description"] = forecast_[i].main_description;
            }
        }
    }
    json["forecast_expired"] = isForecastExpired() || (forecast_count_ > 0 && emittedForecast == 0);
}

void WeatherManager::copyTruncated(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0) return;
    if (src == nullptr) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}
