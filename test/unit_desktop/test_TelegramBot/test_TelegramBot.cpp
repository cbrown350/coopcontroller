#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ArduinoFake.h"

#include "MockHAL.h"
#include "Logger.h"
#include "TelegramBot.h"

using namespace fakeit;

class TelegramBotTest : public ::testing::Test {
protected:
    MockHAL* mockHal;
    TelegramBot bot;

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

        bot.begin(mockHal);
    }

    void TearDown() override {
        delete mockHal;
        mockHal = nullptr;
    }

    void configureBot() {
        bot.setEnabled(true);
        bot.setBotToken("123456:ABC-DEF");
        bot.setChatId("987654321");
        bot.setPollingEnabled(true);
    }

    // Helper: build a Telegram getUpdates response with one command message
    String buildUpdateResponse(long updateId, long chatId, const String& text) {
        return "{\"ok\":true,\"result\":[{\"update_id\":" + String(updateId) +
               ",\"message\":{\"chat\":{\"id\":" + String(chatId) +
               "},\"text\":\"" + text + "\"}}]}";
    }
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(TelegramBotTest, DefaultState) {
    EXPECT_FALSE(bot.getEnabled());
    EXPECT_FALSE(bot.getPollingEnabled());
    EXPECT_FALSE(bot.isConfigured());
    EXPECT_EQ(bot.getMessagesSent(), 0u);
    EXPECT_EQ(bot.getMessagesFailed(), 0u);
    EXPECT_EQ(bot.getCommandsProcessed(), 0u);
}

TEST_F(TelegramBotTest, IsConfigured_RequiresBothTokenAndChatId) {
    bot.setBotToken("token");
    EXPECT_FALSE(bot.isConfigured());

    bot.setChatId("chatid");
    EXPECT_TRUE(bot.isConfigured());
}

// ============================================================================
// SEND MESSAGE TESTS
// ============================================================================

TEST_F(TelegramBotTest, SendMessage_Success) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true,"result":{}})");

    TelegramSendResult result = bot.sendMessage("Hello");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(bot.getMessagesSent(), 1u);
    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "https://api.telegram.org/bot123456:ABC-DEF/sendMessage");
    EXPECT_TRUE(mockHal->getLastHttpPostBody().indexOf("Hello") >= 0);
    EXPECT_TRUE(mockHal->getLastHttpPostBody().indexOf("987654321") >= 0);
}

TEST_F(TelegramBotTest, SendMessage_FailsWhenDisabled) {
    bot.setEnabled(false);
    bot.setBotToken("token");
    bot.setChatId("chatid");
    mockHal->setWiFiConnected(true);

    TelegramSendResult result = bot.sendMessage("Hello");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(mockHal->getLastHttpPostUrl(), "");
}

TEST_F(TelegramBotTest, SendMessage_FailsWithoutWifi) {
    configureBot();
    mockHal->setWiFiConnected(false);

    TelegramSendResult result = bot.sendMessage("Hello");

    EXPECT_FALSE(result.success);
}

TEST_F(TelegramBotTest, SendMessage_FailsOnEmptyResponse) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse("");

    TelegramSendResult result = bot.sendMessage("Hello");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(bot.getMessagesFailed(), 1u);
}

TEST_F(TelegramBotTest, SendMessage_FailsOnApiError) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":false,"description":"Bad Request"})");

    TelegramSendResult result = bot.sendMessage("Hello");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message, "Bad Request");
    EXPECT_EQ(bot.getMessagesFailed(), 1u);
}

// ============================================================================
// POLLING TESTS
// ============================================================================

TEST_F(TelegramBotTest, Update_DoesNothingWhenDisabled) {
    bot.setEnabled(false);
    mockHal->setWiFiConnected(true);
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(TelegramBotTest, Update_DoesNothingWhenPollingDisabled) {
    configureBot();
    bot.setPollingEnabled(false);
    mockHal->setWiFiConnected(true);
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(TelegramBotTest, Update_RespectsPollingInterval) {
    configureBot();
    bot.setPollingIntervalMs(20000);
    mockHal->setWiFiConnected(true);
    mockHal->setHttpGetResponse(R"({"ok":true,"result":[]})");

    // First call should poll
    mockHal->millisValue = 1000;
    bot.update();
    EXPECT_NE(mockHal->getLastHttpGetUrl(), "");

    // Reset and call again within interval - should NOT poll
    mockHal->resetHttpGet();
    mockHal->millisValue = 10000; // Only 9s later
    bot.update();
    EXPECT_EQ(mockHal->getLastHttpGetUrl(), "");

    // Call after interval - should poll
    mockHal->millisValue = 25000; // >20s later
    bot.update();
    EXPECT_NE(mockHal->getLastHttpGetUrl(), "");
}

TEST_F(TelegramBotTest, Update_PollsGetUpdatesUrl) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpGetResponse(R"({"ok":true,"result":[]})");
    mockHal->millisValue = 30000;

    bot.update();

    String url = mockHal->getLastHttpGetUrl();
    EXPECT_TRUE(url.indexOf("bot123456:ABC-DEF/getUpdates") >= 0);
    EXPECT_TRUE(url.indexOf("offset=1") >= 0);
    EXPECT_TRUE(url.indexOf("timeout=0") >= 0);
}

// ============================================================================
// COMMAND DISPATCH TESTS
// ============================================================================

TEST_F(TelegramBotTest, Command_DispatchesRegisteredCommand) {
    configureBot();
    mockHal->setWiFiConnected(true);

    bool commandCalled = false;
    String receivedArgs;
    bot.onCommand("/test", "Test command", [&](const String& args) -> String {
        commandCalled = true;
        receivedArgs = args;
        return "Test response";
    });

    // Simulate getUpdates returning a /test command
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "/test myarg"));
    // For the response sendMessage
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_TRUE(commandCalled);
    EXPECT_EQ(receivedArgs, "myarg");
    EXPECT_EQ(bot.getCommandsProcessed(), 1u);
}

