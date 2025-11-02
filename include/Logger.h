#ifndef LOGGER_H
#define LOGGER_H

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
  LogEntry logBuffer[MAX_LOG_ENTRIES];
  int currentIndex;
  int totalEntries;
  UUID uuidGenerator;

  SimpleSyslog* syslog;

  LogLevel currentLogLevel_;

  Logger();
  ~Logger();

  // Delete copy constructor and assignment operator
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

public:
  // Singleton access method
  static Logger &getInstance();

  void log(const String &message);
  void log(const char *message);
  void logf(const char *format, ...);
  void logWithLevel(const String &message, LogLevel level);
  String getLogsAsJson() const;
  void clearLogs();
  int getLogCount() const;

  // New log level methods
  void setLogLevel(LogLevel level);
  LogLevel getLogLevel() const;
  String logLevelToString(LogLevel level) const;

  // Convenience methods for each log level
  void logVerbose(const String &message);
  void logDebug(const String &message);
  void logInfo(const String &message);
  void logWarning(const String &message);
  void logError(const String &message);

  void logfVerbose(const char *format, ...);
  void logfDebug(const char *format, ...);
  void logfInfo(const char *format, ...);
  void logfWarning(const char *format, ...);
  void logfError(const char *format, ...);
};

// Convenience macro for easier access
#define logger Logger::getInstance()

#endif // LOGGER_H