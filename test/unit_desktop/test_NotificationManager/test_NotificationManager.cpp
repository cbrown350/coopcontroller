#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "NotificationManager.h"

using namespace fakeit;

class NotificationManagerTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    NotificationManager notifier;

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

        notifier.begin(mockHal);
    }

    void TearDown() override {
        delete mockHal;
        mockHal = nullptr;
    }

    void configureTelegram() {
        notifier.setTelegramEnabled(true);
        notifier.setTelegramBotToken("123456:ABC-DEF");
        notifier.setTelegramChatId("987654321");
    }

    void configureEmail() {
        notifier.setEmailEnabled(true);
        notifier.setSmtpServer("smtp.example.com");
        notifier.setSmtpPort(587);
        notifier.setSmtpUsername("user@example.com");
        notifier.setSmtpPassword("password123");
        notifier.setEmailTo("test@example.com");
        notifier.setEmailFrom("coop@example.com");
    }
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(NotificationManagerTest, DefaultState_NothingEnabled) {
    EXPECT_FALSE(notifier.getTelegramEnabled());
    EXPECT_FALSE(notifier.getEmailEnabled());
    EXPECT_EQ(notifier.getTotalSent(), 0u);
    EXPECT_EQ(notifier.getTotalFailed(), 0u);
}

TEST_F(NotificationManagerTest, Begin_InitializesSuccessfully) {
    NotificationManager nm;
    nm.begin(mockHal);
    EXPECT_EQ(nm.getTotalSent(), 0u);
}

// ============================================================================
// TELEGRAM TESTS
// ============================================================================

TEST_F(NotificationManagerTest, Telegram_NotifyWhenConfigured) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true,"result":{}})");

    notifier.notify(AlertType::PUMP_ERROR, "Test pump error");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "https://api.telegram.org/bot123456:ABC-DEF/sendMessage");
    EXPECT_TRUE(mockHal->getLastHttpPostBody().indexOf("Test pump error") >= 0);
    EXPECT_EQ(notifier.getTotalSent(), 1u);
}

TEST_F(NotificationManagerTest, Telegram_NoNotifyWhenDisabled) {
    notifier.setTelegramEnabled(false);
    notifier.setTelegramBotToken("token");
    notifier.setTelegramChatId("chatid");
    mockHal->setWiFiConnected(true);

    notifier.notify(AlertType::PUMP_ERROR, "Test");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
    EXPECT_EQ(notifier.getTotalSent(), 0u);
}

TEST_F(NotificationManagerTest, Telegram_NoNotifyWhenWifiDisconnected) {
    configureTelegram();
    mockHal->setWiFiConnected(false);

    notifier.notify(AlertType::PUMP_ERROR, "Test");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
}

TEST_F(NotificationManagerTest, Telegram_NoNotifyWithoutToken) {
    notifier.setTelegramEnabled(true);
    notifier.setTelegramChatId("123");
    // No bot token set
    mockHal->setWiFiConnected(true);

    notifier.notify(AlertType::PUMP_ERROR, "Test");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
}

TEST_F(NotificationManagerTest, Telegram_FailureIncrementsFailedCount) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(""); // Empty response = failure

    notifier.notify(AlertType::PUMP_ERROR, "Test");

    EXPECT_EQ(notifier.getTotalFailed(), 1u);
    EXPECT_EQ(notifier.getTotalSent(), 0u);
}

TEST_F(NotificationManagerTest, Telegram_TestSend_Success) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true,"result":{}})");

    NotificationResult result = notifier.sendTest(NotificationChannel::TELEGRAM);

    EXPECT_TRUE(result.success);
}

TEST_F(NotificationManagerTest, Telegram_TestSend_FailsWithoutConfig) {
    mockHal->setWiFiConnected(true);

    NotificationResult result = notifier.sendTest(NotificationChannel::TELEGRAM);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error_message.indexOf("not configured") >= 0);
}

TEST_F(NotificationManagerTest, Telegram_TestSend_FailsWithoutWifi) {
    configureTelegram();
    mockHal->setWiFiConnected(false);

    NotificationResult result = notifier.sendTest(NotificationChannel::TELEGRAM);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error_message.indexOf("WiFi") >= 0);
}

// ============================================================================
// EMAIL TESTS
// ============================================================================