TEST_F(TelegramBotTest, Command_IgnoresWrongChatId) {
    configureBot();
    mockHal->setWiFiConnected(true);

    bool commandCalled = false;
    bot.onCommand("/test", "Test", [&](const String&) -> String {
        commandCalled = true;
        return "response";
    });

    // Message from different chat_id (999999 instead of 987654321)
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 999999, "/test"));
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_FALSE(commandCalled);
    EXPECT_EQ(bot.getCommandsProcessed(), 0u);
}

TEST_F(TelegramBotTest, Command_IgnoresNonCommandText) {
    configureBot();
    mockHal->setWiFiConnected(true);

    bool commandCalled = false;
    bot.onCommand("/test", "Test", [&](const String&) -> String {
        commandCalled = true;
        return "response";
    });

    // Regular text, not a command
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "hello world"));
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_FALSE(commandCalled);
}

TEST_F(TelegramBotTest, Command_UnknownCommandSendsHelp) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "/unknown"));
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 30000;

    bot.update();

    // Should send "Unknown command" response
    EXPECT_TRUE(mockHal->getLastHttpPostBody().indexOf("Unknown command") >= 0);
}

TEST_F(TelegramBotTest, Command_HelpListsAllCommands) {
    configureBot();
    mockHal->setWiFiConnected(true);

    bot.onCommand("/status", "Show status", [](const String&) -> String { return "ok"; });
    bot.onCommand("/door", "Control door", [](const String&) -> String { return "ok"; });

    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "/help"));
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 30000;

    bot.update();

    String body = mockHal->getLastHttpPostBody();
    EXPECT_TRUE(body.indexOf("/status") >= 0);
    EXPECT_TRUE(body.indexOf("/door") >= 0);
    EXPECT_TRUE(body.indexOf("/help") >= 0);
    EXPECT_EQ(bot.getCommandsProcessed(), 1u);
}

TEST_F(TelegramBotTest, Command_StripsBotNameSuffix) {
    configureBot();
    mockHal->setWiFiConnected(true);

    bool commandCalled = false;
    bot.onCommand("/status", "Show status", [&](const String&) -> String {
        commandCalled = true;
        return "status info";
    });

    // Command with bot name suffix
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "/status@MyCoopBot"));
    mockHal->setHttpPostResponse(R"({"ok":true})");
    mockHal->millisValue = 30000;

    bot.update();

    EXPECT_TRUE(commandCalled);
}

TEST_F(TelegramBotTest, Update_AdvancesUpdateId) {
    configureBot();
    mockHal->setWiFiConnected(true);
    mockHal->setHttpPostResponse(R"({"ok":true})");

    // First poll with update_id 100
    mockHal->setHttpGetResponse(buildUpdateResponse(100, 987654321, "/help"));
    mockHal->millisValue = 30000;
    bot.update();

    // Second poll should use offset=101
    mockHal->resetHttpGet();
    mockHal->setHttpGetResponse(R"({"ok":true,"result":[]})");
    mockHal->millisValue = 60000;
    bot.update();

    EXPECT_TRUE(mockHal->getLastHttpGetUrl().indexOf("offset=101") >= 0);
}

// ============================================================================
// JSON STATUS TESTS
// ============================================================================

TEST_F(TelegramBotTest, ToJson_ContainsAllFields) {
    configureBot();

    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    bot.toJson(json);

    EXPECT_TRUE(json["telegram_enabled"].as<bool>());
    EXPECT_TRUE(json["telegram_configured"].as<bool>());
    EXPECT_TRUE(json["telegram_polling_enabled"].as<bool>());
    EXPECT_EQ(json["telegram_messages_sent"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["telegram_messages_failed"].as<unsigned int>(), 0u);
    EXPECT_EQ(json["telegram_commands_processed"].as<unsigned int>(), 0u);
}

// ============================================================================
// COMMAND REGISTRATION TESTS
// ============================================================================

TEST_F(TelegramBotTest, OnCommand_RegistersUpToMax) {
    // Register MAX_COMMANDS (12) commands - should not crash
    for (int i = 0; i < 12; i++) {
        bot.onCommand("/" + String(i), "desc", [](const String&) -> String { return ""; });
    }
    // 13th should be silently rejected (logged as error)
    bot.onCommand("/overflow", "desc", [](const String&) -> String { return ""; });
    // No crash = pass
}
