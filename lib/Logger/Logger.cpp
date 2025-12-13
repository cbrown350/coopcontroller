#include "Logger.h"
#include "config.h"

#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cassert>

#include <stdarg.h>


Logger &Logger::getInstance(IHAL* hal)
{
  static Logger instance; // NOSONAR - must construct Logger here instead of inline since constructed UUID uses hal in constructor
  if(hal != nullptr) {
    instance.hal = hal;
    instance.init();
  }
  assert(instance.hal != nullptr && "IHAL pointer must be provided on first getInstance() call");
  return instance;
}

void Logger::init()
{
  currentIndex = 0;
  totalEntries = 0;
  uuidGenerator.generate();
  currentLogLevel_ = stringToLogLevel(DEFAULT_LOGLEVEL);
  
  logInfo(String("Initializing SysLog to send logs to ") + String(syslogServer) + String(":") + String(syslogPort));
  if (!(syslogServer == nullptr || strlen(syslogServer) == 0 || syslogPort == nullptr || strlen(syslogPort) == 0)) 
  {
    syslog = new SimpleSyslog(hostName, "CoopController", syslogServer, (uint16_t)atoi(syslogPort), 400); // NOSONAR, packet size 400 bytes
    logInfo("Syslog initialized");
  } else
  {
    logInfo("Syslog not configured");
  }
}

Logger::~Logger()
{
  if (syslog != nullptr) 
  {
    delete syslog; // NOSONAR
  }
  hal = nullptr;
}

void Logger::logWithLevel(const String &message, LogLevel level) const
{
    // Check if this level should be logged
    if (static_cast<int>(level) < static_cast<int>(currentLogLevel_)) {
        return; // Filter out messages below current log level
    }

    unsigned long timestamp = hal->getTime();
    
    // Create level prefix
    String levelPrefix = "[" + String(logLevelToString(level)) + "] ";
    String fullMessage = levelPrefix + message;
    
    // Print to serial with timestamp and level
    hal->SerialPrintf("[%lu] %s\n", timestamp, fullMessage.c_str());

    if (syslog != nullptr && hal->WiFiIsConnected())
      syslog->printf(FAC_USER, PRI_DEBUG, const_cast<char*>("([%lu] %s)"), timestamp, fullMessage.c_str()); // NOSONAR

    // Generate UUID for this log entry
    uuidGenerator.generate();
    const char* uuidChars = uuidGenerator.toCharArray();
    auto uuid = String(uuidChars);

    // Store in circular buffer
    logBuffer[currentIndex].uuid = uuid;
    logBuffer[currentIndex].timestamp = timestamp;
    logBuffer[currentIndex].message = fullMessage;
    logBuffer[currentIndex].level = level;

    // Update indices
    currentIndex = (currentIndex + 1) % MAX_LOG_ENTRIES;
    if (totalEntries < MAX_LOG_ENTRIES)
    {
        totalEntries++;
    }
}

String Logger::logLevelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::VERBOSE: return "VERBOSE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

LogLevel Logger::stringToLogLevel(const String &levelStr, uint depth) const
{
    if (levelStr == "VERBOSE") return LogLevel::VERBOSE;
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if(depth > 0) {
        logWarning(String("Unknown log level string '") + levelStr + String("', defaulting to INFO"));
        return LogLevel::INFO; // prevent infinite recursion
    }
    logWarning(String("Unknown log level string '") + levelStr + String("', attempting default ") + DEFAULT_LOGLEVEL);
    return stringToLogLevel(DEFAULT_LOGLEVEL, depth+1); 
}

void Logger::setLogLevel(LogLevel level)
{
  currentLogLevel_ = level;
  logInfo(String("Log level set to ") + logLevelToString(level));
}

LogLevel Logger::getLogLevel() const
{
    return currentLogLevel_;
}

void Logger::logVerbose(const String &message) const
{
    logWithLevel(message, LogLevel::VERBOSE);
}

void Logger::logDebug(const String &message) const
{
    logWithLevel(message, LogLevel::DEBUG);
}

void Logger::logInfo(const String &message) const
{
    logWithLevel(message, LogLevel::INFO);
}

void Logger::logWarning(const String &message) const
{
    logWithLevel(message, LogLevel::WARNING);
}

void Logger::logError(const String &message) const
{
    logWithLevel(message, LogLevel::ERROR);
}

void Logger::logfVerbose(const char *format, ...) const // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    logVerbose(String(buffer));
}

void Logger::logfDebug(const char *format, ...) const // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    logDebug(String(buffer));
}

void Logger::logfInfo(const char *format, ...) const // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    logInfo(String(buffer));
}

void Logger::logfWarning(const char *format, ...) const // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    logWarning(String(buffer));
}

void Logger::logfError(const char *format, ...) const // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    logError(String(buffer));
}

String Logger::getLogsAsJson() const
{
  logger.logDebug(String("Free heap before JSON log: ") + String(hal->getFreeHeap()) + " bytes");

  JsonDocument jsonDoc;
  JsonArray logsArray = jsonDoc["logs"].to<JsonArray>();

  // If we have less than MAX_LOG_ENTRIES, start from 0
  // Otherwise, start from currentIndex (oldest entry)
  int startIndex = (totalEntries < MAX_LOG_ENTRIES) ? 0 : currentIndex;
  int count = totalEntries;

  for (int i = 0; i < count; i++)
  {
    int bufferIndex = (startIndex + i) % MAX_LOG_ENTRIES;

    JsonObject logEntry = logsArray.add<JsonObject>();
    logEntry["uuid"] = logBuffer[bufferIndex].uuid;
    logEntry["timestamp"] = logBuffer[bufferIndex].timestamp;
    logEntry["message"] = logBuffer[bufferIndex].message;
    logEntry["level"] = logLevelToString(logBuffer[bufferIndex].level);
  }
  
  if (jsonDoc.overflowed()) {
    logWarning("JSON document overflowed - logs may be truncated");
    return R"({"error":"JSON overflow","logs":[]})";
  }

  String jsonResponse;
  serializeJson(jsonDoc, jsonResponse);
  
  logger.logDebug(String("JSON log response entries: ") + String(totalEntries) + ", size: " + String(jsonResponse.length()) + " bytes, free heap: " + String(hal->getFreeHeap()) + " bytes");
  return jsonResponse;
}

void Logger::clearLogs()
{
  currentIndex = 0;
  totalEntries = 0;
  // Clear the buffer
  for (int i = 0; i < MAX_LOG_ENTRIES; i++) // NOSONAR
  {
    logBuffer[i].uuid = "";
    logBuffer[i].timestamp = 0;
    logBuffer[i].message = "";
  }
}

int Logger::getLogCount() const
{
  return totalEntries;
}