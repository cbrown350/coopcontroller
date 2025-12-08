#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <UUID.h>
#include <SimpleSyslog.h>

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
  static const int MAX_LOG_ENTRIES = 150;
  mutable LogEntry logBuffer[MAX_LOG_ENTRIES];
  mutable int currentIndex;
  mutable int totalEntries;
  mutable UUID uuidGenerator;

  SimpleSyslog* syslog;

  LogLevel currentLogLevel_;

  Logger();
  ~Logger();

  // Delete copy constructor and assignment operator
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  // private internal helpers
  void logWithLevel(const String &message, LogLevel level) const;

public:
  // Singleton access method
  static Logger &getInstance();

  String getLogsAsJson() const;
  void clearLogs();
  int getLogCount() const;

  // Log level management
  void setLogLevel(LogLevel level);
  LogLevel getLogLevel() const;
  String logLevelToString(LogLevel level) const;

  // Public level-specific methods (non-formatted) - const
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