TEST_F(NotificationManagerTest, Email_NotifyWhenConfigured) {
    configureEmail();
    mockHal->setWiFiConnected(true);
    mockHal->setSmtpResult(""); // Empty = success

    notifier.notify(AlertType::SENSOR_ERROR, "Sensor failure");

    EXPECT_EQ(mockHal->getLastSmtpHost(), "smtp.example.com");
    EXPECT_EQ(mockHal->getLastSmtpPort(), 587);
    EXPECT_EQ(mockHal->getLastSmtpTo(), "test@example.com");
    EXPECT_TRUE(mockHal->getLastSmtpSubject().indexOf("Sensor") >= 0 || mockHal->getLastSmtpBody().indexOf("Sensor") >= 0);
}

TEST_F(NotificationManagerTest, Email_TestSend_FailsWithoutConfig) {
    mockHal->setWiFiConnected(true);

    NotificationResult result = notifier.sendTest(NotificationChannel::EMAIL);

    EXPECT_FALSE(result.success);
}

// ============================================================================
// ALERT PREFERENCE TESTS
// ============================================================================

TEST_F(NotificationManagerTest, AlertPreference_PumpErrorDefault) {
    EXPECT_TRUE(notifier.getNotifyOnPumpError());
}

TEST_F(NotificationManagerTest, AlertPreference_WifiDisconnectDefaultOff) {
    EXPECT_FALSE(notifier.getNotifyOnWifiDisconnect());
}

TEST_F(NotificationManagerTest, AlertPreference_DisabledAlertDoesNotNotify) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");

    notifier.setNotifyOnPumpError(false);
    notifier.notify(AlertType::PUMP_ERROR, "Test");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
    EXPECT_EQ(notifier.getTotalSent(), 0u);
}

TEST_F(NotificationManagerTest, AlertPreference_WifiDisconnectNotNotified) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    notifier.setNotifyOnWifiDisconnect(false);

    notifier.notify(AlertType::WIFI_DISCONNECTED, "WiFi lost");

    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
}

// ============================================================================
// RATE LIMITING TESTS
// ============================================================================

TEST_F(NotificationManagerTest, RateLimiting_SecondAlertBlocked) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 10000;

    notifier.notify(AlertType::PUMP_ERROR, "First");
    EXPECT_EQ(notifier.getTotalSent(), 1u);

    // Reset mock to detect if another call is made
    mockHal->resetHttpPost();
    mockHal->millisValue = 20000; // Only 10 seconds later

    notifier.notify(AlertType::PUMP_ERROR, "Second");
    EXPECT_EQ(mockHal->getLastHttpPostUrl(), ""); // Rate limited
    EXPECT_EQ(notifier.getTotalSent(), 1u);
}

TEST_F(NotificationManagerTest, RateLimiting_AllowsAfterInterval) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 10000;

    notifier.notify(AlertType::PUMP_ERROR, "First");
    EXPECT_EQ(notifier.getTotalSent(), 1u);

    mockHal->resetHttpPost();
    mockHal->millisValue = 80000; // 70 seconds later (>60s limit)

    notifier.notify(AlertType::PUMP_ERROR, "Second");
    EXPECT_NE(mockHal->getLastHttpPostUrl(), "");
    EXPECT_EQ(notifier.getTotalSent(), 2u);
}

TEST_F(NotificationManagerTest, RateLimiting_DifferentAlertTypesNotBlocked) {
    configureTelegram();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 10000;

    notifier.notify(AlertType::PUMP_ERROR, "Pump");
    EXPECT_EQ(notifier.getTotalSent(), 1u);

    mockHal->resetHttpPost();
    mockHal->millisValue = 15000; // 5 seconds later

    notifier.notify(AlertType::SENSOR_ERROR, "Sensor");
    EXPECT_NE(mockHal->getLastHttpPostUrl(), "");
    EXPECT_EQ(notifier.getTotalSent(), 2u);
}

// ============================================================================
// JSON STATUS TESTS
// ============================================================================

TEST_F(NotificationManagerTest, ToJson_ContainsAllFields) {
    configureTelegram();
    configureEmail();

    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    notifier.toJson(json);

    EXPECT_TRUE(json["telegram_enabled"].as<bool>());
    EXPECT_TRUE(json["telegram_configured"].as<bool>());
    EXPECT_TRUE(json["email_enabled"].as<bool>());
    EXPECT_TRUE(json["email_configured"].as<bool>());
    EXPECT_EQ(json["total_sent"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["total_failed"].as<unsigned int>(), 0u);
}

// ============================================================================
// BOTH CHANNELS TESTS
// ============================================================================

TEST_F(NotificationManagerTest, BothChannels_SendsToBoth) {
    configureTelegram();
    configureEmail();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");

    notifier.notify(AlertType::DOOR_FAULT, "Door stuck");

    // Both channels sent = 2 total
    EXPECT_EQ(notifier.getTotalSent(), 2u);
}
