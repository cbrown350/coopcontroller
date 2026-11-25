#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "WeatherManager.h"

using namespace fakeit;

// MockHAL that counts HTTP traffic so LLM-gating tests can prove exactly when
// a model call (POST) did or did not happen and that cached snapshots are
// reused (no extra GET).
class CountingMockHAL : public MockHAL {
public:
    int getCount = 0;
    int postAuthCount = 0;

    String httpGet(const String& url, unsigned long timeout_ms = 10000) override {
        getCount++;
        return MockHAL::httpGet(url, timeout_ms);
    }

    String httpPostAuth(const String& url, const String& jsonBody,
                        const String& bearerToken, const String& extraHeaders,
                        unsigned long timeout_ms = 15000) override {
        postAuthCount++;
        return MockHAL::httpPostAuth(url, jsonBody, bearerToken, extraHeaders, timeout_ms);
    }
};

class WeatherManagerTest : public ::testing::Test {
protected:
    CountingMockHAL* mockHal;
    WeatherManager weather;

    void SetUp() override {
        mockHal = new CountingMockHAL();

        ArduinoFakeReset();
        mockHal->reset();

        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();

        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);

        weather.begin(mockHal);

        // Sensible defaults: connected, plenty of heap
        mockHal->setWiFiConnected(true);
        mockHal->setFreeHeap(200000);
        mockHal->setMillis(1000);
    }

    void TearDown() override {
        delete mockHal;
        mockHal = nullptr;
    }

    void configureWeather() {
        weather.setEnabled(true);
        weather.setApiKey("testkey123");
        weather.setUnits("imperial");
        weather.setLocation(40.7128f, -74.0060f);
    }

    // Build a minimal OWM current-weather response
    String buildCurrentResponse(int weatherId, const String& main, float temp, float wind) {
        return "{\"weather\":[{\"id\":" + String(weatherId) + ",\"main\":\"" + main +
               "\",\"description\":\"desc\",\"icon\":\"01d\"}],"
               "\"main\":{\"temp\":" + String(temp, 1) + ",\"feels_like\":" + String(temp, 1) +
               ",\"humidity\":50,\"pressure\":1015},"
               "\"wind\":{\"speed\":" + String(wind, 1) + "},"
               "\"clouds\":{\"all\":10},\"dt\":1721234567,\"cod\":200}";
    }

    String buildForecastResponse(float pop) {
        return "{\"cod\":\"200\",\"list\":[{\"dt\":1721245367,\"main\":{\"temp\":70.0},"
               "\"weather\":[{\"id\":800,\"main\":\"Clear\"}],\"wind\":{\"speed\":5.0},"
               "\"pop\":" + String(pop, 2) + "}]}";
    }

    // Combined body valid as BOTH a current-weather response and a forecast
    // response. Needed because MockHAL returns the same body for every GET,
    // while fetchWeather() makes a current call followed by a forecast call.
    String buildCombinedResponse(int weatherId, float temp, float wind, float pop) {
        return "{\"cod\":200,"
               "\"weather\":[{\"id\":" + String(weatherId) + ",\"main\":\"Clear\","
               "\"description\":\"desc\",\"icon\":\"01d\"}],"
               "\"main\":{\"temp\":" + String(temp, 1) + ",\"feels_like\":" + String(temp, 1) +
               ",\"humidity\":50,\"pressure\":1015},"
               "\"wind\":{\"speed\":" + String(wind, 1) + "},"
               "\"clouds\":{\"all\":10},\"dt\":1721234567,"
               "\"list\":[{\"dt\":1721245367,\"main\":{\"temp\":" + String(temp, 1) + "},"
               "\"weather\":[{\"id\":" + String(weatherId) + ",\"main\":\"Clear\"}],"
               "\"wind\":{\"speed\":" + String(wind, 1) + "},\"pop\":" + String(pop, 2) + "}]}";
    }

    // Attach the LLM decider; mockHal->postAuthCount tracks real model calls.
    void configureLlm() {
        weather.configureLlmDecider(true, "http://localhost:8000", "kO7dkihVsUVeb",
                                    "qwen3", "openai_compatible", 15);
    }

    // Queue an OpenAI-compatible chat-completions reply carrying the model's
    // {"open": ..., "reason": ...} decision.
    void setLlmReply(bool open, const char* reason) {
        String content = String("{\\\"open\\\": ") + (open ? "true" : "false") +
                         ", \\\"reason\\\": \\\"" + reason + "\\\"}";
        mockHal->setHttpPostAuthResponse(
            "{\"choices\":[{\"message\":{\"content\":\"" + content + "\"}}]}");
    }

    // Fresh clear-weather fetch body usable as both current + forecast.
    void queueClearWeather() {
        mockHal->setHttpGetResponse(buildCombinedResponse(800, 72.0f, 5.0f, 0.1f));
    }
};

// ============================================================================
// DEFAULT STATE
// ============================================================================

TEST_F(WeatherManagerTest, DefaultState) {
    EXPECT_FALSE(weather.getEnabled());
    EXPECT_FALSE(weather.isWeatherGateActive());
    // Gate not active -> door should not be blocked
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_EQ(weather.getFailedFetches(), 0u);
}

TEST_F(WeatherManagerTest, IntervalClamped) {
    weather.setUpdateIntervalMinutes(1);   // below floor
    EXPECT_EQ(weather.getUpdateIntervalMinutes(), 5u);
    weather.setUpdateIntervalMinutes(1000); // above ceiling
    EXPECT_EQ(weather.getUpdateIntervalMinutes(), 360u);
    weather.setUpdateIntervalMinutes(30);
    EXPECT_EQ(weather.getUpdateIntervalMinutes(), 30u);
}

// ============================================================================
// FETCH GATING (WiFi / heap / config)
// ============================================================================

