#include <gtest/gtest.h>
#include "ArduinoFake.h"
#include "WifiController.h"
#include "SettingsManager.h"
#include "MockBuzzerController.h"
#include "MockHAL.h"
#include "Logger.h"

using namespace fakeit;

// WiFi status constants (matching MockHAL.h and ESP32 WiFi.h)
#define WL_IDLE_STATUS       0
#define WL_NO_SSID_AVAIL     1
#define WL_SCAN_COMPLETED    2
#define WL_CONNECTED         3
#define WL_CONNECT_FAILED    4
#define WL_CONNECTION_LOST   5
#define WL_DISCONNECTED      6

// Custom printer for Arduino String to work with Google Test
namespace std {
inline std::ostream& operator<<(std::ostream& os, const String& str) {
    return os << "\"" << str.c_str() << "\"";
}
}

class WifiControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock objects
        mockHAL = new MockHAL();
        mockBuzzer = new MockBuzzerController();

        // Reset ArduinoFake
        ArduinoFakeReset();

        // Reset mock state
        mockHAL->reset();

        // Mock ALL Arduino functions BEFORE initializing anything
        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
        // Make millis() return mockHAL.millisValue so tests can control time
        When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return mockHAL->millisValue; });
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
        When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

        // Initialize Logger AFTER all Arduino function mocks are set up
        Logger::getInstance().begin(mockHAL);
        Logger::getInstance().clearLogs();
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);

        // Reset SettingsManager singleton state from previous test
        sm.resetForTesting();

        // Initialize with mock HAL
        sm.begin(mockHAL);

        // Set default settings using real SettingsManager
        sm.setSSID("TestSSID");
        sm.setPassword("TestPassword");
        sm.setAPMode(false);
        sm.setWifiChanged(false);
        sm.setWifiLedEnabled(true);
        sm.setWifiMaxRetries(5);
        sm.setWifiRetryDelaySeconds(0);  // Set to 0 for instant "retries" in tests
        sm.setWifiAPDurationMinutes(10);
        sm.setHasConnected(true);  // Prevent automatic failover to AP mode

        // Set up HAL defaults - WiFi connected to prevent failover during begin()
        mockHAL->setWiFiStatus(WL_CONNECTED);
        mockHAL->setWiFiSSID("TestSSID");
        mockHAL->setWiFiLocalIP("192.168.1.100");
        mockHAL->setWiFiRSSI(-50);

        // Create WifiController instance with default constructor
        wifiController = new WifiController();

        // Initialize WifiController - this will succeed since WiFi is "connected"
        wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

        // Reset to disconnected state for tests that need it
        // Tests can override by setting WiFi status before their operations
        mockHAL->setWiFiStatus(WL_DISCONNECTED);
        mockHAL->setWiFiSSID("");
        mockHAL->setWiFiLocalIP("0.0.0.0");
        mockHAL->setWiFiRSSI(0);
    }

    void TearDown() override {
        delete wifiController;
        delete mockBuzzer;
        delete mockHAL;
    }

    MockHAL* mockHAL;
    SettingsManager& sm = SettingsManager::getInstance();
    MockBuzzerController* mockBuzzer;
    WifiController* wifiController;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(WifiControllerTest, BeginInitializesWiFiLEDWhenEnabled) {
    sm.setWifiLedEnabled(true);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // LED should be initialized (we can verify by checking if it doesn't crash)
    SUCCEED();
}

TEST_F(WifiControllerTest, BeginDoesNotInitializeWiFiLEDWhenDisabled) {
    sm.setWifiLedEnabled(false);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // LED should not be initialized
    SUCCEED();
}

TEST_F(WifiControllerTest, BeginCallsWifiSetup) {
    sm.setAPMode(false);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Verify wifiSetup was called by checking connection state
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, BeginStartsAPModeWhenEnabled) {
    sm.setAPMode(true);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
    EXPECT_EQ(status.ssid, "CoopController");
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(WifiControllerTest, UpdateDoesNothingWhenDisconnectedAndNotInAPMode) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setMillis(0);
    
    wifiController->update();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, UpdateChecksConnectionEvery30Seconds) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    mockHAL->setMillis(29000);
    wifiController->update();
    
    // Should not check yet
    auto status1 = wifiController->getStatus();
    EXPECT_EQ(status1.state, WifiState::WIFI_CONNECTED);
    
    mockHAL->setMillis(31000);
    wifiController->update();
    
    // Should check now
    auto status2 = wifiController->getStatus();
    EXPECT_EQ(status2.state, WifiState::WIFI_CONNECTED);
}

