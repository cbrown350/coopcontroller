#ifndef __NOTIFICATION_MANAGER_H__
#define __NOTIFICATION_MANAGER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IHAL.h"
#include "BuzzerController.h"

/**
 * @brief Notification channel types
 */
enum class NotificationChannel : uint8_t {
    EMAIL = 0,
    TELEGRAM = 1
};

/**
 * @brief Result of a notification send attempt
 */
struct NotificationResult {
    bool success;
    String error_message;
};

/**
 * @brief Notification manager for email and Telegram alerts
 *
 * Sends notifications via email (SMTP) and Telegram Bot API when
 * system alerts are triggered. Integrates with BuzzerController's
 * AlertType system to forward alerts to configured channels.
 *
 * Features:
 * - Telegram Bot API notifications
 * - Email notifications via external SMTP relay
 * - Configurable per-alert-type notification preferences
 * - Rate limiting to prevent notification floods
 * - Test notification endpoints
 */
class NotificationManager {
public:
    NotificationManager() = default;

    /**
     * @brief Initialize the notification manager
     * @param hal Pointer to hardware abstraction layer (for HTTP client)
     */
    void begin(IHAL* hal);

    /**
     * @brief Process pending notifications (call in main loop)
     *
     * Handles rate limiting and queued notification delivery.
     */
    void update();

    /**
     * @brief Send a notification for a system alert
     *
     * Called when BuzzerController triggers an alert. Sends to all
     * enabled notification channels.
     *
     * @param alertType The type of alert that was triggered
     * @param message Human-readable alert message
     */
    void notify(AlertType alertType, const String& message);

    /**
     * @brief Send a test notification to verify configuration
     * @param channel Which channel to test
     * @return Result with success/failure and error details
     */
    NotificationResult sendTest(NotificationChannel channel);

    /**
     * @brief Send a daily status report
     * @param statusJson JSON string with current system status
     */
    void sendDailyReport(const String& statusJson);

    /**
     * @brief Serialize notification status to JSON
     * @param json JsonObject to populate
     */
    void toJson(JsonObject& json) const;

    // Telegram configuration
    void setTelegramEnabled(bool enabled) { telegram_enabled_ = enabled; }
    bool getTelegramEnabled() const { return telegram_enabled_; }
    void setTelegramBotToken(const String& token) { telegram_bot_token_ = token; }
    String getTelegramBotToken() const { return telegram_bot_token_; }
    void setTelegramChatId(const String& chatId) { telegram_chat_id_ = chatId; }
    String getTelegramChatId() const { return telegram_chat_id_; }

    // Email configuration
    void setEmailEnabled(bool enabled) { email_enabled_ = enabled; }
    bool getEmailEnabled() const { return email_enabled_; }
    void setSmtpServer(const String& server) { smtp_server_ = server; }
    String getSmtpServer() const { return smtp_server_; }
    void setSmtpPort(uint16_t port) { smtp_port_ = port; }
    uint16_t getSmtpPort() const { return smtp_port_; }
    void setSmtpUsername(const String& username) { smtp_username_ = username; }
    String getSmtpUsername() const { return smtp_username_; }
    void setSmtpPassword(const String& password) { smtp_password_ = password; }
    String getSmtpPassword() const { return smtp_password_; }
    void setEmailFrom(const String& from) { email_from_ = from; }
    String getEmailFrom() const { return email_from_; }
    void setEmailTo(const String& to) { email_to_ = to; }
    String getEmailTo() const { return email_to_; }

    // Alert type notification preferences
    void setNotifyOnPumpError(bool enabled) { notify_pump_error_ = enabled; }
    bool getNotifyOnPumpError() const { return notify_pump_error_; }
    void setNotifyOnSensorError(bool enabled) { notify_sensor_error_ = enabled; }
    bool getNotifyOnSensorError() const { return notify_sensor_error_; }
    void setNotifyOnDoorFault(bool enabled) { notify_door_fault_ = enabled; }
    bool getNotifyOnDoorFault() const { return notify_door_fault_; }
    void setNotifyOnWifiDisconnect(bool enabled) { notify_wifi_disconnect_ = enabled; }
    bool getNotifyOnWifiDisconnect() const { return notify_wifi_disconnect_; }
    void setNotifyOnSystemError(bool enabled) { notify_system_error_ = enabled; }
    bool getNotifyOnSystemError() const { return notify_system_error_; }

    // Statistics
    unsigned long getLastNotificationTime() const { return last_notification_time_; }
    unsigned int getTotalSent() const { return total_sent_; }
    unsigned int getTotalFailed() const { return total_failed_; }
    String getLastError() const { return last_error_; }

private:
    IHAL* hal_ = nullptr;

    // Telegram settings
    bool telegram_enabled_ = false;
    String telegram_bot_token_;
    String telegram_chat_id_;

    // Email settings
    bool email_enabled_ = false;
    String smtp_server_;
    uint16_t smtp_port_ = 587;
    String smtp_username_;
    String smtp_password_;
    String email_from_;
    String email_to_;

    // Alert type notification preferences (defaults: critical alerts enabled)
    bool notify_pump_error_ = true;
    bool notify_sensor_error_ = true;
    bool notify_door_fault_ = true;
    bool notify_wifi_disconnect_ = false;
    bool notify_system_error_ = true;

    // Rate limiting
    static constexpr unsigned long MIN_NOTIFICATION_INTERVAL_MS = 60000; // 1 minute between same alert type
    unsigned long last_notification_times_[8] = {}; // Per AlertType

    // Statistics
    unsigned long last_notification_time_ = 0;
    unsigned int total_sent_ = 0;
    unsigned int total_failed_ = 0;
    String last_error_;

    /**
     * @brief Check if alert type should trigger a notification
     */
    bool shouldNotify(AlertType alertType) const;

    /**
     * @brief Check rate limit for alert type
     */
    bool isRateLimited(AlertType alertType) const;

    /**
     * @brief Send a Telegram message
     */
    NotificationResult sendTelegram(const String& message);

    /**
     * @brief Send an email via SMTP relay API
     */
    NotificationResult sendEmail(const String& subject, const String& body);

    /**
     * @brief Format alert message for notifications
     */
    String formatAlertMessage(AlertType alertType, const String& message) const;

    /**
     * @brief Get emoji for alert type (Telegram)
     */
    String getAlertEmoji(AlertType alertType) const;
};

#endif // __NOTIFICATION_MANAGER_H__