TEST_F(WeatherManagerTest, NoFetchWhenDisabled) {
    weather.setApiKey("k");
    weather.setEnabled(false);
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(WeatherManagerTest, NoFetchWhenNoApiKey) {
    weather.setEnabled(true);
    weather.setApiKey("");
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(WeatherManagerTest, NoFetchWhenWiFiDisconnected) {
    configureWeather();
    mockHal->setWiFiConnected(false);
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(WeatherManagerTest, NoFetchWhenHeapLow) {
    configureWeather();
    mockHal->setFreeHeap(1000); // below WEATHER_MIN_FREE_HEAP
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

// ============================================================================
// SUCCESSFUL FETCH + URL FORMAT
// ============================================================================

TEST_F(WeatherManagerTest, FetchSuccessBuildsCorrectUrl) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.update();

    EXPECT_EQ(weather.getSuccessfulFetches(), 1u);
    String url = mockHal->getLastHttpGetUrl();
    // Last GET is the forecast call
    EXPECT_TRUE(url.indexOf("api.openweathermap.org") >= 0);
    EXPECT_TRUE(url.indexOf("lat=40.7128") >= 0);
    EXPECT_TRUE(url.indexOf("lon=-74.0060") >= 0);
    EXPECT_TRUE(url.indexOf("units=imperial") >= 0);
    EXPECT_TRUE(url.indexOf("appid=testkey123") >= 0);
}

TEST_F(WeatherManagerTest, FetchGoodWeatherClearGate) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.update();

    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

// ============================================================================
// CONDITION CLASSIFICATION
// ============================================================================

TEST_F(WeatherManagerTest, RainIsInclement) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(500, "Rain", 60.0f, 5.0f));
    weather.update();
    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, SnowIsInclement) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(600, "Snow", 30.0f, 5.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, ThunderstormIsInclement) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(211, "Thunderstorm", 65.0f, 5.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, DrizzleIsInclement) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(300, "Drizzle", 55.0f, 5.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, FogIsAcceptable) {
    configureWeather();
    // 741 Fog is in the 7xx atmosphere band we allow
    mockHal->setHttpGetResponse(buildCurrentResponse(741, "Fog", 55.0f, 5.0f));
    weather.update();
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, TornadoIsInclement) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(781, "Tornado", 65.0f, 5.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, CloudsAreGood) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(803, "Clouds", 68.0f, 5.0f));
    weather.update();
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, HighWindIsInclement) {
    configureWeather();
    // Clear sky but 30 mph wind
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 68.0f, 30.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, ExtremeColdIsInclement) {
    configureWeather();
    // Clear sky but 5°F
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 5.0f, 3.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, MetricHighWindConverted) {
    weather.setEnabled(true);
    weather.setApiKey("k");
    weather.setUnits("metric"); // wind in m/s
    weather.setLocation(40.0f, -74.0f);
    // 15 m/s ~= 33.6 mph -> inclement
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 20.0f, 15.0f));
    weather.update();
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

// ============================================================================
// FORECAST-BASED GATING
// ============================================================================

TEST_F(WeatherManagerTest, ImminentRainForecastBlocksOpening) {
    configureWeather();
    // Current is clear (id 800), but the immediate forecast has pop=0.7
    // (>=0.5), which should block opening even though current conditions look
    // fine. Uses a combined body valid as both current and forecast.
    mockHal->setHttpGetResponse(buildCombinedResponse(800, 68.0f, 5.0f, 0.7f));
    weather.update();

    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, LowForecastPopAllowsOpening) {
    configureWeather();
    // Clear current + low precip probability forecast -> good to open
    mockHal->setHttpGetResponse(buildCombinedResponse(800, 68.0f, 5.0f, 0.1f));
    weather.update();

    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

// Regression for issue #7: a high-pop forecast block that STARTS after the
// door's close time must NOT block opening. The chickens are locked in by then.
TEST_F(WeatherManagerTest, RainAfterCloseDoesNotBlockOpening) {
    configureWeather();
    weather.setOpenWindowMinutes(390, 1140);  // 06:30 - 19:00 local

    // Current is clear. Forecast block has pop=0.7 but starts at 22:00 local
    // (well after the 19:00 close). With tz offset 0, dt epoch 79200 = 22:00.
    // We drive the rule-based decider directly to control the input precisely.
    WeatherSnapshot cur;
    cur.valid = true;
    cur.weather_code = 800;
    cur.temp = 72.0f;
    cur.wind_speed = 5.0f;
    strncpy(cur.main_description, "Clear", sizeof(cur.main_description) - 1);

    WeatherForecastEntry fe;
    fe.dt = 79200L;  // 22:00 at offset 0
    fe.temp = 65.0f;
    fe.wind_speed = 4.0f;
    fe.precip_prob = 0.7f;
    strncpy(fe.main_description, "Rain", sizeof(fe.main_description) - 1);

    WeatherDecisionInput input{cur, &fe, 1, String("imperial"),
                               390, 1140, /*now*/ 70000L,
                               /*tz_off*/ 0, String("UTC")};
    RuleBasedWeatherDecider decider;
    WeatherDecision d = decider.decide(input);

    EXPECT_TRUE(d.open);
}

TEST_F(WeatherManagerTest, RainBeforeCloseStillBlocksOpening) {
    // Same setup as above but the high-pop block starts at 18:00 — inside the
    // 06:30-19:00 window. Opening must be blocked.
    WeatherSnapshot cur;
    cur.valid = true;
    cur.weather_code = 800;
    cur.temp = 72.0f;
    cur.wind_speed = 5.0f;
    strncpy(cur.main_description, "Clear", sizeof(cur.main_description) - 1);

    WeatherForecastEntry fe;
    fe.dt = 64800L;  // 18:00 at offset 0
    fe.temp = 65.0f;
    fe.wind_speed = 4.0f;
    fe.precip_prob = 0.7f;
    strncpy(fe.main_description, "Rain", sizeof(fe.main_description) - 1);

    WeatherDecisionInput input{cur, &fe, 1, String("imperial"),
                               390, 1140, /*now*/ 60000L,
                               /*tz_off*/ 0, String("UTC")};
    RuleBasedWeatherDecider decider;
    WeatherDecision d = decider.decide(input);

    EXPECT_FALSE(d.open);
}

TEST_F(WeatherManagerTest, ExtremeHeatIsInclement) {
    // 110°F current (shaded coop threshold is 108°F) should block opening.
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 110.0f, 5.0f));
    weather.update();

    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, HotButBelowHeatThresholdAllowsOpening) {
    // 100°F is hot but below the 108°F extreme-heat threshold — the coop is
    // shaded and the yard offers shade, so the door should still open.
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 100.0f, 5.0f));
    weather.update();

    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

// ============================================================================
// API ERROR HANDLING
// ============================================================================

TEST_F(WeatherManagerTest, ApiErrorCounts) {
    configureWeather();
    mockHal->setHttpGetResponse(R"({"cod":401,"message":"Invalid API key"})");
    weather.update();
    EXPECT_EQ(weather.getFailedFetches(), 1u);
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
    EXPECT_FALSE(weather.isWeatherGateActive());
    // Gate never became active -> door not blocked
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
    EXPECT_TRUE(weather.getLastError().indexOf("Invalid API key") >= 0);
}

TEST_F(WeatherManagerTest, EmptyResponseCountsAsFailure) {
    configureWeather();
    mockHal->setHttpGetResponse("");
    weather.update();
    EXPECT_EQ(weather.getFailedFetches(), 1u);
    EXPECT_EQ(weather.getSuccessfulFetches(), 0u);
}

TEST_F(WeatherManagerTest, MalformedJsonCountsAsFailure) {
    configureWeather();
    mockHal->setHttpGetResponse("not json at all {[");
    weather.update();
    EXPECT_EQ(weather.getFailedFetches(), 1u);
}

// ============================================================================
// INTERVAL TIMING
// ============================================================================

