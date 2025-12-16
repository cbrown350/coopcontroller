#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "IHAL.h"

#include <Arduino.h> // Requires ArduinoFake to mock in tests
#include <SimpleSyslog.h> // Requires ArduinoFake to mock in tests
#include <UUID.h> // Requires ArduinoFake to mock in tests


enum class LogLevel
{
    VERBOSE,
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

struct LogEntry
{
  String uuid;
  unsigned long timestamp;
  String message;
  LogLevel level;
};

class Logger
{
private:
  static Logger instance;
  IHAL* hal;
  static const int MAX_LOG_ENTRIES = 150;
  mutable LogEntry logBuffer[MAX_LOG_ENTRIES]; // NOSONAR
  mutable int currentIndex;
  mutable int totalEntries;
  mutable UUID uuidGenerator;

  SimpleSyslog* syslog;

  LogLevel currentLogLevel_;

  Logger() = default;
  ~Logger();

  // Delete copy constructor and assignment operator
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  // private internal helpers
  void logWithLevel(const String &message, LogLevel level) const;

public:
  void begin(IHAL* ihal);
  // Singleton access method
  static Logger &getInstance();

  String getLogsAsJson() const;
  void clearLogs();
  int getLogCount() const;

  // Log level management
  void setLogLevel(LogLevel level);
  LogLevel getLogLevel() const;
  String logLevelToString(LogLevel level) const;
  LogLevel stringToLogLevel(const String &levelStr, unsigned int depth = 0) const;

  // Public level-specific methods (non-formatted) - const // NOSONAR
  void logVerbose(const String &message) const;
  void logDebug(const String &message) const;
  void logInfo(const String &message) const;
  void logWarning(const String &message) const;
  void logError(const String &message) const;

  // Public level-specific formatted methods - const
  void logfVerbose(const char *format, ...) const;
  void logfDebug(const char *format, ...) const;
  void logfInfo(const char *format, ...) const;
  void logfWarning(const char *format, ...) const;
  void logfError(const char *format, ...) const;
};

// Convenience macro for easier access
#define logger Logger::getInstance()


#endif // __LOGGER_H__