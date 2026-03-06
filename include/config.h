#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstring>
#include "build_timestamp.h"

#ifndef DEFAULT_LOGLEVEL
#define DEFAULT_LOGLEVEL "DEBUG" // NOSONAR - defined here for use in SettingsManager
#endif

#ifndef SETTINGS_FILE
#define SETTINGS_FILE "/user_settings.json" // NOSONAR - defined here for use in SettingsManager
#endif

#ifndef MIN_AP_TIME
#define MIN_AP_TIME 2 // NOSONAR - defined here for use in SettingsManager, minimum AP mode time in minutes
#endif

#ifndef NVS_SETTINGS_NAMESPACE
#define NVS_SETTINGS_NAMESPACE "settings_bak" // NOSONAR - NVS namespace for settings backup (max 15 chars)
#endif

#ifndef NVS_SETTINGS_KEY
#define NVS_SETTINGS_KEY "json" // NOSONAR - NVS key for settings backup JSON (max 15 chars)
#endif

#define SPIFFS LittleFS


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


// Fix for CHIP_FAMILY_RAW that may contain "ESP32"
#ifdef ESP32
#undef ESP32 // NOSONAR - needed to temporarily avoid conflict with defines below
#define ESP32_CHIP_FAMILY_FIX
#endif

static inline const char* const firmwareVersion __attribute__((unused)) = (strcmp(TOSTRING(FIRMWARE_VERSION_RAW), "") == 0) ? 
                                                    "dev-" BUILD_TIMESTAMP : TOSTRING(FIRMWARE_VERSION_RAW);
static inline const char* const chipFamily __attribute__((unused)) = (strcmp(TOSTRING(CHIP_FAMILY_RAW), "") == 0) ? "unknown" : TOSTRING(CHIP_FAMILY_RAW);
static inline const char* const gitCommitSha __attribute__((unused)) = (strcmp(TOSTRING(GIT_COMMIT_SHA_RAW), "") == 0) ? "" : TOSTRING(GIT_COMMIT_SHA_RAW);
static inline const char* const githubRepo __attribute__((unused)) = (strcmp(TOSTRING(GITHUB_REPO), "") == 0) ? "cbrown350/coopcontroller" : TOSTRING(GITHUB_REPO);

// End fix for CHIP_FAMILY_RAW that may contain "ESP32"
#ifdef ESP32_CHIP_FAMILY_FIX
#undef ESP32_CHIP_FAMILY_FIX
#define ESP32
#endif


static inline const char* const syslogServer __attribute__((unused)) = (strcmp(TOSTRING(SYSLOG_SERVER), "") == 0 || strcmp(TOSTRING(SYSLOG_SERVER), "1") == 0) ? "" : TOSTRING(SYSLOG_SERVER);
static inline const char* const syslogPort __attribute__((unused)) = (strcmp(TOSTRING(SYSLOG_PORT), "") == 0 || strcmp(TOSTRING(SYSLOG_PORT), "1") == 0) ? "514" : TOSTRING(SYSLOG_PORT);

static inline const char* const hostName __attribute__((unused)) = (strcmp(TOSTRING(HOST_NAME), "") == 0) ? "coopcontroller" : TOSTRING(HOST_NAME);
static inline const char* const otaPasswd __attribute__((unused)) = (strcmp(TOSTRING(OTA_PASSWD), "") == 0 || strcmp(TOSTRING(OTA_PASSWD), "1") == 0) ? "" : TOSTRING(OTA_PASSWD);
static inline const char* const apPasswd __attribute__((unused)) = (strcmp(TOSTRING(AP_PASSWD), "") == 0 || strcmp(TOSTRING(AP_PASSWD), "1") == 0) ? "" : TOSTRING(AP_PASSWD);

// Historical data buffer limits (~112 bytes per entry)
static constexpr size_t HISTORY_MAX_BUFFER_SIZE = 500;     // ~55KB max
static constexpr size_t HISTORY_DEFAULT_BUFFER_SIZE = 200; // ~22KB default

#define SENSOR_UPDATE_INTERVAL 5000    // NOSONAR, Update sensors every 5 seconds
#define PUMP_UPDATE_INTERVAL 1000     // NOSONAR, Update pump controller every 1 second
#define DOOR_UPDATE_INTERVAL 100      // NOSONAR, Update door controller every 100ms for faster response
#define LIGHT_UPDATE_INTERVAL 100     // NOSONAR, Update light controller every 100ms for smooth fading


// NTP server to request epoch time
static inline const char* const ntpServer __attribute__((unused)) = "pool.ntp.org";


#endif // __CONFIG_H__