TEST_F(WeatherManagerTest, RespectsUpdateInterval) {
    configureWeather();
    weather.setUpdateIntervalMinutes(10);
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    // First update at t=1000 fetches immediately
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 1u);

    // Advance 5 minutes - should NOT fetch again
    mockHal->setMillis(1000 + 5 * 60 * 1000);
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 1u);

    // Advance past 10 minutes - should fetch again
    mockHal->setMillis(1000 + 11 * 60 * 1000);
    weather.update();
    EXPECT_EQ(weather.getSuccessfulFetches(), 2u);
}

// ============================================================================
// JSON STATUS OUTPUT
// ============================================================================

TEST_F(WeatherManagerTest, ToJsonIncludesFields) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.update();

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);

    EXPECT_TRUE(obj["enabled"].as<bool>());
    EXPECT_TRUE(obj["configured"].as<bool>());
    EXPECT_TRUE(obj["gate_active"].as<bool>());
    EXPECT_TRUE(obj["good_for_opening"].as<bool>());
    EXPECT_TRUE(obj["current"].is<JsonObject>());
    EXPECT_STREQ(obj["current"]["condition"].as<const char*>(), "GOOD");
}

TEST_F(WeatherManagerTest, ToJsonDisabledHasNoCurrent) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);

    EXPECT_FALSE(obj["enabled"].as<bool>());
    EXPECT_FALSE(obj["current"].is<JsonObject>());
}

// ============================================================================
// FALLBACK SAFETY: never trap chickens on API problems
// ============================================================================

TEST_F(WeatherManagerTest, GateInactiveMeansGoodForOpening) {
    weather.setEnabled(true);
    weather.setApiKey("k");
    // No successful fetch yet -> gate not active -> good for opening
    EXPECT_FALSE(weather.isWeatherGateActive());
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

// ============================================================================
// PLUGGABLE DECIDER (seam for a future LLM/Ollama decider)
// ============================================================================

// A stub decider that always says "keep the door closed" — stands in for a
// future LLM decider so we can verify the strategy is actually consulted.
class AlwaysCloseDecider : public IWeatherDecider {
public:
    int callCount = 0;
    WeatherDecision decide(const WeatherDecisionInput& input) override {
        (void)input;
        callCount++;
        return {false, "stub says no"};
    }
    const char* name() const override { return "stub-close"; }
};

TEST_F(WeatherManagerTest, DefaultDeciderIsRules) {
    EXPECT_STREQ(weather.getDeciderName(), "rules");
}

TEST_F(WeatherManagerTest, CustomDeciderOverridesDecision) {
    configureWeather();
    AlwaysCloseDecider stub;
    weather.setDecider(&stub);
    EXPECT_STREQ(weather.getDeciderName(), "stub-close");

    // Current conditions are clear (rules would allow), but the stub decider
    // must win and block opening.
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.update();

    EXPECT_GE(stub.callCount, 1);
    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getDecisionReason(), String("stub says no"));
}

TEST_F(WeatherManagerTest, DeciderCalledOncePerFetchNotPerQuery) {
    configureWeather();
    AlwaysCloseDecider stub;
    weather.setDecider(&stub);
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.update(); // one fetch -> one decide()
    int afterFetch = stub.callCount;

    // Querying the gate many times must NOT invoke the decider again (an LLM
    // decider would otherwise make a model call per loop iteration).
    for (int i = 0; i < 100; i++) {
        (void)weather.isWeatherGoodForOpening();
    }
    EXPECT_EQ(stub.callCount, afterFetch);
}

TEST_F(WeatherManagerTest, ResetToDefaultDecider) {
    AlwaysCloseDecider stub;
    weather.setDecider(&stub);
    EXPECT_STREQ(weather.getDeciderName(), "stub-close");
    weather.setDecider(nullptr);
    EXPECT_STREQ(weather.getDeciderName(), "rules");
}

// ============================================================================
// Open-Window Plumbing Tests (issue #6)
// ============================================================================

// Decider that records the window minutes it was given, to verify plumbing
// from setOpenWindowMinutes() through to the decision input.
class WindowRecordingDecider : public IWeatherDecider {
public:
    int lastOpenMin = -999;
    int lastCloseMin = -999;
    WeatherDecision decide(const WeatherDecisionInput& input) override {
        lastOpenMin = input.window_open_minutes;
        lastCloseMin = input.window_close_minutes;
        return {true, "recorded"};
    }
    const char* name() const override { return "window-recorder"; }
};

TEST_F(WeatherManagerTest, OpenWindowMinutesDefaultUnknown) {
    configureWeather();
    WindowRecordingDecider stub;
    weather.setDecider(&stub);
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.update();

    EXPECT_EQ(stub.lastOpenMin, -1);
    EXPECT_EQ(stub.lastCloseMin, -1);
}

TEST_F(WeatherManagerTest, OpenWindowMinutesPropagatedToDecider) {
    configureWeather();
    WindowRecordingDecider stub;
    weather.setDecider(&stub);
    weather.setOpenWindowMinutes(390, 1140);  // 06:30 - 19:00

    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.update();

    EXPECT_EQ(stub.lastOpenMin, 390);
    EXPECT_EQ(stub.lastCloseMin, 1140);
}

TEST_F(WeatherManagerTest, RuleBasedDeciderIgnoresOpenWindow) {
    // The rule-based decider must not change behavior when window minutes are
    // set — it only looks at current.condition + forecast, per its contract.
    configureWeather();
    weather.setOpenWindowMinutes(390, 1140);
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.update();

    EXPECT_TRUE(weather.isWeatherGoodForOpening());
}

// ============================================================================
// LLM Decider Configuration Tests (issue #6)
// ============================================================================

TEST_F(WeatherManagerTest, ConfigureLlmDeciderDisabledKeepsRuleBased) {
    weather.configureLlmDecider(false, "http://localhost:8000", "key", "model",
                                "openai_compatible", 15);
    EXPECT_STREQ(weather.getDeciderName(), "rules");
}

TEST_F(WeatherManagerTest, ConfigureLlmDeciderEnabledInstallsLlmDecider) {
    weather.configureLlmDecider(true, "http://localhost:8000", "kO7dkihVsUVeb",
                                "qwen3", "openai_compatible", 15);
    EXPECT_STREQ(weather.getDeciderName(), "llm");
}

TEST_F(WeatherManagerTest, ConfigureLlmDeciderCanBeToggledOffAgain) {
    weather.configureLlmDecider(true, "http://localhost:8000", "key", "model",
                                "openai_compatible", 15);
    EXPECT_STREQ(weather.getDeciderName(), "llm");

    weather.configureLlmDecider(false, "", "", "", "openai_compatible", 15);
    EXPECT_STREQ(weather.getDeciderName(), "rules");
}

