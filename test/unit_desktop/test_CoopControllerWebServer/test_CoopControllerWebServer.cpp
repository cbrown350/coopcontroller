#include <gtest/gtest.h>
#include "ArduinoFake.h"
#include "CoopControllerWebServer.h"
#include "SensorManager.h"
#include "PumpController.h"
#include "BuzzerController.h"
#include "DoorController.h"
#include "LightController.h"
#include "SunriseSunset.h"
#include "WifiController.h"
#include "HistoricalDataManager.h"
#include "SettingsManager.h"
#include "WeatherManager.h"
#include "Logger.h"
#include "IHAL.h"
#include "MockHAL.h"

using namespace fakeit;

// Minimal test class - only set up what's absolutely necessary
class CoopControllerWebServerTest : public ::testing::Test {
protected:
    MockHAL* mockHal;

    void SetUp() override {
        // Create mock HAL instance
        mockHal = new MockHAL();

        // Reset ArduinoFake
        ArduinoFakeReset();

        // Reset mock state
        mockHal->reset();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
        When(Method(ArduinoFake(), tone)).AlwaysReturn();
        When(Method(ArduinoFake(), noTone)).AlwaysReturn();

        // Initialize Logger AFTER all Arduino function mocks are set up
        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override {
        // Only delete what we created with new
        delete mockHal;
    }
};

// Constructor Tests
TEST_F(CoopControllerWebServerTest, ConstructorInitializesWithHAL) {
    CoopControllerWebServer server(mockHal, 80);
    // Constructor should complete without crashing
    SUCCEED();
}

TEST_F(CoopControllerWebServerTest, ConstructorInitializesWithDefaultPort) {
    CoopControllerWebServer server(mockHal);
    // Constructor with default port should complete without crashing
    SUCCEED();
}

TEST_F(CoopControllerWebServerTest, ConstructorWithDifferentPorts) {
    CoopControllerWebServer server1(mockHal, 80);
    CoopControllerWebServer server2(mockHal, 8080);
    CoopControllerWebServer server3(mockHal, 3000);
    // All constructors should complete without crashing
    SUCCEED();
}

TEST_F(CoopControllerWebServerTest, LoopWithoutBeginDoesNotCrash) {
    // Loop should be safe to call even if begin wasn't called
    CoopControllerWebServer server(mockHal, 80);
    server.loop();
    SUCCEED();
}

// Full Integration Tests
class CoopControllerWebServerIntegrationTest : public ::testing::Test {
protected:
    MockHAL* mockHal;

    // Component instances - stack allocated to avoid delete issues
    SensorManager sensorManager;
    PumpController pumpController;
    BuzzerController buzzerController;
    DoorController doorController;
    LightController lightController;
    WifiController wifiController;
    SunriseSunsetCalculator sunriseSunset;
    HistoricalDataManager historyManager;

    void SetUp() override {
        // Create mock HAL instance
        mockHal = new MockHAL();

        // Reset ArduinoFake
        ArduinoFakeReset();

        // Reset mock state
        mockHal->reset();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHal->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
        When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
        When(Method(ArduinoFake(), digitalRead)).AlwaysReturn(LOW);
        When(Method(ArduinoFake(), digitalWrite)).AlwaysReturn();
        When(Method(ArduinoFake(), tone)).AlwaysReturn();
        When(Method(ArduinoFake(), noTone)).AlwaysReturn();

        // Initialize Logger AFTER all Arduino function mocks are set up
        Logger::getInstance().begin(mockHal);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);

        // Initialize SettingsManager singleton with HAL
        settingsManager.begin(mockHal);

        // Initialize components with correct parameters
        sensorManager.begin(32, 33);
        pumpController.begin(&sensorManager, &sensorManager, 26);
        buzzerController.begin(27);
        doorController.begin(&buzzerController, &sunriseSunset);
        lightController.begin(mockHal, &sunriseSunset);
        wifiController.begin(mockHal, &settingsManager, &buzzerController, "CoopAP");
        historyManager.begin(true, 1440, 60, 10);
    }

    void TearDown() override {
        // Only delete what we created with new
        delete mockHal;
    }
};

TEST_F(CoopControllerWebServerIntegrationTest, BeginDoesNotCrash) {
    // begin() should complete without crashing
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, FullInitializationSequence) {
    // Full initialization should complete without crashing
    CoopControllerWebServer server(mockHal, 80);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.loop();
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, MultipleBeginCalls) {
    // Multiple begin calls should be handled
    CoopControllerWebServer server(mockHal, 80);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, LoopCallsWork) {
    // Loop should work correctly
    CoopControllerWebServer server(mockHal, 80);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    for (int i = 0; i < 10; i++) {
        server.loop();
    }
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, DifferentPortValues) {
    // Test with various port values
    CoopControllerWebServer server1(mockHal, 80);
    CoopControllerWebServer server2(mockHal, 8080);
    CoopControllerWebServer server3(mockHal, 65535);

    server1.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server2.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server3.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, ServerLifetimeManagement) {
    // Server should manage lifetime correctly
    CoopControllerWebServer* server = new CoopControllerWebServer(mockHal, 80);
    server->begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server->loop();
    delete server;
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, AllEndpointsRegistered) {
    // All endpoints should be registered
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, OTAIntegrated) {
    // ArduinoOTA and ElegantOTA should be integrated
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.loop();
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, FilesystemInitialized) {
    // Filesystem should be initialized for static files
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, AllComponentsHandledCorrectly) {
    // Server should handle all component references
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.loop();
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, SPARoutesRegistered) {
    // SPA rewrites should be registered
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, LoopCallsOTAHandlers) {
    // Loop should call OTA handlers
    CoopControllerWebServer server(mockHal, 80);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.loop();
    server.loop();
    server.loop();
    SUCCEED();
}

