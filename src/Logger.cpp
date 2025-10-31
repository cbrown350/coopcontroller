#include "Logger.h"
#include "time.h"
#include <cstdlib>

#include "SettingsManager.h"

#include <WiFi.h>


extern const char* hostName;
extern const char* syslogServer;
extern const char* syslogPort;

// External function to get current time (from main.cpp)
extern unsigned long getTime();

Logger &Logger::getInstance()
{
  static Logger instance;
  return instance;
}

Logger::Logger()
{
  currentIndex = 0;
  totalEntries = 0;
  uuidGenerator.generate();
  
  log("Initializing SysLog to send logs to " + String(syslogServer) + ":" + String(syslogPort));
  if (!(syslogServer == nullptr || strlen(syslogServer) == 0 || syslogPort == nullptr || strlen(syslogPort) == 0)) 
  {
    syslog = new SimpleSyslog(hostName, "CoopController", syslogServer, atoi(syslogPort), 400); // packet size 400 bytes
    log("Syslog initialized");
  } else
  {
    log("Syslog not configured");
  }
}

Logger::~Logger()
{
  if (syslog != nullptr) 
  {
    delete syslog;
  }
}

void Logger::log(const String &message)
{
    unsigned long timestamp = getTime();
    
    // Print to serial with timestamp
    Serial.printf("[%lu] %s\n", timestamp, message.c_str());

    if (syslog != nullptr && WiFi.isConnected())
      syslog->printf(FAC_USER, PRI_DEBUG, (char*) "[%lu] %s", timestamp, message.c_str());

    // Generate UUID for this log entry
    uuidGenerator.generate();
    char* uuidChars = uuidGenerator.toCharArray();
    String uuid = String(uuidChars);

    // Store in circular buffer
    logBuffer[currentIndex].uuid = uuid;
    logBuffer[currentIndex].timestamp = timestamp;
    logBuffer[currentIndex].message = message;

    // Update indices
    currentIndex = (currentIndex + 1) % MAX_LOG_ENTRIES;
    if (totalEntries < MAX_LOG_ENTRIES)
    {
        totalEntries++;
    }
}

void Logger::log(const char *message)
{
  log(String(message));
}

void Logger::logf(const char *format, ...)
{
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(String(buffer));
}

String Logger::getLogsAsJson() const
{
  if (settingsManager.getDebugEnabled())
  {
    size_t freeHeapBefore = ESP.getFreeHeap();
    logger.logf("Free heap before JSON log: %d bytes", freeHeapBefore);
  }

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
  }
  
  if (jsonDoc.overflowed()) {
    logger.log("JSON document overflowed - logs may be truncated");
    return "{\"error\":\"JSON overflow\",\"logs\":[]}";
  }

  String jsonResponse;
  serializeJson(jsonDoc, jsonResponse);
  
  if (settingsManager.getDebugEnabled())
    logger.logf("JSON log response entries: %d, size: %d bytes, free heap: %d bytes", totalEntries,
              jsonResponse.length(), ESP.getFreeHeap());
  return jsonResponse;
}

void Logger::clearLogs()
{
  currentIndex = 0;
  totalEntries = 0;
  // Clear the buffer
  for (int i = 0; i < MAX_LOG_ENTRIES; i++)
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