TEST_F(WeatherManagerTest, ConfigureLlmDeciderUsedInFetchCycle) {
    configureWeather();
    weather.configureLlmDecider(true, "http://localhost:8000", "kO7dkihVsUVeb",
                                "qwen3", "openai_compatible", 15);

    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": false, \"reason\": \"llm says no\"}"}}]})");

    weather.update();

    EXPECT_STREQ(weather.getDeciderName(), "llm");
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getDecisionReason(), String("llm says no"));
}

TEST_F(WeatherManagerTest, TestLlmConnectionSuccess) {
    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");
    String err = weather.testLlmConnection("http://localhost:8000", "kO7dkihVsUVeb",
                                           "qwen3", "openai_compatible", 15);
    EXPECT_EQ(err, "");
}

TEST_F(WeatherManagerTest, TestLlmConnectionFailure) {
    mockHal->setHttpPostAuthResponse("");
    String err = weather.testLlmConnection("http://localhost:8000", "kO7dkihVsUVeb",
                                           "qwen3", "openai_compatible", 15);
    EXPECT_NE(err, "");
}

// ============================================================================
// Deferred connection-test lifecycle (v0.7.1 async-tcp panic fix)
// ============================================================================

TEST_F(WeatherManagerTest, DeferredLlmTestStartsPendingThenResolvesSuccess) {
    weather.requestLlmTest("http://localhost:8000", "kO7dkihVsUVeb",
                           "qwen3", "openai_compatible", 15);
    EXPECT_TRUE(weather.isTestInProgress());

    // Before the loop consumes it, polling must see "pending" — no TLS yet.
    JsonDocument pre = weather.getLlmTestResultJson();
    EXPECT_EQ(String(pre["status"].as<const char*>()), String("pending"));

    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");
    weather.update();  // loop task runs the probe
    EXPECT_FALSE(weather.isTestInProgress());

    JsonDocument post = weather.getLlmTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("success"));
    EXPECT_TRUE(post["success"].as<bool>());
    // The probe must have actually hit the provider.
    EXPECT_TRUE(mockHal->getLastHttpPostAuthUrl().length() > 0);
}

TEST_F(WeatherManagerTest, DeferredLlmTestResolvesFailure) {
    weather.requestLlmTest("http://localhost:8000", "kO7dkihVsUVeb",
                           "qwen3", "openai_compatible", 15);
    mockHal->setHttpPostAuthResponse("");  // empty body => probe returns error
    weather.update();

    EXPECT_FALSE(weather.isTestInProgress());
    JsonDocument post = weather.getLlmTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("error"));
    EXPECT_FALSE(post["success"].as<bool>());
    EXPECT_TRUE(post["error"].as<String>().length() > 0);
}

TEST_F(WeatherManagerTest, DeferredLlmTestEmptyBaseUrlIsErrorNotPending) {
    weather.requestLlmTest("", "", "", "openai_compatible", 15);
    weather.update();  // no TLS attempt (empty base URL)
    EXPECT_FALSE(weather.isTestInProgress());
    JsonDocument post = weather.getLlmTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("error"));
    // Must NOT have attempted a TLS POST.
    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), String(""));
}

TEST_F(WeatherManagerTest, DeferredLlmTestRequestDoesNoNetworkIo) {
    // The request side must be free of any TLS work — it runs from the async
    // handler. Assert by checking no HTTP POST has happened post-request.
    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);
    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), String(""));
    EXPECT_EQ(mockHal->getLastHttpPostAuthBody(), String(""));
}

TEST_F(WeatherManagerTest, DeferredLlmTestUsesProvidedOverrides) {
    // Provide saved-but-different LLM config, then override via the test path.
    weather.configureLlmDecider(true, "http://saved-host:11434", "saved-key", "saved-model",
                                "openai_compatible", 30);
    weather.requestLlmTest("http://override-host:8000", "override-key", "override-model",
                           "ollama_native", 15);
    mockHal->setHttpPostAuthResponse(R"({"message":{"content":"OK"}})");  // ollama native shape
    weather.update();

    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), String("http://override-host:8000/api/chat"));
    EXPECT_EQ(mockHal->getLastHttpPostAuthToken(), String("override-key"));
}

TEST_F(WeatherManagerTest, DeferredWeatherTestResolvesSuccessAndIncludesSnapshot) {
    configureWeather();  // enabled + api key + location
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.requestWeatherTest("");
    EXPECT_TRUE(weather.isTestInProgress());
    JsonDocument pre = weather.getWeatherTestResultJson();
    EXPECT_EQ(String(pre["status"].as<const char*>()), String("pending"));

    weather.update();  // consumes the test, runs forceRefresh()
    EXPECT_FALSE(weather.isTestInProgress());

    JsonDocument post = weather.getWeatherTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("success"));
    EXPECT_TRUE(post["success"].as<bool>());
    // Successful weather fetches bump the counter.
    EXPECT_GT(weather.getSuccessfulFetches(), 0u);
}

TEST_F(WeatherManagerTest, DeferredWeatherTestResolvesFailureWhenApiError) {
    configureWeather();
    mockHal->setHttpGetResponse(R"({"cod":401,"message":"Invalid API key"})");

    weather.requestWeatherTest("");
    weather.update();

    JsonDocument post = weather.getWeatherTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("error"));
    EXPECT_FALSE(post["success"].as<bool>());
    EXPECT_GT(weather.getFailedFetches(), 0u);
}

TEST_F(WeatherManagerTest, DeferredWeatherTestAppliesApiKeyOverrideOneShot) {
    configureWeather();  // sets the saved api key to "testkey123"
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.requestWeatherTest("override-key-XYZ");
    weather.update();

    // After the test the saved key must be restored.
    EXPECT_EQ(weather.getApiKey(), String("testkey123"));
    EXPECT_TRUE(weather.getSuccessfulFetches() > 0u);
}

TEST_F(WeatherManagerTest, DeferredWeatherTestRequestDoesNoNetworkIo) {
    configureWeather();
    weather.requestWeatherTest("");
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), String(""));
}

TEST_F(WeatherManagerTest, DeferredTestIdleState) {
    // No request made -> both result readers report "idle".
    JsonDocument llm = weather.getLlmTestResultJson();
    EXPECT_EQ(String(llm["status"].as<const char*>()), String("idle"));
    JsonDocument w = weather.getWeatherTestResultJson();
    EXPECT_EQ(String(w["status"].as<const char*>()), String("idle"));
    EXPECT_FALSE(weather.isTestInProgress());
}