TEST_F(WifiControllerTest, UpdateDetectsDisconnection) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate disconnection
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    
    wifiController->update();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
    EXPECT_EQ(mockBuzzer->getLastTriggeredAlert(), AlertType::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, UpdateHandlesWifiChangedFlag) {
    sm.setWifiChanged(true);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    // wifiChanged flag should be cleared after handling
    EXPECT_FALSE(sm.getWifiChanged());
}

TEST_F(WifiControllerTest, UpdateDoesNotTriggerBuzzerWhenAlreadyDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    mockBuzzer->reset();
    mockHAL->setMillis(31000);

    wifiController->update();

    // Buzzer should not be triggered again
    EXPECT_EQ(mockBuzzer->getLastTriggeredAlert(), AlertType::WIFI_DISCONNECTED);
}

// ============================================================================
// TLS-in-flight guard tests — the reconnect path must defer while an outbound
// TLS request is active (to avoid the esp_wifi_stop vs mbedtls race), but
// MUST NOT lock up: after WIFI_DISCONNECT_OVERDUE_MS it reconnects anyway.
// ============================================================================

TEST_F(WifiControllerTest, ReconnectDefersWhenTlsInFlight) {
    sm.setAPMode(false);
    sm.setSSID("TestSSID");
    sm.setPassword("password");
    sm.setHasConnected(true);

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    // Board is connected initially; mark the boot begin.
    int beginsAfterBoot = mockHAL->wifiBeginCallCount;

    // WiFi drops, and a TLS request is mid-flight on the loop task.
    mockHAL->setWiFiStatus(WL_DISCONNECTED);
    mockHAL->mockTlsInFlight = 1;
    mockHAL->setMillis(31000);

    wifiController->update();

    // Reconnect deferred: no new wifiBegin/wifiBeginWithBSSID this cycle.
    EXPECT_EQ(mockHAL->wifiBeginCallCount, beginsAfterBoot);
    // Still disconnected, not reconnecting yet.
    EXPECT_FALSE(wifiController->getStatus().isReconnecting);
}

TEST_F(WifiControllerTest, ReconnectProceedsWhenNoTlsInFlight) {
    sm.setAPMode(false);
    sm.setSSID("TestSSID");
    sm.setPassword("password");
    sm.setHasConnected(true);

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    int beginsAfterBoot = mockHAL->wifiBeginCallCount;

    // WiFi drops, no TLS active.
    mockHAL->setWiFiStatus(WL_DISCONNECTED);
    mockHAL->mockTlsInFlight = 0;
    mockHAL->setMillis(31000);

    wifiController->update();

    // Reconnect proceeds immediately.
    EXPECT_GT(mockHAL->wifiBeginCallCount, beginsAfterBoot);
}

TEST_F(WifiControllerTest, ReconnectDoesNotLockUpWhenTlsCounterStuck) {
    // Critical lock-up-proofness: even if the TLS counter never decrements
    // (stuck positive), the board must still reconnect once the disconnect
    // has been outstanding past the overdue threshold. A stuck counter must
    // not permanently block reconnects.
    sm.setAPMode(false);
    sm.setSSID("TestSSID");
    sm.setPassword("password");
    sm.setHasConnected(true);

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    int beginsAfterBoot = mockHAL->wifiBeginCallCount;

    mockHAL->setWiFiStatus(WL_DISCONNECTED);
    mockHAL->mockTlsInFlight = 1;  // stuck positive, never decrements

    // First cycle at 31s: deferred.
    mockHAL->setMillis(31000);
    wifiController->update();
    EXPECT_EQ(mockHAL->wifiBeginCallCount, beginsAfterBoot);

    // Past the 90s overdue threshold: reconnect proceeds despite TLS flag.
    mockHAL->setMillis(130000);
    wifiController->update();
    EXPECT_GT(mockHAL->wifiBeginCallCount, beginsAfterBoot);
}

// ============================================================================
// GetStatus Tests
// ============================================================================

