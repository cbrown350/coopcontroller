#include "NotificationManager.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>

void NotificationManager::begin(IHAL* hal) {
    hal_ = hal;
    logger.logInfo("NotificationManager initialized");
}

void NotificationManager::update() {
    // Currently no periodic work needed
    // Future: daily status reports could be triggered here
}

void NotificationManager::notify(AlertType alertType, const String& message) {
    if (!hal_ || !hal_->WiFiIsConnected()) {
        return;
    }

    if (!shouldNotify(alertType)) {
        return;
    }

    if (isRateLimited(alertType)) {
        logger.logDebug("Notification rate-limited for alert type");
        return;
    }

    String formattedMessage = formatAlertMessage(alertType, message);

    bool anySent = false;

    if (telegramBot_ && telegramBot_->getEnabled() && telegramBot_->isConfigured()) {
        TelegramSendResult result = telegramBot_->sendMessage(formattedMessage);
        if (result.success) {
            anySent = true;
            total_sent_++;
        } else {
            total_failed_++;
            last_error_ = "Telegram: " + result.error_message;
            logger.logError(("Telegram notification failed: " + result.error_message).c_str());
        }
    }

    if (email_enabled_ && smtp_server_.length() > 0 && email_to_.length() > 0) {
        String subject = "Coop Alert: " + String(BuzzerController().getAlertTypeString(alertType));
        NotificationResult result = sendEmail(subject, message);
        if (result.success) {
            anySent = true;
            total_sent_++;
        } else {
            total_failed_++;
            last_error_ = "Email: " + result.error_message;
            logger.logError(("Email notification failed: " + result.error_message).c_str());
        }
    }

    if (anySent) {
        last_notification_time_ = hal_->millis();
        last_notification_times_[static_cast<uint8_t>(alertType)] = hal_->millis();
    }
}

NotificationResult NotificationManager::sendTest(NotificationChannel channel) {
    if (!hal_) {
        return {false, "HAL not initialized"};
    }
    if (!hal_->WiFiIsConnected()) {
        return {false, "WiFi not connected"};
    }

    if (channel == NotificationChannel::TELEGRAM) {
        if (!telegramBot_ || !telegramBot_->isConfigured()) {
            return {false, "Telegram bot token or chat ID not configured"};
        }
        bool wasEnabled = telegramBot_->getEnabled();
        telegramBot_->setEnabled(true); // Temporarily enable for test
        TelegramSendResult tResult = telegramBot_->sendMessage("🐔 *Coop Controller Test*\nThis is a test notification from your chicken coop controller!");
        telegramBot_->setEnabled(wasEnabled);
        return {tResult.success, tResult.error_message};
    } else if (channel == NotificationChannel::EMAIL) {
        if (smtp_server_.length() == 0 || email_to_.length() == 0) {
            return {false, "SMTP server or recipient not configured"};
        }
        return sendEmail("Coop Controller Test", "This is a test notification from your chicken coop controller.");
    }

    return {false, "Unknown channel"};
}

NotificationResult NotificationManager::sendTestWithConfig(NotificationChannel channel, const JsonObject& config) {
    if (!hal_) {
        return {false, "HAL not initialized"};
    }
    if (!hal_->WiFiIsConnected()) {
        return {false, "WiFi not connected"};
    }

    if (channel == NotificationChannel::TELEGRAM) {
        String token = config["telegram_bot_token"] | (telegramBot_ ? telegramBot_->getBotToken() : String());
        String chatId = config["telegram_chat_id"] | (telegramBot_ ? telegramBot_->getChatId() : String());

        if (token.length() == 0 || chatId.length() == 0) {
            return {false, "Telegram bot token or chat ID not configured"};
        }

        // Use the provided (potentially unsaved) credentials directly via HAL
        String url = "https://api.telegram.org/bot" + token + "/sendMessage";
        JsonDocument doc;
        doc["chat_id"] = chatId;
        doc["text"] = "🐔 *Coop Controller Test*\nThis is a test notification from your chicken coop controller!";
        doc["parse_mode"] = "Markdown";
        String payload;
        serializeJson(doc, payload);

        String response = hal_->httpPost(url, payload, 10000);
        if (response.length() == 0) {
            return {false, "No response from Telegram API"};
        }

        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            if (respDoc["ok"].as<bool>()) {
                return {true, ""};
            } else {
                return {false, respDoc["description"].as<String>()};
            }
        }
        return {true, ""};
    } else if (channel == NotificationChannel::EMAIL) {
        String server = config["email_smtp_server"] | smtp_server_;
        uint16_t port = config["email_smtp_port"] | smtp_port_;
        String username = config["email_smtp_username"] | smtp_username_;
        String password = config["email_smtp_password"] | smtp_password_;
        String from = config["email_from"] | email_from_;
        String to = config["email_to"] | email_to_;

        if (server.length() == 0 || to.length() == 0) {
            return {false, "SMTP server or recipient not configured"};
        }
        return sendEmailWithConfig("Coop Controller Test",
                                   "This is a test notification from your chicken coop controller.",
                                   server, port, username, password, from, to);
    }

    return {false, "Unknown channel"};
}

