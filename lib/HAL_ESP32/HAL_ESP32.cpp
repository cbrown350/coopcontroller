#include "HAL_ESP32.h"

#include <cstddef>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>

#include "Logger.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "esp32-hal-ledc.h"


void HAL_ESP32::SerialPrintf(const char* format, ...) // NOSONAR
{
    char buffer[512]; // NOSONAR
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // NOSONAR
    va_end(args);
    Serial.print(String(buffer));
}

void HAL_ESP32::SerialPrintln(const char* message)
{
    Serial.println(message);
}

void HAL_ESP32::SerialPrint(const char* message)
{
    Serial.print(message);
}

bool HAL_ESP32::begin() {
    return initFilesystem();
}

bool HAL_ESP32::initFilesystem() { // NOSONAR - not const, initializes filesystem
    
    // Initialize filesystem first so settings can be loaded
    if (!LittleFS.begin(true)) {  // The 'true' parameter formats the filesystem if it fails to mount
        logger.logWarning("Failed to mount LittleFS filesystem, formatting...");
        if (!LittleFS.begin(true)) {
            logger.logError("Failed to initialize LittleFS even after formatting");
            // Continue without filesystem - settings will use defaults
        } else {
            logger.logInfo("LittleFS filesystem initialized after formatting");
        }
    } else {
        logger.logInfo("LittleFS filesystem initialized");
    }
    return true;
}

unsigned long HAL_ESP32::getTime()
{
    time_t now;
    if (struct tm timeinfo; !getLocalTime(&timeinfo, 200)) { // Add timeout to prevent hanging
        if (static unsigned long warned = 0; millis() - warned > 30000 || warned == 0) { // Warn at most once per X
            warned = millis();
            Serial.println("Failed to obtain time"); // Can't call logger since it may call getTime()
        }
        return millis();
    }
    time(&now);
    return now;
}

uint32_t HAL_ESP32::getFreeHeap()
{
    return ESP.getFreeHeap();
}

bool HAL_ESP32::WiFiIsConnected()
{
    return WiFi.isConnected();
}