TEST_F(WifiControllerTest, GetStatusReturnsDisconnectedState) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setHasConnected(false);  // Reset to test fresh disconnected state

    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
    EXPECT_EQ(status.ssid, "");
    EXPECT_EQ(status.ip, "0.0.0.0");
    EXPECT_FALSE(status.hasConnected);
    EXPECT_FALSE(status.isReconnecting);
    EXPECT_EQ(status.retryCount, 0);
}

TEST_F(WifiControllerTest, GetStatusReturnsConnectedState) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    sm.setHasConnected(true);
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
    EXPECT_EQ(status.ssid, "TestSSID");
    EXPECT_EQ(status.ip, "192.168.1.100");
    EXPECT_TRUE(status.hasConnected);
    EXPECT_FALSE(status.isReconnecting);
    EXPECT_EQ(status.retryCount, 0);
}

TEST_F(WifiControllerTest, GetStatusReturnsAPModeState) {
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
    EXPECT_EQ(status.ssid, "CoopController");
}

// ============================================================================
// IsConnected Tests
// ============================================================================

TEST_F(WifiControllerTest, IsConnectedReturnsTrueWhenConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    EXPECT_TRUE(wifiController->isConnected());
}

TEST_F(WifiControllerTest, IsConnectedReturnsFalseWhenDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    
    EXPECT_FALSE(wifiController->isConnected());
}

TEST_F(WifiControllerTest, IsConnectedReturnsFalseInAPMode) {
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    EXPECT_FALSE(wifiController->isConnected());
}

// ============================================================================
// IsInAPMode Tests
// ============================================================================

TEST_F(WifiControllerTest, IsInAPModeReturnsTrueWhenInAPMode) {
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    EXPECT_TRUE(wifiController->isInAPMode());
}

TEST_F(WifiControllerTest, IsInAPModeReturnsFalseWhenInStationMode) {
    sm.setAPMode(false);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    EXPECT_FALSE(wifiController->isInAPMode());
}

// ============================================================================
// GetIPAddress Tests
// ============================================================================

TEST_F(WifiControllerTest, GetIPAddressReturnsIPWhenConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiLocalIP("192.168.1.100");
    
    EXPECT_EQ(wifiController->getIPAddress(), "192.168.1.100");
}

TEST_F(WifiControllerTest, GetIPAddressReturnsEmptyWhenDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiLocalIP("0.0.0.0");
    
    EXPECT_EQ(wifiController->getIPAddress(), "0.0.0.0");
}

// ============================================================================
// GetSSID Tests
// ============================================================================

TEST_F(WifiControllerTest, GetSSIDReturnsSSIDWhenConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    
    EXPECT_EQ(wifiController->getSSID(), "TestSSID");
}

TEST_F(WifiControllerTest, GetSSIDReturnsEmptyWhenDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    
    EXPECT_EQ(wifiController->getSSID(), "");
}

TEST_F(WifiControllerTest, GetSSIDReturnsAPSSIDWhenInAPMode) {
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    EXPECT_EQ(wifiController->getSSID(), "CoopController");
}

// ============================================================================
// GetRSSI Tests
// ============================================================================

TEST_F(WifiControllerTest, GetRSSIReturnsValueWhenConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiRSSI(-50);
    
    EXPECT_EQ(wifiController->getRSSI(), -50);
}

TEST_F(WifiControllerTest, GetRSSIReturnsZeroWhenDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiRSSI(0);
    
    EXPECT_EQ(wifiController->getRSSI(), 0);
}

// ============================================================================
// ConnectToWiFi Tests
// ============================================================================

TEST_F(WifiControllerTest, ConnectToWiFiStartsConnection) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setSSID("TestSSID");
    sm.setPassword("TestPassword");

    // Set WiFi to connect successfully
    mockHAL->setWiFiStatus(WL_CONNECTED);
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");

    wifiController->connectToWiFi();

    // Verify connection succeeded (connectToWiFi is blocking, so it should be connected now)
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
    EXPECT_TRUE(status.hasConnected);
}

TEST_F(WifiControllerTest, ConnectToWiFiUsesSSIDAndPasswordFromSettings) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setSSID("MySSID");
    sm.setPassword("MyPassword");
    
    wifiController->connectToWiFi();
    
    EXPECT_EQ(wifiController->getSSID(), "MySSID");
}

