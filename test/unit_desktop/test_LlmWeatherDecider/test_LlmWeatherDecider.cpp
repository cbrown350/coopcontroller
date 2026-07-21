#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "MockHAL.h"
#include "Logger.h"
#include "LlmWeatherDecider.h"

using namespace fakeit;

class LlmWeatherDeciderTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    WeatherSnapshot snap;

    void SetUp() override {
        mockHal = new MockHAL();
        ArduinoFakeReset();
        mockHal->reset();

        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);

        mockHal->setFreeHeap(200000);
        mockHal->setMillis(1000);

        snap.valid = true;
        snap.weather_code = 800;
        strncpy(snap.main_description, "Clear", sizeof(snap.main_description) - 1);
        snap.temp = 72.0f;
        snap.feels_like = 71.0f;
        snap.humidity = 40;
        snap.wind_speed = 5.0f;
    }

    void TearDown() override {
        delete mockHal;
        mockHal = nullptr;
    }

    WeatherDecisionInput buildInput(int openMin = -1, int closeMin = -1) {
        return WeatherDecisionInput{snap, nullptr, 0, String("imperial"), openMin, closeMin};
    }

    // Build an input that carries a short forecast + a real local-time anchor
    // (epoch + DST-aware offset) so prompt-building tests exercise the new
    // time-anchoring path. forecastDts/precipProbs must be parallel arrays.
    WeatherDecisionInput buildForecastInput(const std::vector<long>& forecastDts,
                                            const std::vector<float>& precipProbs,
                                            const std::vector<std::string>& descriptions,
                                            long nowEpoch, int tzOffsetMin,
                                            const String& tzName,
                                            int openMin = -1, int closeMin = -1) {
        static std::vector<WeatherForecastEntry> entries;
        entries.clear();
        size_t n = std::min({forecastDts.size(), precipProbs.size(), descriptions.size()});
        for (size_t i = 0; i < n; i++) {
            WeatherForecastEntry fe;
            fe.dt = forecastDts[i];
            fe.precip_prob = precipProbs[i];
            fe.temp = 90.0f;
            fe.wind_speed = 6.0f;
            strncpy(fe.main_description, descriptions[i].c_str(), sizeof(fe.main_description) - 1);
            entries.push_back(fe);
        }
        return WeatherDecisionInput{snap, entries.data(), entries.size(), String("imperial"),
                                    openMin, closeMin, nowEpoch, tzOffsetMin, tzName};
    }
};

TEST_F(LlmWeatherDeciderTest, ValidJsonReplyOpenTrue) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"clear skies\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key123", "gpt-4o-mini",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_TRUE(d.open);
    EXPECT_EQ(d.reason, "clear skies");
    EXPECT_STREQ(decider.name(), "llm");
}

TEST_F(LlmWeatherDeciderTest, ValidJsonReplyOpenFalse) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": false, \"reason\": \"storm incoming\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key123", "gpt-4o-mini",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_FALSE(d.open);
    EXPECT_EQ(d.reason, "storm incoming");
}

TEST_F(LlmWeatherDeciderTest, OllamaNativeWireParsesMessageContent) {
    mockHal->setHttpPostAuthResponse(R"({"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}})");

    LlmWeatherDecider decider(mockHal, "http://192.168.1.5:11434", "", "llama3.1",
                              LlmProviderWire::OLLAMA_NATIVE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_TRUE(d.open);
    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), "http://192.168.1.5:11434/api/chat");
}

TEST_F(LlmWeatherDeciderTest, OpenAiCompatibleWireUsesChatCompletionsPath) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "http://localhost:8000", "kO7dkihVsUVeb", "qwen3",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput());

    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), "http://localhost:8000/v1/chat/completions");
    EXPECT_EQ(mockHal->getLastHttpPostAuthToken(), "kO7dkihVsUVeb");
}

TEST_F(LlmWeatherDeciderTest, MalformedJsonFallsBackToRuleBased) {
    // Not valid JSON at all -> parseReply fails -> falls back to rule-based.
    // Current snapshot is "Clear"/72F/5mph => rule-based says open=true.
    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"I think it's fine outside"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_TRUE(d.open);
    EXPECT_THAT(d.reason.c_str(), testing::HasSubstr("llm unavailable"));
}

TEST_F(LlmWeatherDeciderTest, HttpFailureFallsBackToRuleBased) {
    mockHal->setHttpPostAuthResponse("");  // Empty = HAL httpPostAuth failure

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_TRUE(d.open);  // rule-based: clear/72F/5mph -> open
    EXPECT_THAT(d.reason.c_str(), testing::HasSubstr("llm unavailable"));
}

