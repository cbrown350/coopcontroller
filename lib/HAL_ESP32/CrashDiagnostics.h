#ifndef __CRASH_DIAGNOSTICS_H__
#define __CRASH_DIAGNOSTICS_H__

#include <stdint.h>

/**
 * @brief Check for and log crash diagnostics from previous boot.
 *
 * Reads coredump summary from flash (saved automatically by ESP-IDF panic handler).
 * Logs the exception cause, PC, task name, and backtrace via the provided
 * log function. Erases the coredump after reading to avoid re-logging.
 *
 * Call this after logger and WiFi are initialized so syslog captures the data.
 *
 * @param logFunc Function to call for each log line
 */
void crashDiagnosticsCheck(void (*logFunc)(const char* msg));

/**
 * @brief Get the human-readable name for an Xtensa exception cause code.
 * @param cause Exception cause number
 * @return Static string describing the exception
 */
const char* xtensaExceptionName(uint32_t cause);

#endif // __CRASH_DIAGNOSTICS_H__