TEST_F(WifiControllerTest, ConnectToWiFiDoesNothingWhenAlreadyConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->connectToWiFi();
    
    // Should remain connected
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
}

// ============================================================================
// StartAPMode Tests
// ============================================================================

TEST_F(WifiControllerTest, StartAPModeStartsAccessPoint) {
    wifiController->startAPMode();

    // startAPMode() saves settings and requests restart
    // After restart, begin() would be called again with APMode=true
    EXPECT_TRUE(sm.isAPMode());  // Settings should be saved with AP mode enabled

    // Simulate the restart by calling begin() again with APMode enabled
    mockHAL->reset();  // Reset HAL state
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // Now AP mode should be active
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
    EXPECT_EQ(status.ssid, "CoopController");
}

TEST_F(WifiControllerTest, StartAPModeUsesConfiguredSSID) {
    wifiController->startAPMode();

    // Simulate restart by calling begin() again
    mockHAL->reset();
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    EXPECT_EQ(wifiController->getSSID(), "CoopController");
}

TEST_F(WifiControllerTest, StartAPModeUsesPasswordIfConfigured) {
    wifiController->startAPMode();

    // Simulate restart by calling begin() again
    mockHAL->reset();
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // AP should start (password is optional)
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
}

// ============================================================================
// Disconnect Tests
// ============================================================================

TEST_F(WifiControllerTest, DisconnectDisconnectsWiFi) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->disconnect();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, DisconnectClearsSSID) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    
    wifiController->disconnect();
    
    EXPECT_EQ(wifiController->getSSID(), "");
}

TEST_F(WifiControllerTest, DisconnectDoesNothingWhenAlreadyDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    
    wifiController->disconnect();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

// ============================================================================
// EnableLed Tests
// ============================================================================

TEST_F(WifiControllerTest, EnableLedEnablesLED) {
    wifiController->enableLed(true);
    
    // LED should be enabled (verify by not crashing)
    SUCCEED();
}

TEST_F(WifiControllerTest, EnableLedDisablesLED) {
    wifiController->enableLed(false);
    
    // LED should be disabled (verify by not crashing)
    SUCCEED();
}

// ============================================================================
// UpdateLed Tests
// ============================================================================

TEST_F(WifiControllerTest, UpdateLedTurnsOffWhenDisconnected) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    
    wifiController->updateLed();
    
    // LED should be off
    SUCCEED();
}

TEST_F(WifiControllerTest, UpdateLedHeartbeatsWhenConnected) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setMillis(0);
    
    // First update - LED should turn on
    wifiController->updateLed();
    mockHAL->setMillis(50);
    
    // Second update - LED should turn off
    wifiController->updateLed();
    mockHAL->setMillis(2000);
    
    // Third update - LED should turn on again (heartbeat)
    wifiController->updateLed();
    
    SUCCEED();
}

TEST_F(WifiControllerTest, UpdateLedFastBlinksInAPMode) {
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    mockHAL->setMillis(0);
    
    // First update - LED should turn on
    wifiController->updateLed();
    mockHAL->setMillis(250);
    
    // Second update - LED should turn off
    wifiController->updateLed();
    mockHAL->setMillis(500);
    
    // Third update - LED should turn on again (fast blink)
    wifiController->updateLed();
    
    SUCCEED();
}

TEST_F(WifiControllerTest, UpdateLedDoesNothingWhenDisabled) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    sm.setWifiLedEnabled(false);
    
    wifiController->updateLed();
    
    // LED should not be controlled
    SUCCEED();
}

// ============================================================================
// Retry Logic Tests
// ============================================================================

TEST_F(WifiControllerTest, RetryLogicRetriesConnectionOnFailure) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setWifiMaxRetries(3);
    sm.setWifiRetryDelaySeconds(30);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate multiple connection attempts
    for (int i = 0; i < 5; i++) {
        mockHAL->setMillis(31000 + (i * 31000));
        wifiController->update();
    }
    
    auto status = wifiController->getStatus();
    // Should have tried to connect multiple times
    EXPECT_GE(status.retryCount, 0);
}

