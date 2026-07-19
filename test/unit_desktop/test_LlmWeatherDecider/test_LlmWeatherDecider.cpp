#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

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