TEST_F(LlmWeatherDeciderTest, HttpFailureFallsBackAndReflectsInclementRuleResult) {
    mockHal->setHttpPostAuthResponse("");

    // Make current conditions inclement (thunderstorm code 200) so the
    // rule-based fallback should say open=false.
    snap.weather_code = 200;
    strncpy(snap.main_description, "Thunderstorm", sizeof(snap.main_description) - 1);

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    EXPECT_FALSE(d.open);
}

TEST_F(LlmWeatherDeciderTest, LowHeapSkipsCallAndFallsBack) {
    mockHal->setFreeHeap(1000);  // below LLM_MIN_FREE_HEAP
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    WeatherDecision d = decider.decide(buildInput());

    // Should not have even attempted the HTTP call.
    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), "");
    EXPECT_TRUE(d.open);  // falls back to rule-based (clear/72F/5mph)
}

TEST_F(LlmWeatherDeciderTest, PromptIncludesOpenWindowWhenSet) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput(390, 1140));  // 06:30 to 19:00

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("06:30"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("19:00"));
}

TEST_F(LlmWeatherDeciderTest, PromptShowsUnknownWhenWindowNotSet) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput());  // -1, -1 defaults

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("unknown"));
}

TEST_F(LlmWeatherDeciderTest, TestConnectionSuccess) {
    mockHal->setHttpPostAuthResponse(R"({"choices":[{"message":{"content":"OK"}}]})");

    LlmWeatherDecider decider(mockHal, "http://localhost:8000", "kO7dkihVsUVeb", "qwen3",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    String err = decider.testConnection();

    EXPECT_EQ(err, "");  // empty = success
}

TEST_F(LlmWeatherDeciderTest, TestConnectionFailureNoResponse) {
    mockHal->setHttpPostAuthResponse("");

    LlmWeatherDecider decider(mockHal, "http://localhost:8000", "kO7dkihVsUVeb", "qwen3",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    String err = decider.testConnection();

    EXPECT_NE(err, "");
}

TEST_F(LlmWeatherDeciderTest, TestConnectionEmptyBaseUrlFailsFast) {
    LlmWeatherDecider decider(mockHal, "", "key", "model", LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    String err = decider.testConnection();

    EXPECT_NE(err, "");
    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), "");  // never attempted
}

TEST_F(LlmWeatherDeciderTest, TimeoutClampedToValidRange) {
    // Below minimum (5000ms) should clamp up.
    LlmWeatherDecider tooLow(mockHal, "https://api.example.com", "key", "model",
                             LlmProviderWire::OPENAI_COMPATIBLE, 100);
    // Above maximum (60000ms) should clamp down. We can't directly read the
    // clamped value, but the constructor must not crash and decide() must
    // still function normally (indirect verification via a successful call).
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");
    WeatherDecision d = tooLow.decide(buildInput());
    EXPECT_TRUE(d.open);

    LlmWeatherDecider tooHigh(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 999999);
    d = tooHigh.decide(buildInput());
    EXPECT_TRUE(d.open);
}

TEST_F(LlmWeatherDeciderTest, TrailingSlashOnBaseUrlIsTrimmed) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "http://localhost:8000/", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput());

    EXPECT_EQ(mockHal->getLastHttpPostAuthUrl(), "http://localhost:8000/v1/chat/completions");
}

// Regression for the issue #7 prompt bug: the LLM must see the local-time
// timestamp of each forecast block AND the current local time, all on the same
// wall clock, so it can tell a rain block that starts after the door closes is
// irrelevant. Without timestamps the model blocked on a midnight rain block
// even though the door closes at sunset.
TEST_F(LlmWeatherDeciderTest, PromptIncludesLocalTimesAndTzForForecast) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    // now = 18:00 MDT (Jul 20 2026 18:00 local). tz offset -420 min (-7h, DST).
    long now = 1784595600L;
    // Forecast blocks: 18:00 MDT (Clouds, 0%), 21:00 MDT (Clouds, 0%),
    // 00:00 MDT next day (Rain, 20%) — all UTC epochs.
    std::vector<long> dts = {1784595600L, 1784606400L, 1784617200L};
    std::vector<float> pops = {0.0f, 0.0f, 0.2f};
    std::vector<std::string> descs = {"Clouds", "Clouds", "Rain"};
    decider.decide(buildForecastInput(dts, pops, descs, now, -420, "MDT",
                                      370, 1256));  // window 06:10-20:56

    String body = mockHal->getLastHttpPostAuthBody();

    // Current local time is rendered with the tz label.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("Now (local MDT):"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("18:00"));
    // The window is rendered as before.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("06:10"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("20:56"));
    // Each forecast block is anchored with its local start time.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("18:00 Clouds"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("21:00 Clouds"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("00:00 Rain"));
    // The instruction to ignore post-close blocks is present.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("IGNORE"));
    // Coop context is present so the model judges against the real environment.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("chicken wire"));
}