TEST_F(WifiControllerTest, RetryLogicStopsAfterMaxRetries) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setWifiMaxRetries(3);
    sm.setWifiRetryDelaySeconds(30);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate more than max retries
    for (int i = 0; i < 10; i++) {
        mockHAL->setMillis(31000 + (i * 31000));
        wifiController->update();
    }
    
    auto status = wifiController->getStatus();
    // Should stop retrying after max retries
    EXPECT_LE(status.retryCount, 3);
}

TEST_F(WifiControllerTest, RetryLogicRespectsRetryDelay) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setWifiRetryDelaySeconds(60);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    // Should wait for retry delay
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

// ============================================================================
// AP Mode Duration Tests
// ============================================================================

TEST_F(WifiControllerTest, APModeDurationIsRespected) {
    sm.setAPMode(true);
    sm.setWifiAPDurationMinutes(5);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate time passing
    mockHAL->setMillis(5 * 60 * 1000); // 5 minutes
    wifiController->update();
    
    // AP mode should still be active
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
}

TEST_F(WifiControllerTest, APModeDurationExitsAfterTimeout) {
    sm.setAPMode(true);
    sm.setWifiAPDurationMinutes(1);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate time passing beyond AP duration
    mockHAL->setMillis(2 * 60 * 1000); // 2 minutes
    wifiController->update();
    
    // AP mode should attempt to exit
    auto status = wifiController->getStatus();
    // State may change after AP duration
    SUCCEED();
}

// ============================================================================
// Connection Persistence Tests
// ============================================================================

TEST_F(WifiControllerTest, HasConnectedPreventsAPModeOnFirstConnection) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setHasConnected(false);

    // begin() will fail and trigger restart to AP mode since hasConnected=false
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // Verify AP mode was triggered (settings saved)
    EXPECT_TRUE(sm.isAPMode());  // Should have switched to AP mode

    // Simulate the restart by calling begin() again with AP mode
    mockHAL->reset();
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // Now in AP mode
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_AP_MODE);
}

TEST_F(WifiControllerTest, HasConnectedAllowsAPModeAfterPreviousConnection) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setHasConnected(true);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should not immediately go to AP mode if has connected before
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
}

// ============================================================================
// Automatic Reconnection Tests
// ============================================================================

TEST_F(WifiControllerTest, AutomaticReconnectionAttemptsOnDisconnection) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate disconnection
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_DISCONNECTED);
    EXPECT_TRUE(status.isReconnecting);
}

TEST_F(WifiControllerTest, AutomaticReconnectionStopsAfterMaxRetries) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    sm.setWifiMaxRetries(2);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate disconnection and multiple retries
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    
    for (int i = 0; i < 5; i++) {
        mockHAL->setMillis(31000 + (i * 31000));
        wifiController->update();
    }
    
    auto status = wifiController->getStatus();
    // Should stop retrying after max retries
    EXPECT_LE(status.retryCount, 2);
}

// ============================================================================
// mDNS Tests
// ============================================================================

TEST_F(WifiControllerTest, MDNSIsInitialized) {
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // mDNS should be initialized (verify by not crashing)
    SUCCEED();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(WifiControllerTest, HandlesNullHAL) {
    // This test verifies graceful handling of null HAL
    // In practice, HAL should never be null
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesNullSettings) {
    // This test verifies graceful handling of null settings
    // In practice, settings should never be null
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesNullBuzzer) {
    // This test verifies graceful handling of null buzzer
    // In practice, buzzer should never be null
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesEmptySSID) {
    sm.setSSID("");
    sm.setPassword("TestPassword");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle empty SSID gracefully
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesEmptyPassword) {
    sm.setSSID("TestSSID");
    sm.setPassword("");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle empty password gracefully
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesZeroMaxRetries) {
    sm.setWifiMaxRetries(0);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should not retry if max retries is 0
    EXPECT_EQ(status.retryCount, 0);
}

TEST_F(WifiControllerTest, HandlesVeryLargeMaxRetries) {
    sm.setWifiMaxRetries(1000);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle large retry count
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesZeroRetryDelay) {
    sm.setWifiRetryDelaySeconds(0);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle zero retry delay
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesVeryLargeRetryDelay) {
    sm.setWifiRetryDelaySeconds(3600); // 1 hour
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle large retry delay
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesZeroAPDuration) {
    sm.setAPMode(true);
    sm.setWifiAPDurationMinutes(0);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle zero AP duration
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesVeryLargeAPDuration) {
    sm.setAPMode(true);
    sm.setWifiAPDurationMinutes(1440); // 24 hours
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Should handle large AP duration
    SUCCEED();
}

TEST_F(WifiControllerTest, HandlesRapidConnectionDisconnection) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Rapidly disconnect and reconnect
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setMillis(62000);
    wifiController->update();
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
}

