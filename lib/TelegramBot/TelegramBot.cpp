#include "TelegramBot.h"
#include "Logger.h"

void TelegramBot::begin(IHAL* hal) {
    hal_ = hal;
    logger.logInfo("TelegramBot initialized");
}

TelegramSendResult TelegramBot::sendMessage(const String& message) {
    if (!hal_) {
        return {false, "HAL not initialized"};
    }
    if (!enabled_ || !isConfigured()) {
        return {false, "Telegram not configured"};
    }
    if (!hal_->WiFiIsConnected()) {
        return {false, "WiFi not connected"};
    }

    String url = "https://api.telegram.org/bot" + bot_token_ + "/sendMessage";

    JsonDocument doc;
    doc["chat_id"] = chat_id_;
    doc["text"] = message;
    doc["parse_mode"] = "Markdown";

    String payload;
    serializeJson(doc, payload);

    String response = hal_->httpPost(url, payload, 10000);
    if (response.length() == 0) {
        messages_failed_++;
        last_error_ = "No response from Telegram API";
        return {false, last_error_};
    }

    JsonDocument respDoc;
    if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
        if (respDoc["ok"].as<bool>()) {
            messages_sent_++;
            return {true, ""};
        } else {
            String desc = respDoc["description"].as<String>();
            messages_failed_++;
            last_error_ = desc;
            return {false, desc};
        }
    }

    // Got a response but couldn't parse - assume success
    messages_sent_++;
    return {true, ""};
}

void TelegramBot::onCommand(const String& command, const String& description, TelegramCommandCallback callback) {
    if (command_count_ >= MAX_COMMANDS) {
        logger.logError("TelegramBot: command registry full");
        return;
    }
    commands_[command_count_].command = command;
    commands_[command_count_].description = description;
    commands_[command_count_].callback = std::move(callback);
    command_count_++;
}

void TelegramBot::update() {
    if (!hal_ || !enabled_ || !polling_enabled_ || !isConfigured()) {
        return;
    }
    if (!hal_->WiFiIsConnected()) {
        return;
    }

    unsigned long now = hal_->millis();
    if (last_poll_time_ > 0 && (now - last_poll_time_) < polling_interval_ms_) {
        return;
    }
    last_poll_time_ = now;

    pollUpdates();
}

void TelegramBot::pollUpdates() {
    String url = "https://api.telegram.org/bot" + bot_token_ + "/getUpdates?offset="
                 + String(last_update_id_ + 1)
                 + "&limit=5&timeout=0&allowed_updates=[\"message\"]";

    String response = hal_->httpGet(url, 10000);
    if (response.length() == 0) {
        return; // Silent fail - transient network issue
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err != DeserializationError::Ok) {
        logger.logfDebug("TelegramBot: JSON parse error: %s", err.c_str());
        return;
    }

    if (!doc["ok"].as<bool>()) {
        return;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    for (JsonObject update : results) {
        long updateId = update["update_id"].as<long>();
        if (updateId > last_update_id_) {
            last_update_id_ = updateId;
        }

        if (update["message"].is<JsonObject>()) {
            processMessage(update["message"].as<JsonObject>());
        }
    }
}

void TelegramBot::processMessage(const JsonObject& message) {
    // Security: only respond to messages from the configured chat_id
    long long chatIdNum = message["chat"]["id"].as<long long>();
    char chatIdBuf[21];
    snprintf(chatIdBuf, sizeof(chatIdBuf), "%lld", chatIdNum);
    String fromChatId = chatIdBuf;
    if (fromChatId != chat_id_) {
        logger.logfDebug("TelegramBot: ignoring message from chat %s", fromChatId.c_str());
        return;
    }

    String text = message["text"].as<String>();
    if (text.length() == 0 || text[0] != '/') {
        return; // Not a command
    }

    // Parse command and args: "/door open" -> command="/door", args="open"
    String command;
    String args;
    int spaceIdx = text.indexOf(' ');
    if (spaceIdx > 0) {
        command = text.substring(0, spaceIdx);
        args = text.substring(spaceIdx + 1);
        args.trim();
    } else {
        command = text;
    }

    // Strip @botname suffix (e.g. "/status@MyCoopBot" -> "/status")
    int atIdx = command.indexOf('@');
    if (atIdx > 0) {
        command = command.substring(0, atIdx);
    }

    command.toLowerCase();

    dispatchCommand(command, args);
}

void TelegramBot::dispatchCommand(const String& command, const String& args) {
    // Built-in /help command
    if (command == "/help") {
        sendMessage(buildHelpMessage());
        commands_processed_++;
        return;
    }

    for (size_t i = 0; i < command_count_; i++) {
        if (commands_[i].command == command) {
            String response = commands_[i].callback(args);
            if (response.length() > 0) {
                sendMessage(response);
            }
            commands_processed_++;
            return;
        }
    }

    sendMessage("Unknown command. Send /help for available commands.");
}

String TelegramBot::buildHelpMessage() const {
    String msg = "*Coop Controller Commands*\n\n";
    for (size_t i = 0; i < command_count_; i++) {
        msg += commands_[i].command + " - " + commands_[i].description + "\n";
    }
    msg += "/help - Show this help message";
    return msg;
}

void TelegramBot::toJson(JsonObject& json) const {
    json["telegram_enabled"] = enabled_;
    json["telegram_configured"] = isConfigured();
    json["telegram_polling_enabled"] = polling_enabled_;
    json["telegram_polling_interval_ms"] = polling_interval_ms_;
    json["telegram_commands_processed"] = commands_processed_;
    json["telegram_messages_sent"] = messages_sent_;
    json["telegram_messages_failed"] = messages_failed_;
    if (last_error_.length() > 0) {
        json["telegram_last_error"] = last_error_;
    }
}