TEST_F(LlmWeatherDeciderTest, PromptFallsBackToUtcWhenNoTzInfo) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    // No tz offset (0) and empty tz name -> prompt should label times as UTC
    // and still render them.
    std::vector<long> dts = {1784592000L};
    std::vector<float> pops = {0.0f};
    std::vector<std::string> descs = {"Clear"};
    decider.decide(buildForecastInput(dts, pops, descs, /*now*/ 1784592000L,
                                      /*tzOffset*/ 0, /*tzName*/ "",
                                      390, 1140));

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("UTC"));
    // 1784592000 = 2026-07-21 00:00 UTC
    EXPECT_THAT(body.c_str(), testing::HasSubstr("00:00"));
}

// The anti-hallucination integrity rule must always appear, regardless of
// whether a custom prompt override is set (issue #7).
TEST_F(LlmWeatherDeciderTest, IntegrityRuleAlwaysPresent) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput(390, 1140));

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("Integrity rule"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("MUST answer"));
}

// Default judgment guidance is used when no override is set (issue #8).
TEST_F(LlmWeatherDeciderTest, DefaultGuidanceUsedWhenNoOverride) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput(390, 1140));

    String body = mockHal->getLastHttpPostAuthBody();
    // A distinctive phrase from the built-in default guidance.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("Default to letting them out"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("108F"));
}

// A custom override replaces the guidance paragraph, while the coop context,
// integrity rule, data, and JSON instruction remain (issue #8).
TEST_F(LlmWeatherDeciderTest, CustomOverrideReplacesDefaultGuidance) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.setPromptOverride("MY_CUSTOM_MARKER_RULEXYZ");
    decider.decide(buildInput(390, 1140));

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("MY_CUSTOM_MARKER_RULEXYZ"));
    // Default guidance should NOT appear when an override is set.
    EXPECT_THAT(body.c_str(), testing::Not(testing::HasSubstr("Default to letting them out")));
    // Firmware-injected scaffolding must survive the override.
    EXPECT_THAT(body.c_str(), testing::HasSubstr("Integrity rule"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("chicken wire"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("06:30"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("19:00"));
}

TEST_F(LlmWeatherDeciderTest, DefaultGuidanceIsExposedStatically) {
    // The UI uses this to pre-populate the editable textarea. It must be a
    // non-empty string containing the key thresholds.
    const char* d = LlmWeatherDecider::defaultJudgmentGuidance();
    ASSERT_NE(d, nullptr);
    EXPECT_GT(strlen(d), 0u);
    EXPECT_STREQ(d, LlmWeatherDecider::defaultJudgmentGuidance());  // stable
    std::string s(d);
    EXPECT_NE(s.find("25 mph"), std::string::npos);
    EXPECT_NE(s.find("108F"), std::string::npos);
}

// The coop-local on-board sensor temp must appear in the prompt and be flagged
// as authoritative over the OWM area temp (issue #8). The coop is shaded under
// a tree and routinely differs from the OWM grid-cell average.
TEST_F(LlmWeatherDeciderTest, LocalCoopTempIncludedAndMarkedAuthoritative) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    // Build an input by hand so we can set the local-temp fields directly.
    WeatherDecisionInput input{snap, nullptr, 0, String("imperial"),
                               390, 1140, /*now*/ -1, /*tz*/ 0, String(""),
                               /*local_temp_f*/ 104.5f, /*source*/ "coop sensor 1"};
    decider.decide(input);

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::HasSubstr("Coop-local temp (coop sensor 1): 104.5F"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("ACTUAL temperature at the coop"));
    EXPECT_THAT(body.c_str(), testing::HasSubstr("more authoritative"));
}

// When no sensor is connected (local_temp_f = NAN), the local-temp line must
// NOT appear — the model judges from the OWM temp alone.
TEST_F(LlmWeatherDeciderTest, LocalCoopTempOmittedWhenNoSensor) {
    mockHal->setHttpPostAuthResponse(
        R"({"choices":[{"message":{"content":"{\"open\": true, \"reason\": \"ok\"}"}}]})");

    LlmWeatherDecider decider(mockHal, "https://api.example.com", "key", "model",
                              LlmProviderWire::OPENAI_COMPATIBLE, 15000);
    decider.decide(buildInput(390, 1140));  // local_temp_f defaults to NAN

    String body = mockHal->getLastHttpPostAuthBody();
    EXPECT_THAT(body.c_str(), testing::Not(testing::HasSubstr("Coop-local temp")));
}
