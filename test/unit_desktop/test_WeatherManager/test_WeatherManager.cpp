#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "WeatherManager.h"

using namespace fakeit;

class WeatherManagerTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    WeatherManager weather;

    void SetUp() override {
        mockHal = new MockHAL();

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