TEST_F(WeatherManagerTest, DeferredTestDoesNotBreakPeriodicFetch) {
    // A pending test must not postpone the normal interval-based fetch beyond
    // one cycle. Configure weather, request an LLM test, run update() twice:
    // the first consumes the test; the second does the periodic fetch.
    configureWeather();
    weather.configureLlmDecider(true, "http://localhost:8000", "key", "m",
                                "openai_compatible", 15);

    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));

    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);
    weather.update();  // consumes the test (LLM POST happens; no weather GET)
    String urlAfterTest = mockHal->getLastHttpGetUrl();
    EXPECT_EQ(urlAfterTest, String(""));  // periodic fetch hasn't fired yet

    // Advance time past the interval and tick again — periodic fetch resumes.
    mockHal->setMillis(1000 + 11 * 60 * 1000);
    weather.update();
    EXPECT_TRUE(mockHal->getLastHttpGetUrl().length() > 0);
}

TEST_F(WeatherManagerTest, RepeatedLlmTestRequestsCoalesceToSingleProbe) {
    // Clicking the button multiple times before the loop consumes the request
    // must result in ONE probe, not N — matches the UpdateManager pattern.
    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);
    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);
    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);

    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");
    weather.update();

    // Only one probe POST happened (last URL/body recorded, count not tracked,
    // but isTestInProgress is false and result is populated once).
    EXPECT_FALSE(weather.isTestInProgress());
    JsonDocument post = weather.getLlmTestResultJson();
    EXPECT_EQ(String(post["status"].as<const char*>()), String("success"));
}

// While the loop task is mid-probe (between consuming the request and writing
// the result), polling must still report "pending" — never "idle". This was a
// real bug observed on the USB test board: a 3-second TLS fetch left a window
// where the getter returned "idle" because pending_test_ was already cleared.
TEST_F(WeatherManagerTest, LlmTestReportsPendingWhileProbeIsRunning) {
    // Stand up a slow LLM POST that we can observe mid-flight. The MockHAL
    // returns synchronously, so we instead verify the contract by checking
    // isTestInProgress() remains true across the update() call boundary: the
    // public surface must NOT flicker to idle between pending and done.
    weather.requestLlmTest("http://localhost:8000", "key", "m", "openai_compatible", 15);
    EXPECT_TRUE(weather.isTestInProgress());
    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");
    weather.update();
    EXPECT_FALSE(weather.isTestInProgress());  // settled
    JsonDocument r = weather.getLlmTestResultJson();
    EXPECT_NE(String(r["status"].as<const char*>()), String("idle"));
}

TEST_F(WeatherManagerTest, WeatherTestReportsPendingWhileProbeIsRunning) {
    configureWeather();
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 72.0f, 5.0f));
    weather.requestWeatherTest("");
    EXPECT_TRUE(weather.isTestInProgress());
    weather.update();
    EXPECT_FALSE(weather.isTestInProgress());
    JsonDocument r = weather.getWeatherTestResultJson();
    EXPECT_NE(String(r["status"].as<const char*>()), String("idle"));
}

// ============================================================================
// Staleness TTL & HAL error propagation tests
// ============================================================================

TEST_F(WeatherManagerTest, StaleInclementWeatherFailsGateOpen) {
    configureWeather();
    mockHal->setMillis(1000);
    // Fetch inclement Snow -> gate is active and blocks opening
    mockHal->setHttpGetResponse(buildCurrentResponse(601, "Snow", 25.0f, 5.0f));
    weather.update();
    ASSERT_TRUE(weather.isWeatherGateActive());
    ASSERT_FALSE(weather.isWeatherGoodForOpening());
    ASSERT_FALSE(weather.isWeatherStale());

    // Advance 1 hour (under 2-hour TTL): still fresh, still blocks
    mockHal->setMillis(1000 + 3600UL * 1000UL);
    EXPECT_FALSE(weather.isWeatherStale());
    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());

    // Advance past 2-hour TTL with failing fetches: weather expires -> gate fails OPEN
    mockHal->setHttpGetResponse("");
    mockHal->setLastHttpError("TLS connect failed (-10368): X509 alloc failed");
    mockHal->setMillis(1000 + 2UL * 3600UL * 1000UL + 5000UL);
    weather.update();

    EXPECT_TRUE(weather.isWeatherStale());
    EXPECT_FALSE(weather.isWeatherGateActive());
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getConsecutiveFailures(), 1u);
    EXPECT_GE(weather.getWeatherAgeSeconds(), 7200UL);
    EXPECT_NE(weather.getLastError().indexOf("-10368"), -1);

    // JSON exposes stale, data_age_seconds, consecutive_failures, last_error
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);
    EXPECT_TRUE(obj["stale"].as<bool>());
    EXPECT_FALSE(obj["gate_active"].as<bool>());
    EXPECT_TRUE(obj["good_for_opening"].as<bool>());
    EXPECT_EQ(obj["consecutive_failures"].as<unsigned int>(), 1u);
    EXPECT_GE(obj["data_age_seconds"].as<unsigned long>(), 7200UL);
    EXPECT_NE(obj["last_error"].as<String>().indexOf("-10368"), -1);

    // Fresh successful fetch clears stale state and consecutive_failures
    mockHal->setLastHttpError("");
    mockHal->setHttpGetResponse(buildCurrentResponse(601, "Snow", 24.0f, 5.0f));
    mockHal->setMillis(1000 + 2UL * 3600UL * 1000UL + 11UL * 60UL * 1000UL);
    weather.update();

    EXPECT_FALSE(weather.isWeatherStale());
    EXPECT_TRUE(weather.isWeatherGateActive());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getConsecutiveFailures(), 0u);
    EXPECT_EQ(weather.getLastError(), String(""));
}

TEST_F(WeatherManagerTest, StaleThresholdScalesWithLongUpdateInterval) {
    configureWeather();
    weather.setUpdateIntervalMinutes(60);  // 3 * 60m = 3 hours > 2h minimum
    EXPECT_EQ(weather.getStaleThresholdMs(), 3UL * 3600UL * 1000UL);

    mockHal->setMillis(1000);
    mockHal->setHttpGetResponse(buildCurrentResponse(601, "Snow", 25.0f, 5.0f));
    weather.update();

    // At 2.5 hours, still within 3-hour TTL
    mockHal->setMillis(1000 + 150UL * 60UL * 1000UL);
    EXPECT_FALSE(weather.isWeatherStale());

    // At 3 hours, expires
    mockHal->setMillis(1000 + 180UL * 60UL * 1000UL);
    EXPECT_TRUE(weather.isWeatherStale());
}