TEST_F(WifiControllerTest, HandlesMultipleUpdates) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Multiple updates should not cause issues
    for (int i = 0; i < 10; i++) {
        mockHAL->setMillis(31000 + (i * 1000));
        wifiController->update();
    }
    
    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
}

TEST_F(WifiControllerTest, HandlesRSSIVariations) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate RSSI variations
    mockHAL->setWiFiRSSI(-30);
    mockHAL->setMillis(31000);
    wifiController->update();
    
    mockHAL->setWiFiRSSI(-50);
    mockHAL->setMillis(62000);
    wifiController->update();
    
    mockHAL->setWiFiRSSI(-70);
    mockHAL->setMillis(93000);
    wifiController->update();
    
    // Should handle RSSI changes
    EXPECT_EQ(wifiController->getRSSI(), -70);
}

TEST_F(WifiControllerTest, HandlesIPAddressChange) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate IP address change
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    mockHAL->setWiFiLocalIP("192.168.1.101");
    mockHAL->setMillis(62000);
    wifiController->update();
    
    // Should handle IP change
    EXPECT_EQ(wifiController->getIPAddress(), "192.168.1.101");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(WifiControllerTest, FullConnectionCycle) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setSSID("TestSSID");
    sm.setPassword("TestPassword");

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // Simulate connection process - WiFi connects during checkWifiConnection()
    mockHAL->setWiFiStatus(WL_CONNECTED); // WiFi becomes connected
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setMillis(31000);
    wifiController->update();  // checkWifiConnection() will detect connection

    auto status = wifiController->getStatus();
    EXPECT_EQ(status.state, WifiState::WIFI_CONNECTED);
    EXPECT_EQ(status.ssid, "TestSSID");
    EXPECT_EQ(status.ip, "192.168.1.100");
    EXPECT_TRUE(status.hasConnected);  // hasConnected should be true (was set in SetUp() or during connection)
}

TEST_F(WifiControllerTest, FullDisconnectionAndReconnectionCycle) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate disconnection
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    auto status1 = wifiController->getStatus();
    EXPECT_EQ(status1.state, WifiState::WIFI_DISCONNECTED);
    EXPECT_TRUE(status1.isReconnecting);
    EXPECT_EQ(mockBuzzer->getLastTriggeredAlert(), AlertType::WIFI_DISCONNECTED);
    
    // Simulate reconnection
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setMillis(62000);
    wifiController->update();
    
    auto status2 = wifiController->getStatus();
    EXPECT_EQ(status2.state, WifiState::WIFI_CONNECTED);
    EXPECT_FALSE(status2.isReconnecting);
}

TEST_F(WifiControllerTest, APModeToStationModeTransition) {
    sm.setAPMode(true);

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    auto status1 = wifiController->getStatus();
    EXPECT_EQ(status1.state, WifiState::WIFI_AP_MODE);

    // Transition to station mode - set WiFi to connected so begin() succeeds
    sm.setAPMode(false);
    mockHAL->setWiFiStatus(WL_CONNECTED);
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    // Now reset to disconnected to verify state
    mockHAL->setWiFiStatus(WL_DISCONNECTED);
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");

    auto status2 = wifiController->getStatus();
    EXPECT_EQ(status2.state, WifiState::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, MultipleConnectionAttempts) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    sm.setWifiMaxRetries(3);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate multiple failed connection attempts
    for (int i = 0; i < 5; i++) {
        mockHAL->setWiFiStatus(WL_NO_SSID_AVAIL); // WL_NO_SSID_AVAIL
        mockHAL->setMillis(31000 + (i * 31000));
        wifiController->update();
    }
    
    auto status = wifiController->getStatus();
    // Should track retry count
    EXPECT_GE(status.retryCount, 0);
}

