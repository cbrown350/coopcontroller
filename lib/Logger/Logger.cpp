#include "Logger.h"
#include "config.h"

#include "IHAL.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cassert>

#include <stdarg.h>
#include <stdint.h>


Logger &Logger::getInstance()
{
  static Logger instance; // NOSONAR - must construct Logger here instead of inline since constructed UUID uses hal in constructor
  return instance;
}

void Logger::begin(IHAL* _hal)
{
  hal = _hal;
  
  currentIndex = 0;
  totalEntries = 0;
  uuidGenerator.generate();
  currentLogLevel_ = stringToLogLevel(DEFAULT_LOGLEVEL);
  
  logfInfo("Initializing SysLog to send logs to %s:%s", syslogServer ? syslogServer : "(null)", syslogPort ? syslogPort : "(null)");
  if (!(syslogServer == nullptr || strlen(syslogServer) == 0 || syslogPort == nullptr || strlen(syslogPort) == 0)) 
  {
    syslog = new SimpleSyslog(hostName, "CoopController", syslogServer, (uint16_t)atoi(syslogPort), 400); // NOSONAR, packet size 400 bytes
    logInfo("Syslog initialized");
  } else
  {
    logInfo("Syslog not configured");
  }
}

void Logger::reconfigureSyslog(const String& server, int port, const char* hostname)
{
  // Hold the log mutex: this swaps the syslog pointer that logWithLevel()
  // dereferences on another core. Without the lock a concurrent log could use
  // a syslog client mid-delete (use-after-free).
  std::lock_guard<std::recursive_mutex> guard(logMutex_);

  // Destroy existing syslog client
  if (syslog != nullptr) {
    delete syslog; // NOSONAR
    syslog = nullptr;
  }

  if (server.length() > 0 && port > 0) {
    syslogServerStr_ = server;
    syslogHostnameStr_ = hostname;
    syslog = new SimpleSyslog(syslogHostnameStr_.c_str(), "CoopController", syslogServerStr_.c_str(), (uint16_t)port, 400); // NOSONAR
    logfInfo("Syslog reconfigured: %s:%d", server.c_str(), port);
  } else {
    logInfo("Syslog disabled (no server configured)");
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
    assert(hal != nullptr && "IHAL pointer must be provided in Logger::begin(&hal) call");

    // Check if this level should be logged
    if (static_cast<int>(level) < static_cast<int>(currentLogLevel_)) {
        return; // Filter out messages below current log level
    }

    // Serialize the whole emission (serial print, syslog UDP send, and circular
    // buffer write) across cores/tasks. See logMutex_ in Logger.h for why: the
    // shared SimpleSyslog WiFiUDP tx_buffer and the buffer indices are not
    // safe for concurrent use from the loop task (core 1) and async web
    // handlers (async_tcp, core 0). Recursive so a nested log (e.g. a warning
    // emitted from within stringToLogLevel) does not deadlock.
    std::lock_guard<std::recursive_mutex> guard(logMutex_);

    unsigned long timestamp = hal->getTime();
    
    // Create level prefix
    String levelPrefix = "[" + String(logLevelToString(level)) + "] ";
    String fullMessage = levelPrefix + message;
    const char* msgCStr = fullMessage.c_str();
    if (msgCStr == nullptr) msgCStr = "[LOG_ALLOC_FAIL]";

    // Print to serial with timestamp and level. Serial output and the circular
    // buffer below are ALWAYS written in full — only the syslog UDP fan-out is
    // rate-limited, so no log line is ever lost locally.
    hal->SerialPrintf("[%lu] %s\n", timestamp, msgCStr);

    if (syslog != nullptr && hal->WiFiIsConnected()) {
      // Rate-limit outbound syslog UDP to at most one packet per
      // SYSLOG_MIN_SEND_INTERVAL_MS. A burst of log lines (e.g. per-loop sensor
      // wiring-error spam, amplified when a TLS connect fails) would otherwise
      // fire one WiFiUDP send per line; under buffer pressure those fail with
      // ENOMEM and starve lwIP until AsyncTCP can't accept HTTP (soft wedge —
      // see SYSLOG_MIN_SEND_INTERVAL_MS). Throttling decouples log rate from
      // send rate and breaks that amplification loop. Uses millis() (monotonic,
      // wraparound-safe via unsigned subtraction), independent of NTP time.
      unsigned long nowMs = millis();
      bool sendNow = !syslogSentOnce_ ||
                     (nowMs - lastSyslogSendMs_) >= SYSLOG_MIN_SEND_INTERVAL_MS;
      if (sendNow) {
        // If sends were suppressed since the last one, note how many so the
        // remote log shows the gap instead of silently dropping lines.
        if (syslogSuppressedCount_ > 0) {
          syslog->printf(FAC_USER, PRI_DEBUG, // NOSONAR
                         const_cast<char*>("([%lu] [INFO] %u syslog msgs suppressed (rate limit))"),
                         timestamp, (unsigned)syslogSuppressedCount_);
          syslogSuppressedCount_ = 0;
        }
        syslog->printf(FAC_USER, PRI_DEBUG, const_cast<char*>("([%lu] %s)"), timestamp, msgCStr); // NOSONAR
        lastSyslogSendMs_ = nowMs;
        syslogSentOnce_ = true;
      } else {
        syslogSuppressedCount_++;
      }
    }

    // Generate UUID for this log entry
    uuidGenerator.generate();
    const char* uuidChars = uuidGenerator.toCharArray();

    // Store in circular buffer (fixed char arrays - no heap allocation)
    strncpy(logBuffer[currentIndex].uuid, uuidChars, sizeof(logBuffer[currentIndex].uuid) - 1);
    logBuffer[currentIndex].uuid[sizeof(logBuffer[currentIndex].uuid) - 1] = '\0';
    logBuffer[currentIndex].timestamp = timestamp;
    strncpy(logBuffer[currentIndex].message, msgCStr, sizeof(logBuffer[currentIndex].message) - 1);
    logBuffer[currentIndex].message[sizeof(logBuffer[currentIndex].message) - 1] = '\0';
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

LogLevel Logger::stringToLogLevel(const String &levelStr, unsigned int depth) const
{
    if (levelStr == "VERBOSE") return LogLevel::VERBOSE;
    if (levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "ERROR") return LogLevel::ERROR;
    if(depth > 0) {
        logfWarning("Unknown log level string '%s', defaulting to INFO", levelStr.c_str());
        return LogLevel::INFO; // prevent infinite recursion
    }
    logfWarning("Unknown log level string '%s', attempting default %s", levelStr.c_str(), DEFAULT_LOGLEVEL);
    return stringToLogLevel(DEFAULT_LOGLEVEL, depth+1); 
}

void Logger::setLogLevel(LogLevel level)
{
  currentLogLevel_ = level;
  logfInfo("Log level set to %s", logLevelToString(level).c_str());
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
  // Defensive check: ensure HAL is initialized
  if (hal == nullptr) {
    return R"({"error":"Logger not initialized","logs":[]})";
  }

  logger.logfDebug("Free heap before JSON log: %u bytes", hal->getFreeHeap());

  // Runs on the async web task while the main loop may be appending entries.
  // Hold the log mutex for the buffer read so we never serialize a half-written
  // entry. Recursive mutex: the logfDebug calls in this method re-enter safely.
  std::lock_guard<std::recursive_mutex> guard(logMutex_);

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
  size_t serializedSize = serializeJson(jsonDoc, jsonResponse);

  // Defensive check: ensure serialization produced valid output
  if (serializedSize == 0 || jsonResponse.length() == 0) {
    return R"({"error":"JSON serialization failed","logs":[]})";
  }

  logger.logfDebug("JSON log response entries: %d, size: %u bytes, free heap: %u bytes", totalEntries, jsonResponse.length(), hal->getFreeHeap());
  return jsonResponse;
}

void Logger::clearLogs()
{
  // Mutates the shared buffer that logWithLevel() writes from other tasks.
  std::lock_guard<std::recursive_mutex> guard(logMutex_);
  currentIndex = 0;
  totalEntries = 0;
  // Clear the buffer
  for (int i = 0; i < MAX_LOG_ENTRIES; i++) // NOSONAR
  {
    logBuffer[i].uuid[0] = '\0';
    logBuffer[i].timestamp = 0;
    logBuffer[i].message[0] = '\0';
  }
}

int Logger::getLogCount() const
{
  std::lock_guard<std::recursive_mutex> guard(logMutex_);
  return totalEntries;
}

LogEntry Logger::getLogEntryAt(int index) const
{
  // Return a COPY taken under the lock so the caller reads a stable snapshot
  // even if the main loop overwrites this slot immediately afterward. See the
  // header for why a reference would be unsafe here.
  std::lock_guard<std::recursive_mutex> guard(logMutex_);
  return logBuffer[index];
}

int Logger::getStartIndex() const
{
  std::lock_guard<std::recursive_mutex> guard(logMutex_);
  return (totalEntries < MAX_LOG_ENTRIES) ? 0 : currentIndex;
}