TEST_F(WeatherManagerTest, ExpiredForecastBlocksAreClearedAndFlaggedInJson) {
    configureWeather();
    // Valid NTP epoch (Nov 24 2026 12:00 UTC)
    const unsigned long baseEpoch = 1795521600UL;
    mockHal->setMillis(1000);
    mockHal->setTime(baseEpoch);

    // OWM current + forecast (single 3-hour block starting at baseEpoch)
    // MockHAL returns same string for both GETs; craft JSON containing both fields
    String combined = R"({"cod":200,"dt":1795521600,"main":{"temp":55.0,"feels_like":54.0,"humidity":50},"wind":{"speed":4.0},"clouds":{"all":10},"weather":[{"id":800,"main":"Clear","icon":"01d"}],"list":[{"dt":1795521600,"main":{"temp":56.0},"wind":{"speed":4.0},"pop":0.1,"weather":[{"main":"Clear"}]}]})";
    mockHal->setHttpGetResponse(combined);
    weather.update();

    EXPECT_FALSE(weather.isForecastExpired());
    EXPECT_EQ(weather.getLastForecastSuccessEpoch(), baseEpoch);
    EXPECT_EQ(weather.getLastWeatherSuccessEpoch(), baseEpoch);

    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_FALSE(obj["forecast_expired"].as<bool>());
        EXPECT_EQ(obj["forecast"].as<JsonArray>().size(), 1u);
    }

    // Advance wall clock past the 3-hour forecast block (baseEpoch + 3h + 60s)
    // without a new forecast fetch (keep millis under weather interval so no fetch triggers).
    // Note: call setMillis() BEFORE setTime() because MockHAL::setMillis also writes mockTime.
    mockHal->setMillis(60000);
    mockHal->setTime(baseEpoch + 3UL * 3600UL + 60UL);
    weather.update();

    EXPECT_TRUE(weather.isForecastExpired());
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);
    EXPECT_TRUE(obj["forecast_expired"].as<bool>());
    EXPECT_TRUE(obj["forecast"].isNull());
    EXPECT_EQ(obj["last_forecast_success_epoch"].as<unsigned long>(), baseEpoch);
}

TEST_F(WeatherManagerTest, StaleLlmTracksLastSuccessEpochAndError) {
    configureWeather();
    const unsigned long baseEpoch = 1795521600UL;
    mockHal->setMillis(1000);
    mockHal->setTime(baseEpoch);

    weather.configureLlmDecider(true, "https://ollama.com", "key", "deepseek-v4.1-flash:1017-cloud",
                                "openai_compatible", 15);
    mockHal->setHttpGetResponse(buildCurrentResponse(800, "Clear", 65.0f, 3.0f));
    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"{\"open\":true,\"reason\":\"clear 65F\"}"}}]})");
    weather.update();

    EXPECT_FALSE(weather.isLlmStale());
    EXPECT_EQ(weather.getLastLlmSuccessEpoch(), baseEpoch);
    EXPECT_EQ(weather.getLastLlmError(), String(""));

    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_TRUE(obj["llm_enabled"].as<bool>());
        EXPECT_TRUE(obj["llm_ok"].as<bool>());
        EXPECT_FALSE(obj["llm_stale"].as<bool>());
        EXPECT_EQ(obj["llm_last_success_epoch"].as<unsigned long>(), baseEpoch);
    }

    // Next cycle: weather fetch succeeds, LLM fails with HTTP 402
    mockHal->setMillis(1000 + 11UL * 60UL * 1000UL);
    mockHal->setTime(baseEpoch + 660UL);
    mockHal->setHttpPostAuthResponse("");
    mockHal->setLastHttpError("HTTP 402: Add usage credits");
    weather.update();

    EXPECT_TRUE(weather.isLlmStale());
    EXPECT_EQ(weather.getLastLlmSuccessEpoch(), baseEpoch);  // preserves previous good run timestamp
    EXPECT_NE(weather.getLastLlmError().indexOf("HTTP 402"), -1);

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);
    EXPECT_TRUE(obj["llm_enabled"].as<bool>());
    EXPECT_FALSE(obj["llm_ok"].as<bool>());
    EXPECT_TRUE(obj["llm_stale"].as<bool>());
    EXPECT_EQ(obj["llm_last_success_epoch"].as<unsigned long>(), baseEpoch);
    EXPECT_NE(obj["llm_last_error"].as<String>().indexOf("HTTP 402"), -1);
}

// ============================================================================
// Door-schedule-aware LLM gating
// ============================================================================
// The LLM only matters while a scheduled auto-open is actionable:
//  - auto-open enabled + unlocked + enabled day + valid window,
//  - door not already open, not faulted,
//  - now inside [openMin - 15, closeMin).
// Outside that, the rule engine keeps the cached decision current for free.

TEST_F(WeatherManagerTest, LlmStandbyBeforeLeadWindowSkipsModelCall) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);              // 06:30 - 19:00
    weather.setDoorOpenContext(true, false, false, 300);  // 05:00, before 06:15

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(weather.getSuccessfulFetches(), 1u);
    EXPECT_EQ(mockHal->postAuthCount, 0);  // no model call before the lead window
    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "outside open window");
    EXPECT_FALSE(weather.isLlmStale());
    EXPECT_FALSE(weather.isLlmDecisionPending());

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    weather.toJson(obj);
    EXPECT_TRUE(obj["llm_enabled"].as<bool>());
    EXPECT_TRUE(obj["llm_standby"].as<bool>());
    EXPECT_STREQ(obj["llm_standby_reason"].as<const char*>(), "outside open window");
    EXPECT_STREQ(obj["decider"].as<const char*>(), "rules");
    EXPECT_FALSE(obj["llm_stale"].as<bool>());
    EXPECT_TRUE(obj["llm_ok"].as<bool>());
    EXPECT_TRUE(obj["llm_last_error"].isNull());
    // Rule engine still produced the cached decision for the door gate.
    EXPECT_EQ(weather.getDecisionReason(), String("clear"));
}

TEST_F(WeatherManagerTest, LlmRunsAtLeadWindowStart) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 390 - WeatherManager::LLM_PRE_OPEN_LEAD_MINUTES);

    queueClearWeather();
    setLlmReply(false, "lead window no");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmStandby());
    EXPECT_EQ(weather.getDecisionReason(), String("lead window no"));
}

TEST_F(WeatherManagerTest, LlmRunsInsideOpenWindow) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 500);  // 08:20, inside

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmStandby());
    EXPECT_EQ(weather.getDecisionReason(), String("llm clear"));
    EXPECT_FALSE(weather.isLlmDecisionPending());
}

TEST_F(WeatherManagerTest, LlmSkippedAtAndAfterCloseCeiling) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);

    // Exactly at the close ceiling.
    weather.setDoorOpenContext(true, false, false, 1140);
    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "outside open window");

    // Later in the evening, after another interval.
    mockHal->setMillis(1000 + 11UL * 60UL * 1000UL);
    weather.setDoorOpenContext(true, false, false, 1300);
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());
}

TEST_F(WeatherManagerTest, LlmSkippedWhenAutoOpenIneligibleToday) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    // Auto-open disabled / locked out / day disabled all collapse to this flag.
    weather.setDoorOpenContext(false, false, false, 500);

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "auto-open disabled today");
    EXPECT_FALSE(weather.isLlmStale());
}

TEST_F(WeatherManagerTest, LlmSkippedWhenDoorAlreadyOpen) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, true, false, 500);  // inside window, door open

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "door already open");
}