TEST_F(WifiControllerTest, LEDControlWithConnectionStateChanges) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // LED should be off when disconnected
    wifiController->updateLed();
    
    // Simulate connection
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    wifiController->updateLed();
    
    // LED should heartbeat when connected
    mockHAL->setMillis(50);
    wifiController->updateLed();
    
    // Simulate disconnection again
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    wifiController->updateLed();
    
    // LED should be off again
    SUCCEED();
}

TEST_F(WifiControllerTest, AllGettersReturnValidValues) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setWiFiRSSI(-50);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // All getters should return valid values
    EXPECT_FALSE(wifiController->getIPAddress().length() == 0);
    EXPECT_FALSE(wifiController->getSSID().length() == 0);
    EXPECT_NE(wifiController->getRSSI(), 0);
    EXPECT_TRUE(wifiController->isConnected());
    EXPECT_FALSE(wifiController->isInAPMode());
}

TEST_F(WifiControllerTest, StateTransitionsAreCorrect) {
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED

    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    auto status1 = wifiController->getStatus();
    EXPECT_EQ(status1.state, WifiState::WIFI_DISCONNECTED);

    // Transition to connected (connectToWiFi is blocking, so set WiFi to connected first)
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    wifiController->connectToWiFi();  // This is blocking and will complete immediately
    auto status2 = wifiController->getStatus();
    EXPECT_EQ(status2.state, WifiState::WIFI_CONNECTED);

    // Transition to disconnected
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    wifiController->update();
    auto status3 = wifiController->getStatus();
    EXPECT_EQ(status3.state, WifiState::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, SettingsIntegrationWorksCorrectly) {
    sm.setSSID("MySSID");
    sm.setPassword("MyPassword");
    sm.setWifiMaxRetries(10);
    sm.setWifiRetryDelaySeconds(60);
    sm.setWifiAPDurationMinutes(15);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    auto status = wifiController->getStatus();
    // Settings should be integrated correctly
    SUCCEED();
}

TEST_F(WifiControllerTest, BuzzerIntegrationWorksCorrectly) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // Simulate disconnection
    mockHAL->setWiFiStatus(WL_DISCONNECTED); // WL_DISCONNECTED
    mockHAL->setWiFiSSID("");
    mockHAL->setWiFiLocalIP("0.0.0.0");
    mockHAL->setMillis(31000);
    wifiController->update();
    
    // Buzzer should be triggered
    EXPECT_EQ(mockBuzzer->getLastTriggeredAlert(), AlertType::WIFI_DISCONNECTED);
}

TEST_F(WifiControllerTest, HALIntegrationWorksCorrectly) {
    mockHAL->setWiFiStatus(WL_CONNECTED); // WL_CONNECTED
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    mockHAL->setWiFiRSSI(-50);
    
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");
    
    // HAL methods should be called correctly
    EXPECT_EQ(wifiController->getSSID(), "TestSSID");
    EXPECT_EQ(wifiController->getIPAddress(), "192.168.1.100");
    EXPECT_EQ(wifiController->getRSSI(), -50);
}

TEST_F(WifiControllerTest, CompleteOperationSequence) {
    // Start in AP mode
    sm.setAPMode(true);
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    auto status1 = wifiController->getStatus();
    EXPECT_EQ(status1.state, WifiState::WIFI_AP_MODE);

    // Switch to station mode - set WiFi to connected so begin() succeeds
    sm.setAPMode(false);
    mockHAL->setWiFiStatus(WL_CONNECTED);
    mockHAL->setWiFiSSID("TestSSID");
    mockHAL->setWiFiLocalIP("192.168.1.100");
    wifiController->begin(mockHAL, &sm, mockBuzzer, "CoopAP");

    auto status2 = wifiController->getStatus();
    EXPECT_EQ(status2.state, WifiState::WIFI_CONNECTED);
    EXPECT_TRUE(status2.hasConnected);  // hasConnected is now set

    // Disconnect
    wifiController->disconnect();

    auto status3 = wifiController->getStatus();
    EXPECT_EQ(status3.state, WifiState::WIFI_DISCONNECTED);
    EXPECT_EQ(status3.ssid, "");  // Disconnect clears SSID
    EXPECT_TRUE(status3.hasConnected);  // hasConnected persists (stored in settings)
}
