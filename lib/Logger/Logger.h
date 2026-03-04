#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "IHAL.h"

#include <Arduino.h> // Requires ArduinoFake to mock in tests
#include <SimpleSyslog.h> // Requires ArduinoFake to mock in tests
#include <UUID.h> // Requires ArduinoFake to mock in tests

/**
 * @brief Logging severity levels
 *
 * Defines the severity/priority levels for log messages.
 * Messages are only logged if their level is >= current log level.
 */
enum class LogLevel
{
    VERBOSE,  ///< Detailed information for debugging
    DEBUG,    ///< General debugging information
    INFO,     ///< informational messages
    WARNING,  ///< Warning messages for potential issues
    ERROR     ///< Error messages for failures and exceptions
};

/**
 * @brief Single log entry structure
 *
 * Contains all information for a single log message including
 * unique identifier, timestamp, message, and severity level.
 */
struct LogEntry
{
  char uuid[40];            ///< Unique identifier for this log entry (UUID ~36 chars + null)
  unsigned long timestamp;  ///< Timestamp when log entry was created
  char message[256];        ///< Log message content (truncated if longer)
  LogLevel level;           ///< Severity level of this log entry
};

/**
 * @brief System logging singleton
 *
 * Provides centralized logging for the chicken coop controller system.
 * Features include:
 *
 * - Singleton pattern for global access
 * - Multiple log levels (VERBOSE, DEBUG, INFO, WARNING, ERROR)
 * - Circular buffer for log entries (150 entries max)
 * - JSON export for web API
 * - Syslog integration for remote logging
 * - UUID generation for each log entry
 * - Printf-style formatted logging
 *
 * Usage:
 *   logger.logInfo("System started");
 *   logger.logfWarning("Temperature: %f°F", temp);
 */
class Logger
{
private:
  IHAL* hal;                                 ///< Hardware abstraction layer
  static const int MAX_LOG_ENTRIES = 150;    ///< Maximum number of log entries in buffer
  mutable LogEntry logBuffer[MAX_LOG_ENTRIES]; ///< Circular buffer for log entries
  mutable int currentIndex;                  ///< Current index in circular buffer
  mutable int totalEntries;                  ///< Total entries logged (for overflow detection)
  mutable UUID uuidGenerator;                ///< UUID generator for unique entry IDs

  SimpleSyslog* syslog;                      ///< Syslog client for remote logging
  String syslogServerStr_;                   ///< Persistent copy of syslog server (SimpleSyslog stores raw pointer)
  String syslogHostnameStr_;                 ///< Persistent copy of hostname (SimpleSyslog stores raw pointer)

  LogLevel currentLogLevel_;                 ///< Current minimum log level

  /**
   * @brief Private constructor for singleton
   */
  Logger() = default;

  /**
   * @brief Private destructor
   */
  ~Logger();

  // Delete copy constructor and assignment operator (singleton)
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  /**
   * @brief Internal logging helper
   *
   * Adds log entry to buffer and outputs to syslog if level is sufficient.
   *
   * @param message Log message content
   * @param level Severity level of this message
   */
  void logWithLevel(const String &message, LogLevel level) const;

public:
  /**
   * @brief Initialize logger with HAL interface
   *
   * Must be called before any logging operations.
   *
   * @param ihal Pointer to hardware abstraction layer
   */
  void begin(IHAL* ihal);

  /**
   * @brief Reconfigure syslog with new server/port at runtime
   *
   * Destroys existing syslog client and creates a new one with the
   * specified server and port. Pass empty server to disable syslog.
   *
   * @param server Syslog server address
   * @param port Syslog server port
   * @param hostname Device hostname for syslog identification
   */
  void reconfigureSyslog(const String& server, int port, const char* hostname);

  /**
   * @brief Get singleton instance
   *
   * Provides global access to logger instance.
   *
   * @return Reference to Logger singleton
   */
  static Logger &getInstance();

  /**
   * @brief Get all log entries as JSON string
   *
   * Serializes the entire log buffer to JSON format.
   *
   * @return JSON string containing all log entries
   */
  String getLogsAsJson() const;

  /**
   * @brief Clear all log entries
   *
   * Resets the log buffer to empty state.
   */
  void clearLogs();

  /**
   * @brief Get current number of log entries
   *
   * @return Number of entries in buffer (max 150)
   */
  int getLogCount() const;

  // ========================================================================
  // LOG LEVEL MANAGEMENT
  // ========================================================================

  /**
   * @brief Set minimum log level
   *
   * Only messages at or above this level will be logged.
   *
   * @param level Minimum log level to record
   */
  void setLogLevel(LogLevel level);

  /**
   * @brief Get current minimum log level
   *
   * @return Current minimum log level
   */
  LogLevel getLogLevel() const;

  /**
   * @brief Convert LogLevel to string
   *
   * @param level LogLevel to convert
   * @return String representation of level
   */
  String logLevelToString(LogLevel level) const;

  /**
   * @brief Convert string to LogLevel
   *
   * Parses string log level name to enum value.
   *
   * @param levelStr String representation of level
   * @param depth Recursion depth for internal use
   * @return Corresponding LogLevel
   */
  LogLevel stringToLogLevel(const String &levelStr, unsigned int depth = 0) const;

  // ========================================================================
  // LEVEL-SPECIFIC LOGGING METHODS (non-formatted)
  // ========================================================================

  /**
   * @brief Log VERBOSE level message
   *
   * @param message Message to log
   */
  void logVerbose(const String &message) const;

  /**
   * @brief Log DEBUG level message
   *
   * @param message Message to log
   */
  void logDebug(const String &message) const;

  /**
   * @brief Log INFO level message
   *
   * @param message Message to log
   */
  void logInfo(const String &message) const;

  /**
   * @brief Log WARNING level message
   *
   * @param message Message to log
   */
  void logWarning(const String &message) const;

  /**
   * @brief Log ERROR level message
   *
   * @param message Message to log
   */
  void logError(const String &message) const;

  // ========================================================================
  // LEVEL-SPECIFIC LOGGING METHODS (formatted)
  // ========================================================================

  /**
   * @brief Log VERBOSE level formatted message
   *
   * Printf-style formatting supported.
   *
   * @param format Format string
   * @param ... Variable arguments
   */
  void logfVerbose(const char *format, ...) const;

  /**
   * @brief Log DEBUG level formatted message
   *
   * Printf-style formatting supported.
   *
   * @param format Format string
   * @param ... Variable arguments
   */
  void logfDebug(const char *format, ...) const;

  /**
   * @brief Log INFO level formatted message
   *
   * Printf-style formatting supported.
   *
   * @param format Format string
   * @param ... Variable arguments
   */
  void logfInfo(const char *format, ...) const;

  /**
   * @brief Log WARNING level formatted message
   *
   * Printf-style formatting supported.
   *
   * @param format Format string
   * @param ... Variable arguments
   */
  void logfWarning(const char *format, ...) const;

  /**
   * @brief Log ERROR level formatted message
   *
   * Printf-style formatting supported.
   *
   * @param format Format string
   * @param ... Variable arguments
   */
  void logfError(const char *format, ...) const;
};

// Convenience macro for easier access
#define logger Logger::getInstance()


#endif // __LOGGER_H__