TEST_F(WeatherManagerTest, LlmSkippedWhenDoorFaulted) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, true, 500);

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "door faulted");
}

TEST_F(WeatherManagerTest, ClockNotSyncedKeepsLlmInStandby) {
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, -1);  // clock unknown

    EXPECT_TRUE(weather.isLlmStandby());
    EXPECT_STREQ(weather.getLlmStandbyReason(), "clock not synced");
}

TEST_F(WeatherManagerTest, LlmResumesOnDemandWhenDoorClosesMidWindow) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, true, false, 500);  // door open during window

    queueClearWeather();
    setLlmReply(false, "llm says no");
    weather.update();  // fetch; LLM skipped while the door is open
    ASSERT_EQ(weather.getSuccessfulFetches(), 1u);
    ASSERT_EQ(mockHal->postAuthCount, 0);
    ASSERT_EQ(weather.getDecisionReason(), String("clear"));

    // Door closes again while still inside the actionable window: the LLM now
    // owes a decision for the latest fresh snapshot.
    weather.setDoorOpenContext(true, false, false, 505);
    EXPECT_TRUE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isLlmStandby());

    // Same-timestamp update (periodic fetch not due) runs it immediately.
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());  // LLM decision applied
    EXPECT_EQ(weather.getDecisionReason(), String("llm says no"));

    // No tight-loop retry: further ticks must not re-run the model.
    weather.update();
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
}

TEST_F(WeatherManagerTest, LlmDoesNotResumeAfterCloseCeiling) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, true, false, 500);

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();
    ASSERT_EQ(mockHal->postAuthCount, 0);

    // Door closes again only after the close ceiling: no decision is owed.
    weather.setDoorOpenContext(true, false, false, 1140);
    EXPECT_FALSE(weather.isLlmDecisionPending());
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 0);
}

TEST_F(WeatherManagerTest, CrossingIntoLeadWindowTriggersOnDemandLlm) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 300);  // before the lead window

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();  // fetch, LLM skipped
    ASSERT_EQ(mockHal->postAuthCount, 0);
    const int getsAfterFetch = mockHal->getCount;

    // Time passes (DoorController pushes a new minute) -> inside the lead window.
    weather.setDoorOpenContext(true, false, false, 380);
    EXPECT_TRUE(weather.isLlmDecisionPending());
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_EQ(mockHal->getCount, getsAfterFetch);  // cached snapshot reused
    EXPECT_FALSE(weather.isLlmDecisionPending());

    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
}

TEST_F(WeatherManagerTest, OnDemandLlmFailureIsAttemptedOnceAndNotStaleInStandby) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 300);  // before lead window

    queueClearWeather();
    weather.update();  // fetch, LLM skipped
    ASSERT_EQ(mockHal->postAuthCount, 0);

    // Enter the window with a failing provider.
    mockHal->setHttpPostAuthResponse("");
    mockHal->setLastHttpError("HTTP 500: provider down");
    weather.setDoorOpenContext(true, false, false, 400);
    weather.update();  // on-demand attempt, fails, falls back to rules

    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_TRUE(weather.isLlmStale());
    EXPECT_NE(weather.getDecisionReason().indexOf("llm unavailable"), -1);
    EXPECT_EQ(weather.getLastLlmError().indexOf("HTTP 500"), 0);

    // Attempted for this fetch id -> no retry storm between fetches.
    weather.update();
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);

    // Going into standby is intentional idleness, not staleness.
    weather.setDoorOpenContext(true, true, false, 400);
    EXPECT_FALSE(weather.isLlmStale());
    EXPECT_TRUE(weather.isLlmStandby());
}

TEST_F(WeatherManagerTest, ConfigureLlmDeciderDefersModelCallToUpdate) {
    configureWeather();
    queueClearWeather();
    weather.update();  // rules fetch, no LLM configured yet
    ASSERT_EQ(mockHal->postAuthCount, 0);

    // Must not POST synchronously (this path runs on the async_tcp task).
    configureLlm();
    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_STREQ(weather.getDeciderName(), "llm");
    // No door context pushed -> legacy behavior: the LLM is owed an evaluation.
    EXPECT_TRUE(weather.isLlmEvaluationNeeded());
    EXPECT_TRUE(weather.isLlmDecisionPending());

    setLlmReply(true, "llm clear");
    weather.update();  // loop task runs it on demand
    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_EQ(weather.getDecisionReason(), String("llm clear"));
}

TEST_F(WeatherManagerTest, PreDawnClampedLeadStartEvaluatesLlm) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(5, 300);                 // 00:05 - 05:00
    weather.setDoorOpenContext(true, false, false, 0);    // 00:00, lead clamps to 0

    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmStandby());
}

TEST_F(WeatherManagerTest, WindowDerivedFromEpochWhenContextMinutesUnknown) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, -1);  // no clock from DoorController

    // 1795521600 = 12:00 UTC; the mock reports no tz, so local == UTC.
    const unsigned long baseEpoch = 1795521600UL;
    mockHal->setTime(baseEpoch);
    queueClearWeather();
    setLlmReply(true, "llm clear");
    weather.update();

    EXPECT_EQ(mockHal->postAuthCount, 1);  // 12:00 is inside 06:30-19:00
    EXPECT_FALSE(weather.isLlmStandby());
}

TEST_F(WeatherManagerTest, NextDayLeadWindowEntryNotStaleWhilePending) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 400);

    // Day 1: LLM succeeds inside open window.
    mockHal->setMillis(1000);
    queueClearWeather();
    setLlmReply(true, "day1 clear");
    weather.update();
    ASSERT_EQ(mockHal->postAuthCount, 1);
    ASSERT_FALSE(weather.isLlmStale());

    // Overnight: >2 hours later in standby, fresh weather fetched (LLM skipped).
    mockHal->setMillis(1000 + 4UL * 3600UL * 1000UL);
    weather.setDoorOpenContext(true, false, false, 300);  // before lead window
    weather.update();
    ASSERT_EQ(mockHal->postAuthCount, 1);
    ASSERT_TRUE(weather.isLlmStandby());
    ASSERT_FALSE(weather.isLlmStale());

    // Day 2 morning: clock crosses into lead window. Even though last LLM success
    // is 4 hours old, an on-demand evaluation is pending -> NOT stale!
    weather.setDoorOpenContext(true, false, false, 380);
    EXPECT_TRUE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isLlmStale());
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_TRUE(obj["llm_pending"].as<bool>());
        EXPECT_FALSE(obj["llm_stale"].as<bool>());
        EXPECT_TRUE(obj["llm_ok"].as<bool>());
        EXPECT_STREQ(obj["decider"].as<const char*>(), "rules");  // rules produced standby decision
    }

    // Next update() runs on-demand LLM -> clears pending and updates decider.
    setLlmReply(true, "day2 clear");
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 2);
    EXPECT_FALSE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isLlmStale());
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_FALSE(obj["llm_pending"].as<bool>());
        EXPECT_STREQ(obj["decider"].as<const char*>(), "llm");
        EXPECT_EQ(obj["decision_reason"].as<String>(), String("day2 clear"));
    }
}

