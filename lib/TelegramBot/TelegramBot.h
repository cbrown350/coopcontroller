#ifndef __TELEGRAM_BOT_H__
#define __TELEGRAM_BOT_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "IHAL.h"

struct TelegramSendResult {
    bool success;
    String error_message;
};

/// Callback: receives argument string, returns response text to send back
using TelegramCommandCallback = std::function<String(const String& args)>;

class TelegramBot {
public:
    TelegramBot() = default;

    void begin(IHAL* hal);

    // Configuration
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool getEnabled() const { return enabled_; }
    void setBotToken(const String& token) { bot_token_ = token; }
    String getBotToken() const { return bot_token_; }
    void setChatId(const String& chatId) { chat_id_ = chatId; }
    String getChatId() const { return chat_id_; }
    void setPollingEnabled(bool enabled) { polling_enabled_ = enabled; }
    bool getPollingEnabled() const { return polling_enabled_; }
    void setPollingIntervalMs(unsigned long interval) { polling_interval_ms_ = interval; }
    unsigned long getPollingIntervalMs() const { return polling_interval_ms_; }

    /// Send a message to the configured chat
    TelegramSendResult sendMessage(const String& message);

    /// Register a bot command (e.g. "/status", "Show system status", callback)
    void onCommand(const String& command, const String& description, TelegramCommandCallback callback);

    /// Call from main loop - polls getUpdates at configured interval
    void update();

    /// Serialize bot status to JSON
    void toJson(JsonObject& json) const;

    // Statistics
    unsigned int getCommandsProcessed() const { return commands_processed_; }
    unsigned int getMessagesSent() const { return messages_sent_; }
    unsigned int getMessagesFailed() const { return messages_failed_; }
    String getLastError() const { return last_error_; }

    /// Check if bot is fully configured (token + chat_id set)
    bool isConfigured() const { return bot_token_.length() > 0 && chat_id_.length() > 0; }

private:
    IHAL* hal_ = nullptr;
    bool enabled_ = false;
    bool polling_enabled_ = false;
    String bot_token_;
    String chat_id_;
    unsigned long polling_interval_ms_ = 20000; // 20 seconds
    unsigned long last_poll_time_ = 0;
    long last_update_id_ = 0;

    // Statistics
    unsigned int commands_processed_ = 0;
    unsigned int messages_sent_ = 0;
    unsigned int messages_failed_ = 0;
    String last_error_;

    // Command registry - fixed-size to avoid heap fragmentation
    static constexpr size_t MAX_COMMANDS = 12;
    struct CommandEntry {
        String command;       // e.g. "/status"
        String description;   // e.g. "Show system status"
        TelegramCommandCallback callback;
    };
    CommandEntry commands_[MAX_COMMANDS];
    size_t command_count_ = 0;

    void pollUpdates();
    void processMessage(const JsonObject& message);
    void dispatchCommand(const String& command, const String& args);
    String buildHelpMessage() const;
};

#endif // __TELEGRAM_BOT_H__
