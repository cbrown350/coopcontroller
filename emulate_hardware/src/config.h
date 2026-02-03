#ifndef EMULATOR_CONFIG_H
#define EMULATOR_CONFIG_H

#include <Arduino.h>

// ============================================================================
// FIRMWARE IDENTIFICATION
// ============================================================================

// Stringify macro for build-time version
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef FIRMWARE_VERSION_RAW
static const char* firmwareVersion = TOSTRING(FIRMWARE_VERSION_RAW);
#else
static const char* firmwareVersion = "0.0.0-dev";
#endif

#ifdef CHIP_FAMILY_RAW
static const char* chipFamily = TOSTRING(CHIP_FAMILY_RAW);
#else
static const char* chipFamily = "ESP32";
#endif

#ifdef HOST_NAME
static const char* hostName = TOSTRING(HOST_NAME);
#else
static const char* hostName = "HWEmulator";
#endif

#ifdef AP_PASSWD
static const char* apPasswd = TOSTRING(AP_PASSWD);
#else
static const char* apPasswd = "";
#endif

// NTP Server
static const char* ntpServer = "pool.ntp.org";

// ============================================================================
// PIN DEFINITIONS (from platformio.ini build flags)
// ============================================================================

// Input pins - Reading main controller outputs
#ifndef EMU_READ_PUMP_PIN
#define EMU_READ_PUMP_PIN 34
#endif

#ifndef EMU_READ_LIGHT_PIN
#define EMU_READ_LIGHT_PIN 35
#endif

#ifndef EMU_READ_DOOR_POS_PIN
#define EMU_READ_DOOR_POS_PIN 36
#endif

#ifndef EMU_READ_DOOR_NEG_PIN
#define EMU_READ_DOOR_NEG_PIN 39
#endif

#ifndef EMU_READ_BUZZER_PIN
#define EMU_READ_BUZZER_PIN 32
#endif

#ifndef EMU_READ_LED_PIN
#define EMU_READ_LED_PIN 33
#endif

// Output pins - Driving main controller inputs
#ifndef EMU_WATER_PULSE1_PIN
#define EMU_WATER_PULSE1_PIN 26
#endif

#ifndef EMU_WATER_PULSE2_PIN
#define EMU_WATER_PULSE2_PIN 25
#endif

#ifndef EMU_HALL_OPEN_PIN
#define EMU_HALL_OPEN_PIN 13
#endif

#ifndef EMU_HALL_CLOSE_PIN
#define EMU_HALL_CLOSE_PIN 12
#endif

#ifndef EMU_MANUAL_SW_PIN
#define EMU_MANUAL_SW_PIN 27
#endif

#ifndef EMU_DOOR_FAULT_PIN
#define EMU_DOOR_FAULT_PIN 14
#endif

// Status pins
#ifndef EMU_STATUS_LED_PIN
#define EMU_STATUS_LED_PIN 2
#endif

#ifndef EMU_WIFI_LED_PIN
#define EMU_WIFI_LED_PIN 4
#endif

// ============================================================================
// EMULATOR DEFAULTS
// ============================================================================

// Door simulation
constexpr uint32_t DEFAULT_DOOR_TRAVEL_TIME_MS = 10000;  // 10 seconds
constexpr uint8_t DOOR_POSITION_OPEN = 100;
constexpr uint8_t DOOR_POSITION_CLOSED = 0;
constexpr uint8_t DOOR_HALL_TRIGGER_THRESHOLD = 5;  // Position within 5% triggers hall

// Water meter simulation
constexpr float DEFAULT_PULSES_PER_GALLON = 450.0f;
constexpr float DEFAULT_FLOW_RATE_GPM = 2.5f;  // Gallons per minute when pump running
constexpr uint32_t PULSE_DURATION_MS = 50;      // Duration of each pulse

// Signal sampling
constexpr uint32_t SIGNAL_SAMPLE_INTERVAL_MS = 50;   // How often to sample input signals
constexpr uint32_t PWM_SAMPLE_WINDOW_MS = 100;       // Window for PWM measurement

// Manual switch simulation
constexpr uint32_t DEFAULT_SHORT_PRESS_MS = 200;     // Short press duration
constexpr uint32_t DEFAULT_LONG_PRESS_MS = 2000;     // Long press duration threshold
constexpr uint32_t SWITCH_DEBOUNCE_MS = 50;          // Debounce delay

// Buzzer/LED pattern tracking
constexpr uint32_t PATTERN_HISTORY_SIZE = 10;        // Number of on/off cycles to track
constexpr uint32_t PATTERN_TIMEOUT_MS = 5000;        // Reset pattern after this idle time
constexpr uint32_t MIN_BLINK_PERIOD_MS = 50;         // Minimum period to consider as blinking

// WebSocket/Status update
constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 250;  // How often to send status updates

// WiFi
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t AP_MODE_TIMEOUT_MS = 300000;  // 5 minutes before retry

// Serial
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

// ============================================================================
// ACTIVE LOW PIN DEFINITIONS
// ============================================================================
// Pins with _B suffix in main controller are active-low
// Hall sensors: Active LOW when magnet detected (door at position)
// Manual switch: Active LOW when pressed
// Door fault: Active LOW when fault detected
// Buzzer: Active LOW to sound
// WiFi LED: Active LOW to illuminate

constexpr bool HALL_SENSOR_ACTIVE_STATE = LOW;
constexpr bool MANUAL_SWITCH_ACTIVE_STATE = LOW;
constexpr bool DOOR_FAULT_ACTIVE_STATE = LOW;

// For driving outputs to main controller, we output the opposite of what we want the main to read
// So to simulate "hall sensor triggered", we output LOW
// To simulate "no trigger", we output HIGH

#endif // EMULATOR_CONFIG_H