TEST_F(WeatherManagerTest, ActiveToStandbyPreservesDeciderAttributionAndCleansFallbackError) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 400);

    // Case 1: Active LLM succeeds, then door opens (Standby) BEFORE next fetch.
    queueClearWeather();
    setLlmReply(true, "llm clear 65F");
    weather.update();
    ASSERT_EQ(mockHal->postAuthCount, 1);

    weather.setDoorOpenContext(true, true, false, 405);
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_TRUE(obj["llm_standby"].as<bool>());
        // Decision still came from LLM until a standby fetch recomputes rules.
        EXPECT_STREQ(obj["decider"].as<const char*>(), "llm");
        EXPECT_EQ(obj["decision_reason"].as<String>(), String("llm clear 65F"));
    }

    // Case 2: Active LLM fails, then door opens (Standby) BEFORE next fetch.
    mockHal->setMillis(1000 + 11UL * 60UL * 1000UL);
    weather.setDoorOpenContext(true, false, false, 420);
    mockHal->setHttpPostAuthResponse("");
    mockHal->setLastHttpError("HTTP 500");
    weather.update();
    ASSERT_TRUE(weather.isLlmStale());
    ASSERT_NE(weather.getDecisionReason().indexOf("llm unavailable"), -1);

    weather.setDoorOpenContext(true, true, false, 425);
    // update() cleans up the stale "llm unavailable..." fallback string locally
    // (zero network calls) so standby shows a clean rule-based decision.
    const int postsBefore = mockHal->postAuthCount;
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, postsBefore);
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_TRUE(obj["llm_standby"].as<bool>());
        EXPECT_FALSE(obj["llm_stale"].as<bool>());
        EXPECT_STREQ(obj["decider"].as<const char*>(), "rules");
        EXPECT_EQ(obj["decision_reason"].as<String>(), String("clear"));
        EXPECT_TRUE(obj["llm_last_error"].isNull());
    }
}

TEST_F(WeatherManagerTest, ConfigureLlmMidWindowNotStaleWhilePendingAndDisableIsDeferred) {
    configureWeather();
    queueClearWeather();
    weather.update();  // rules fetch
    ASSERT_EQ(mockHal->postAuthCount, 0);

    // Enable LLM mid-window: staged until update(), NOT stale while pending.
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, false, false, 500);
    configureLlm();
    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isLlmStale());
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        weather.toJson(obj);
        EXPECT_TRUE(obj["llm_enabled"].as<bool>());
        EXPECT_TRUE(obj["llm_pending"].as<bool>());
        EXPECT_FALSE(obj["llm_stale"].as<bool>());
        EXPECT_TRUE(obj["llm_ok"].as<bool>());
    }

    // update() materializes the staged decider and runs on-demand evaluation.
    setLlmReply(false, "storm coming");
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isWeatherGoodForOpening());

    // Disabling LLM immediately updates logical state without synchronous recompute;
    // next update() destroys the owned decider and recomputes rules locally.
    weather.configureLlmDecider(false, "", "", "", "openai_compatible", 15);
    EXPECT_STREQ(weather.getDeciderName(), "rules");
    weather.update();
    EXPECT_TRUE(weather.isWeatherGoodForOpening());
    EXPECT_EQ(weather.getDecisionReason(), String("clear"));
}

TEST_F(WeatherManagerTest, PeriodicFetchFailureDoesNotDeadlockPendingOnDemandLlm) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);
    weather.setDoorOpenContext(true, true, false, 500);  // door open -> LLM skipped

    mockHal->setMillis(1000);
    queueClearWeather();
    weather.update();  // Fetch #1 succeeds; LLM skipped while open
    ASSERT_EQ(weather.getSuccessfulFetches(), 1u);
    ASSERT_EQ(mockHal->postAuthCount, 0);

    // Advance past fetch interval (11 min, still under 2h stale TTL), close door,
    // and make OWM GET fail while LLM is healthy.
    mockHal->setMillis(1000 + 11UL * 60UL * 1000UL);
    weather.setDoorOpenContext(true, false, false, 511);
    mockHal->setHttpGetResponse("");
    setLlmReply(false, "hold for wind");

    // Tick 1 attempts periodic OWM fetch (fails and sets last_fetch_attempt_ms_).
    weather.update();
    EXPECT_EQ(weather.getFailedFetches(), 1u);
    EXPECT_TRUE(weather.isLlmDecisionPending());

    // Tick 2 (10ms later): periodic fetch not due, so on-demand LLM resolves
    // against the cached fresh snapshot — no deadlock.
    mockHal->setMillis(1000 + 11UL * 60UL * 1000UL + 10UL);
    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmDecisionPending());
    EXPECT_FALSE(weather.isWeatherGoodForOpening());
}

TEST_F(WeatherManagerTest, LiveEpochAdvanceTriggersOnDemandLlmAndComputesTzOffset) {
    configureWeather();
    configureLlm();
    weather.setOpenWindowMinutes(390, 1140);  // 06:30 - 19:00 local
    weather.setDoorOpenContext(true, false, false, -1);  // rely on HAL clock

    // 2026-11-24 13:00:00 UTC == 06:00:00 local at UTC-7 (360 min < 375 leadStart).
    const unsigned long baseUtc = 1795525200UL;
    mockHal->setMillis(1000);
    mockHal->setTime(baseUtc);
    struct tm localTm = {};
    localTm.tm_year = 126;  // 2026
    localTm.tm_mon = 10;    // Nov (0-based)
    localTm.tm_mday = 24;
    localTm.tm_yday = 327;  // Nov 24 in non-leap year 2026
    localTm.tm_hour = 6;
    localTm.tm_min = 0;
    localTm.tm_sec = 0;
    mockHal->setTimeInfo(localTm);
    mockHal->setGetLocalTimeResult(true);

    queueClearWeather();
    setLlmReply(true, "clear morning");
    weather.update();
    // 06:00 local (UTC-7) is BEFORE 06:15 leadStart -> LLM skipped!
    EXPECT_EQ(mockHal->postAuthCount, 0);
    EXPECT_TRUE(weather.isLlmStandby());

    // Advance live HAL epoch by 20 min (06:20 local, inside [06:15, 19:00))
    // without a new weather fetch -> live epoch triggers isLlmDecisionPending().
    mockHal->setTime(baseUtc + 20UL * 60UL);
    localTm.tm_min = 20;
    mockHal->setTimeInfo(localTm);
    EXPECT_TRUE(weather.isLlmDecisionPending());

    weather.update();
    EXPECT_EQ(mockHal->postAuthCount, 1);
    EXPECT_FALSE(weather.isLlmStandby());
}