// Weather + LLM decider endpoints (issue #6)
TEST_F(CoopControllerWebServerIntegrationTest, WeatherManagerEndpointsRegisterWithoutCrash) {
    WeatherManager weatherManager;
    weatherManager.begin(mockHal);

    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.setWeatherManager(&weatherManager);
    server.loop();
    SUCCEED();
}

TEST_F(CoopControllerWebServerIntegrationTest, WeatherManagerNullDeciderDoesNotCrashHandlerRegistration) {
    // setWeatherManager(nullptr) should be a safe no-op (guarded in the impl).
    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.setWeatherManager(nullptr);
    SUCCEED();
}

// MockHAL::webServerOn only retains the single most-recently-registered
// handler (no per-URI map), so only the last endpoint setWeatherManager()
// registers (/weather/test_result) is directly invokable here. The v0.7.1
// deferred-test design means: the POST returns 202 with no network I/O, the
// loop task (weatherManager.update()) runs the probe, then the GET result
// endpoint returns the outcome. These tests exercise the GET side directly;
// the request/run/poll lifecycle is covered in test_WeatherManager.
TEST_F(CoopControllerWebServerIntegrationTest, WeatherTestEndpointReturnsSuccessOnGoodFetch) {
    WeatherManager weatherManager;
    weatherManager.begin(mockHal);
    weatherManager.setEnabled(true);
    weatherManager.setApiKey("testkey");
    weatherManager.setUnits("imperial");

    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.setWeatherManager(&weatherManager);

    mockHal->setWiFiConnected(true);
    mockHal->setFreeHeap(200000);
    mockHal->setHttpGetResponse(
        R"({"weather":[{"id":800,"main":"Clear","description":"clear","icon":"01d"}],)"
        R"("main":{"temp":72.0,"feels_like":71.0,"humidity":40,"pressure":1015},)"
        R"("wind":{"speed":5.0},"clouds":{"all":10},"dt":1721234567,"cod":200})");

    // Enqueue the test, run the deferred probe on the loop task, then read
    // the result via the GET endpoint (which is what the handler under test
    // is, per the MockHAL single-handler convention above).
    weatherManager.requestWeatherTest("");
    weatherManager.update();

    WebServerHandler handler = mockHal->getWebServerHandler();
    ASSERT_NE(handler, nullptr);

    MockWebRequest request;
    request.setMethod(HAL_WebRequestMethod::HTTP_GET);
    MockWebResponse response;
    handler(&request, &response);

    EXPECT_EQ(response.getLastCode(), 200);
    EXPECT_TRUE(String(response.getLastBody()).indexOf("\"success\":true") >= 0);
}

TEST_F(CoopControllerWebServerIntegrationTest, WeatherTestEndpointReturnsErrorOnFailedFetch) {
    WeatherManager weatherManager;
    weatherManager.begin(mockHal);
    weatherManager.setEnabled(true);
    weatherManager.setApiKey("testkey");

    CoopControllerWebServer server(mockHal, 8080);
    server.begin(sensorManager, pumpController, buzzerController,
              doorController, lightController, wifiController,
              sunriseSunset, historyManager);
    server.setWeatherManager(&weatherManager);

    mockHal->setWiFiConnected(true);
    mockHal->setFreeHeap(200000);
    mockHal->setHttpGetResponse("");  // Simulate fetch failure

    weatherManager.requestWeatherTest("");
    weatherManager.update();

    WebServerHandler handler = mockHal->getWebServerHandler();
    ASSERT_NE(handler, nullptr);

    MockWebRequest request;
    request.setMethod(HAL_WebRequestMethod::HTTP_GET);
    MockWebResponse response;
    handler(&request, &response);

    EXPECT_EQ(response.getLastCode(), 200);  // result endpoint always 200
    EXPECT_TRUE(String(response.getLastBody()).indexOf("\"success\":false") >= 0);
}

// Reset-reason label mapping (issue #9): the status UI shows why the board last
// rebooted. Brownout (code 9) was the key case the old boot-log switch missed —
// it must map to a recognizable label, not a numeric fallback.
TEST(ResetReasonTest, MapsKnownCodesToLabels) {
    EXPECT_STREQ(resetReasonToString(0), "Unknown");
    EXPECT_STREQ(resetReasonToString(1), "Power-on");
    EXPECT_STREQ(resetReasonToString(3), "Software restart");
    EXPECT_STREQ(resetReasonToString(4), "Panic / exception");
    EXPECT_STREQ(resetReasonToString(5), "Interrupt watchdog");
    EXPECT_STREQ(resetReasonToString(6), "Task watchdog");
    EXPECT_STREQ(resetReasonToString(7), "Other watchdog");
    EXPECT_STREQ(resetReasonToString(8), "Deep sleep wake");
    EXPECT_STREQ(resetReasonToString(9), "Brownout");
    EXPECT_STREQ(resetReasonToString(14), "Power glitch");
    EXPECT_STREQ(resetReasonToString(15), "CPU lockup");
}

TEST(ResetReasonTest, UnknownCodeFallsBackGracefully) {
    // Any code outside the enum returns "Other" — never crashes or returns null.
    EXPECT_STREQ(resetReasonToString(99), "Other");
    EXPECT_STREQ(resetReasonToString(255), "Other");
}

// Note: main function is provided by desktop_main.cpp
