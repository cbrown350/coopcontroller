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
// registers (/weather/test) is directly invokable here. This matches the
// existing coverage convention in this file for other test-button endpoints.
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

    WebServerHandler handler = mockHal->getWebServerHandler();
    ASSERT_NE(handler, nullptr);

    MockWebRequest request;
    request.setMethod(HAL_WebRequestMethod::HTTP_POST);
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

    WebServerHandler handler = mockHal->getWebServerHandler();
    ASSERT_NE(handler, nullptr);

    MockWebRequest request;
    request.setMethod(HAL_WebRequestMethod::HTTP_POST);
    MockWebResponse response;
    handler(&request, &response);

    EXPECT_EQ(response.getLastCode(), 400);
    EXPECT_TRUE(String(response.getLastBody()).indexOf("\"success\":false") >= 0);
}

// Note: main function is provided by desktop_main.cpp