void NotificationManager::sendDailyReport(const String& statusJson) {
    if (!hal_ || !hal_->WiFiIsConnected()) {
        return;
    }

    String message = "🐔 *Daily Coop Status Report*\n" + statusJson;

    if (telegramBot_ && telegramBot_->getEnabled() && telegramBot_->isConfigured()) {
        telegramBot_->sendMessage(message);
    }

    if (email_enabled_ && smtp_server_.length() > 0 && email_to_.length() > 0) {
        sendEmail("Daily Coop Status Report", statusJson);
    }
}

void NotificationManager::toJson(JsonObject& json) const {
    if (telegramBot_) {
        telegramBot_->toJson(json);
    } else {
        json["telegram_enabled"] = false;
        json["telegram_configured"] = false;
    }
    json["email_enabled"] = email_enabled_;
    json["email_configured"] = (smtp_server_.length() > 0 && email_to_.length() > 0);
    json["total_sent"] = total_sent_;
    json["total_failed"] = total_failed_;
    json["last_error"] = last_error_;
    json["last_notification_time"] = last_notification_time_;
}

bool NotificationManager::shouldNotify(AlertType alertType) const {
    switch (alertType) {
        case AlertType::PUMP_ERROR:       return notify_pump_error_;
        case AlertType::SENSOR_ERROR:     return notify_sensor_error_;
        case AlertType::DOOR_FAULT:       return notify_door_fault_;
        case AlertType::WIFI_DISCONNECTED: return notify_wifi_disconnect_;
        case AlertType::SYSTEM_ERROR:     return notify_system_error_;
        case AlertType::LOW_MEMORY:       return notify_system_error_;
        case AlertType::LIGHT_FAULT:      return notify_system_error_;
        case AlertType::TEST_ALERT:       return true;
        default:                          return false;
    }
}

bool NotificationManager::isRateLimited(AlertType alertType) const {
    if (!hal_) return true;
    uint8_t idx = static_cast<uint8_t>(alertType);
    if (idx >= 8) return true;
    unsigned long lastTime = last_notification_times_[idx];
    return (lastTime > 0 && (hal_->millis() - lastTime) < MIN_NOTIFICATION_INTERVAL_MS);
}

NotificationResult NotificationManager::sendEmail(const String& subject, const String& body) {
    String from = email_from_.length() > 0 ? email_from_ : "coop@controller.local";

    // Parse host and port from smtp_server_ which may contain:
    //   "hostname"  "hostname:port"  "https://hostname/path"  etc.
    String host = smtp_server_;
    uint16_t port = smtp_port_;

    // Reject URLs — this field expects an SMTP hostname, not an API URL
    if (host.startsWith("http://") || host.startsWith("https://")) {
        return {false, "Enter an SMTP hostname (e.g. smtp.gmail.com), not an API URL"};
    }

    // Extract port from host:port if present
    int colonIdx = host.indexOf(':');
    if (colonIdx >= 0) {
        port = host.substring(colonIdx + 1).toInt();
        host = host.substring(0, colonIdx);
    }

    String error = hal_->smtpSend(
        host, port,
        smtp_username_, smtp_password_,
        from, email_to_,
        subject, body,
        15000
    );

    if (error.length() > 0) {
        return {false, error};
    }

    logger.logInfo("Email notification sent");
    return {true, ""};
}

NotificationResult NotificationManager::sendEmailWithConfig(const String& subject, const String& body,
                                                           const String& server, uint16_t port,
                                                           const String& username, const String& password,
                                                           const String& from, const String& to) {
    String fromAddr = from.length() > 0 ? from : "coop@controller.local";
    String host = server;
    uint16_t usePort = port;

    if (host.startsWith("http://") || host.startsWith("https://")) {
        return {false, "Enter an SMTP hostname (e.g. smtp.gmail.com), not an API URL"};
    }

    int colonIdx = host.indexOf(':');
    if (colonIdx >= 0) {
        usePort = host.substring(colonIdx + 1).toInt();
        host = host.substring(0, colonIdx);
    }

    String error = hal_->smtpSend(host, usePort, username, password, fromAddr, to, subject, body, 15000);
    if (error.length() > 0) {
        return {false, error};
    }

    logger.logInfo("Email test notification sent");
    return {true, ""};
}

String NotificationManager::formatAlertMessage(AlertType alertType, const String& message) const {
    String emoji = getAlertEmoji(alertType);
    BuzzerController temp;
    String typeName = temp.getAlertTypeString(alertType);
    return emoji + " *Coop Alert: " + typeName + "*\n" + message;
}

String NotificationManager::getAlertEmoji(AlertType alertType) const {
    switch (alertType) {
        case AlertType::PUMP_ERROR:       return "💧";
        case AlertType::SENSOR_ERROR:     return "🌡️";
        case AlertType::WIFI_DISCONNECTED: return "📶";
        case AlertType::LOW_MEMORY:       return "🧠";
        case AlertType::DOOR_FAULT:       return "🚪";
        case AlertType::LIGHT_FAULT:      return "💡";
        case AlertType::SYSTEM_ERROR:     return "⚠️";
        case AlertType::TEST_ALERT:       return "🔔";
        default:                          return "❗";
    }
}
