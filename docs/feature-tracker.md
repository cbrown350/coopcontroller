# Feature Implementation Tracker

This document tracks all features: completed, in-progress, and planned.

---

## Implementation Status Summary

| Phase | Status |
|-------|--------|
| Phase 3 (Hardware I/O) | 100% complete |
| Phase 3.5 (Critical Refactoring) | 100% complete - HAL refactoring complete, all ESP32-specific functions abstracted |
| Phase 3.5a (Sunrise/Sunset Integration) | 100% complete with accurate UTC to local time conversion |
| Phase 3.5b (Light Control with Web UI) | 100% complete |
| Phase 3.5c (Desktop Unit Testing) | 100% complete - All 570 desktop unit tests passing, all 11 core components covered |

**Current Build:** RAM 17.6% (57,648 bytes), Flash 81.2% (1,436,321 bytes)

**Latest Build (2026-02-11):** Firmware and web UI builds successful

**Core features:** Sensors, Pump, Light, Door, Buzzer, WiFi, WebServer, SunriseSunset, Settings, Logger controllers fully implemented. HAL refactoring complete: Desktop unit testing infrastructure fully functional with MockHAL and ArduinoFake. NVS-based settings preservation for OTA filesystem updates. Actual functionality hasn't been checked for correctness.

**Test Coverage (February 2026):** 574/574 desktop tests passing (100% pass rate)

| Component | Tests |
|-----------|-------|
| BuzzerController | 3 |
| CoopControllerWebServer | 16 |
| DoorController | 68 |
| LightController | 102 |
| Logger | 11 |
| PumpController | 83 |
| SensorManager | 33 |
| SettingsManager | 149 |
| SunriseSunset | 36 |
| UpdateManager | 71 |
| WifiController | 1 |

- **Embedded Unit Tests:** 1/1 passing - Logger singleton pattern test
- **Test Infrastructure:** Complete mocking framework with MockHAL, MockSensorManager, MockBuzzerController

---

## Completed Features

### Historical Data Visualization (Event-Based) ✅

**Implemented:** 2026-02-09 (initial), 2026-02-10 (refactored to event-based)
**Status:** Complete and tested
**Implementation:** Event-based capture with in-RAM storage and CSV export

**Summary:**
Historical data collection using event-based capture instead of fixed-interval sampling. Data is recorded only when meaningful state changes occur, dramatically reducing data point count (~150-300/day vs ~1440/day with periodic sampling). Tracks temperature, pump state, flow rate, light brightness, door state, door position, and trigger sources. Each data point includes an `event_type` field indicating what triggered the recording.

**Capture Strategy:**
- **Pump, Light, Door:** Recorded immediately on any state change (no minimum interval)
- **Temperature:** Recorded when change >= 0.5°F with configurable minimum interval (default 60s)
- **Flow Rate:** Recorded when change > 0.001 GPM with configurable minimum interval (default 10s)
- **First update:** Always records initial state snapshot on boot

**Key Changes:**

1. **Backend (HistoricalDataManager)**
   - Created component `lib/HistoricalDataManager/` with circular buffer for data storage
   - Stores temperature, pump state, flow rate, light brightness, door state, door position, and trigger sources
   - Event-based `checkAndRecord()` method with internal change detection replaces periodic `update()` + `recordEvent()`
   - Each data point has `event_type` field: "temp", "flow", "pump", "light", "door"
   - Configurable minimum intervals for temperature and flow recordings
   - Memory efficient: ~96 bytes per data point
   - Automatic oldest-data overwrite when buffer is full
   - JSON and CSV export methods with all fields

2. **Backend (TriggerSource - Granular Event Sources)**
   - Expanded TriggerSource enum from 9 to 15 values for precise event tracking
   - `MANUAL` → `MANUAL_BUTTON` (physical button/switch press)
   - `AUTOMATIC` → split into: `SUNRISE`, `SUNSET`, `AUTO_CLOSE_SUNSET`, `TIMER`
   - `SENSOR` → split into: `TEMP_THRESHOLD`, `TEMP_CYCLE`, `FLOW_FAULT`
   - Added: `MAINTENANCE_CYCLE`
   - All controller default params changed from `MANUAL` to `WEB_UI`
   - DoorController: `checkManualSwitch()` uses `MANUAL_BUTTON`, `checkSchedule()` uses `SUNRISE`/`SUNSET`
   - PumpController: Temperature triggers use `TEMP_THRESHOLD`/`TEMP_CYCLE`, maintenance uses `MAINTENANCE_CYCLE`
   - LightController: Schedule uses `TIMER`

3. **Backend (SettingsManager)**
   - Settings: `history_enabled`, `history_temp_min_interval_seconds` (default 60), `history_flow_min_interval_seconds` (default 10), `history_buffer_size`
   - Getters/setters with proper constraints (temp: 10-3600s, flow: 5-300s)
   - Backward compatibility: reads old `history_sample_interval_seconds` during deserialization

4. **Backend (CoopControllerWebServer)**
   - REST API endpoints for historical data:
     - `GET /data/history` - Returns JSON array of all data points
     - `GET /data/export_csv` - Downloads CSV file with proper headers
     - `POST /data/clear` - Clears all historical data (protected)
   - `/update_settings` handles history settings (enabled, temp/flow intervals, buffer size)

5. **Backend (main.cpp)**
   - Integrated `checkAndRecord()` after each controller update for prompt state change capture
   - Removed old event detection block (prevDoorState, prevPumpActive, flowWasActive tracking)
   - Simplified integration: HistoricalDataManager handles all change detection internally

6. **Frontend (Web UI)**
   - History.tsx: Chart.js charts for temperature, pump, flow, light, door state/position
   - Event-type-specific markers: each chart highlights its own event type with red dots
   - Tooltips show `[event_type]` tag (e.g., [pump], [door], [temp])
   - Data point counter shows breakdown by event type (temp/flow/pump/light/door)
   - Settings.tsx: New "Historical Data Settings" section with enable toggle, temp/flow interval inputs, buffer size
   - types.ts: Added history settings to Settings interface

**Features:**
- **Event-based capture** - Records only on meaningful state changes, not at fixed intervals
- **Granular trigger sources** - 15 distinct trigger types (button, web UI, sunrise, sunset, temp threshold, etc.)
- **Configurable intervals** - Temperature and flow minimum recording intervals adjustable via web UI
- **Real-time visualization** - Interactive line charts with Chart.js
- **CSV export** - Download historical data for offline analysis
- **Memory efficient** - Circular buffer with predictable memory footprint

**API Endpoints (3 total):**
- `GET /data/history` - Public, returns JSON data array
- `GET /data/export_csv` - Public, downloads CSV file
- `POST /data/clear` - Protected, clears all history

**Build Status:**
- Firmware: Compiled successfully (RAM 17.3%, Flash 99.5%)
- Web UI: Compiled successfully with zero TypeScript errors
- Tests: 488/488 passing

**Future Enhancements:**
- Remote database storage (InfluxDB, PostgreSQL)
- Data compression for longer retention
- SD card backup for permanent storage
- Configurable data retention policies

---

### API Authentication for Critical Endpoints ✅

**Implemented:** 2026-02-06
**Status:** Complete and tested
**Implementation Plan:** [temp_api_auth_implementation_plan.md](temp_api_auth_implementation_plan.md)

**Summary:**
Added HTTP Basic Authentication to protect state-modifying REST API endpoints while keeping read-only endpoints public for monitoring.

**Key Changes:**

1. **Backend (SettingsManager)**
   - Added API authentication settings to `user_settings` struct
   - Fields: `api_auth_enabled`, `api_username`, `api_password`
   - Getters/setters with proper encapsulation
   - JSON serialization/deserialization support

2. **Backend (HAL Layer)**
   - Extended `IWebRequest` interface with `hasHeader()` and `header()` methods
   - Extended `IWebResponse` interface with `addHeader()` method
   - Implemented in both ESP32 (HAL_ESP32.cpp) and Mock (MockHAL.h) HALs
   - Response wrapper pattern to apply headers when response is sent

3. **Backend (CoopControllerWebServer)**
   - Implemented Base64 decode function for credentials
   - Added `isAuthenticated()` middleware checking Authorization header
   - Added `sendAuthRequired()` helper sending 401 with WWW-Authenticate header
   - Protected 30 state-modifying endpoints with authentication checks
   - Left 6 read-only endpoints public: `/sensor_status`, `/system_status`, `/logs`, `/version`, `/sun/times`, `/get_settings`

4. **Frontend (Web UI)**
   - Added `api_auth_enabled`, `api_username`, `api_password` fields to Settings interface (types.ts)
   - Created `authenticatedFetch()` utility wrapping fetch with HTTP Basic Auth (utils/api.ts)
   - Added credential caching with `setAuthCredentials()`, `clearAuthCredentials()`, `getAuthCredentials()`
   - Added authentication UI section in Settings.tsx with toggle, username, and password fields
   - Updated all state-modifying fetch calls in Settings.tsx and Status.tsx to use `authenticatedFetch()`

**Protected Endpoints (30 total):**
- Pump: `/pump/on`, `/pump/off`, `/pump/auto`, `/pump/force_cycle`, `/pump/clear_error`, `/pump/clear_off_flow_detected`, `/pump/reset_stats`
- Water: `/water/reset/1`, `/water/reset/2`
- Light: `/light/on`, `/light/off`, `/light/fade_in`, `/light/fade_out`, `/light/set_brightness`, `/light/reset_stats`
- Door: `/door/open`, `/door/close`, `/door/stop`, `/door/clear_fault`, `/door/reset_stats`
- Buzzer: `/buzzer/test`, `/buzzer/silence`, `/buzzer/clear`
- Settings: `/update_settings`, `/factory_reset`, `/settings/restore`, `/reboot`

**Public Endpoints (7 total):**
- `/sensor_status` - Real-time sensor data
- `/system_status` - System health metrics
- `/logs` - Log retrieval
- `/version` - Firmware version
- `/sun/times` - Sunrise/sunset calculation
- `/get_settings` - Settings (excludes password)
- `/settings/backup` - Settings backup download

**Security Features:**
- Optional authentication (disabled by default for backwards compatibility)
- HTTP Basic Authentication with Base64-encoded credentials
- Failed authentication attempts logged
- Password never returned in `/get_settings` endpoint
- Credentials cached in browser for seamless UX
- Clear warning in UI about credential security

**Testing:**
- ✅ All 469 desktop unit tests pass
- ✅ ESP32 firmware builds successfully (RAM: 17.3%, Flash: 96.1%)
- ✅ Web UI builds successfully with TypeScript

**User Experience:**
- Authentication disabled by default - no breaking changes
- Admin can enable auth and set credentials in Settings page
- Once enabled, all control operations require username/password
- Credentials auto-submitted with each request after initial entry
- Factory reset remains accessible with physical device access

**Files Modified:**
- `lib/SettingsManager/SettingsManager.h` - Added auth settings
- `lib/SettingsManager/SettingsManager.cpp` - Implemented getters/setters
- `lib/HAL/IHAL.h` - Extended web interfaces for headers
- `lib/HAL_ESP32/HAL_ESP32.cpp` - Implemented header support
- `lib/CoopControllerWebServer/CoopControllerWebServer.h` - Added auth methods
- `lib/CoopControllerWebServer/CoopControllerWebServer.cpp` - Implemented authentication middleware
- `test/common/mocks/MockHAL.h` - Added mock header support
- `web/src/types.ts` - Added auth settings interface
- `web/src/utils/api.ts` - Created authenticatedFetch utility
- `web/src/Settings.tsx` - Added auth UI and updated endpoints
- `web/src/Status.tsx` - Updated all control endpoints

---

### Factory Reset on Boot ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Added hardware-triggered factory reset capability activated by holding a button during boot sequence.

**Hardware Factory Reset:**
- Hold manual door switch (DOOR_MANUAL_SWITCH_B_PIN) for 20 seconds during bootup to trigger factory reset
- WIFI_LED_B_PIN indicates factory reset in progress (rapid blink pattern at 100ms intervals)
- Countdown printed to serial console every second
- Clears all settings to defaults
- Clears WiFi credentials
- Forces AP mode on next boot
- Serial log confirmation of factory reset
- Device automatically restarts after factory reset
- Implemented in `main.cpp` with `checkFactoryResetRequest()` function, runs before component initialization

**Software Factory Reset:**
- Factory reset button available in web UI Settings page
- Confirmation dialog required before executing
- Same behavior as hardware reset

---

### Water Meter Calibration ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Added configurable pulses-per-gallon calibration for water meter sensors.

**Key Changes:**
- `pulses_per_gallon` setting in SettingsManager (default 450.0)
- Exposed in web UI Settings page with input control (min 100, max 2000)
- Cached in SensorManager for ISR-safe access
- Handled in `/update_settings` API endpoint
- Flow rate calculation using calibration

---

### Scheduled Pump Maintenance Cycles ✅

**Implemented:** 2026-01-XX
**Status:** Complete and tested

**Summary:**
Added minimum daily pump cycling to prevent water stagnation and maintain pump seal lubrication.

**Key Changes:**
- Configurable minimum cycles per 24 hours
- Configurable cycle duration (30-600 seconds)
- Automatic scheduling when temperature cycling is insufficient
- Temperature-triggered cycles count toward minimum
- Clear UI indication when scheduled cycle is active
- All unit tests pass

**Settings:**
- `pump_min_daily_cycles_enabled` (bool, default: false)
- `pump_min_daily_cycles` (int, range 1-12)
- `pump_min_cycle_run_seconds` (int, range 30-600)
- Uses millis()-based interval scheduling
- Disabled by default

---

### Pump Flow Per-Pulse Calculation ✅

**Summary:**
Added per-pulse flow rate calculation for more responsive flow monitoring.

**Key Changes:**
- New setting `water_meter_per_pulse_calculation_enabled` (default: false) for backward compatibility
- Per-pulse flow calculation in SensorManager interrupt handlers for instantaneous measurement
- Noise filtering (10ms threshold) to prevent false pulse detection
- No-flow timeout detection (5 seconds) to identify when flow has stopped
- Atomic operations for thread-safe access to shared variables in interrupt context
- Proper millis() rollover handling for correct time calculations after ~49.7 days
- Web UI toggle control with descriptive help text

**Key Features:**
- **Instantaneous measurement** - Flow rate calculated after every pulse instead of waiting for fixed intervals
- **Noise filtering** - 10ms minimum pulse interval filters electrical noise from the water meter
- **No-flow timeout** - Automatically detects when flow has stopped (no pulses for 5 seconds)
- **Rollover handling** - Proper millis() overflow handling ensures correct calculations after extended runtime
- **Thread-safe** - Atomic operations protect shared variables in interrupt context
- **Backward compatible** - Default disabled, users can enable via web UI settings

**Build Verification:**
- ESP32: ✅ RAM: 56,436 bytes (17.2%) - no change; Flash: 1,081,881 bytes (82.6%) - +0.1%
- Web UI: ✅ TypeScript compilation successful, Settings page updated with new toggle control

---

### Pump OFF Flow Monitoring ✅

**Summary:**
Monitors for water flow when pump is OFF to detect hardware faults (stuck relay, valve leak).

**Key Changes:**
- New settings: `pump_off_flow_monitoring_enabled` (bool, default: false) and `pump_off_flow_grace_period_seconds` (int, default: 30)
- Records timestamp when pump turns OFF and monitors flow rate after grace period elapses
- Logs WARNING: "Water flow detected while pump is OFF - Possible stuck relay or valve leak"
- Public methods: `getPumpOffFlowDetected()` and `clearPumpOffFlowDetected()`
- Proper millis() rollover handling for long-running systems
- Web UI toggle control with descriptive help text
- Web UI warning alert banner displays when pump off flow is detected, with "Clear Warning" button
- REST endpoint: `GET /pump/clear_off_flow_detected`

**Key Features:**
- **Grace period** - Configurable delay (default 30 seconds) after pump turns off before monitoring begins to prevent false alarms
- **Hardware fault detection** - Identifies stuck relays, valve leaks, or other hardware issues
- **Automatic reset** - Detection flag automatically clears when pump turns ON
- **Manual acknowledgment** - Users can clear warning via web UI button or REST API endpoint
- **Disabled by default** - Prevents false alarms during normal operation until user enables it

**Build Verification:**
- ESP32: ✅ RAM: 56,444 bytes (17.2%) - +8 bytes; Flash: 1,084,957 bytes (82.8%) - +3,076 bytes
- Web UI: ✅ Build time: 1.50 seconds

---

### SPA Routing Fix ✅

**Issue:** When users navigated to client-side routes like `/update`, `/settings`, `/logs`, the web server attempted to load non-existent files from LittleFS, causing filesystem error messages in the serial logs.

**Fix:**
- Reordered web server method registration to ensure proper handler execution order
- Moved static file handlers to be registered BEFORE catch-all handler
- The catch-all handler now correctly serves `index.htm` for client-side routes that don't match existing files
- Removed commented code from HAL implementation to clean up the codebase
- Proper SPA behavior implemented - SolidJS router handles client-side routing without server errors

**Build Verification:**
- ESP32: ✅ RAM: 56,444 bytes (17.2%) - no change; Flash: 1,085,581 bytes (82.8%) - +624 bytes

---

### HAL Refactoring ✅

**Issue:** Direct ESP32 API calls throughout the codebase made unit testing impossible without physical hardware and created tight coupling to ESP32-specific implementations.

**Solution:**
- Created comprehensive HAL interface (`IHAL.h`) with 32 methods abstracting all ESP32-specific functionality
- Implemented ESP32 HAL (`HAL_ESP32`) with full ESP32 API support
- Created mock HAL implementation (`MockHAL.h`) for desktop testing
- Refactored core components to use HAL interface:
  - `SettingsManager` - Uses HAL for filesystem operations
  - `WifiController` - Uses HAL for WiFi operations
  - `CoopControllerWebServer` - Uses HAL for web server operations
  - `LightController` - Uses HAL for LEDC PWM control
- Partially refactored `main.cpp` - Only `esp_reset_reason` replaced with HAL call
- Watchdog functions remain unabstracted (not critical for testing)

**HAL Interface Methods (32 total):**
- **Filesystem:** `fileExists()`, `readFile()`, `writeFile()`, `deleteFile()`, `listFiles()`
- **Web Server:** `createWebServer()`, `on()`, `send()`, `send_P()`, `sendChunked()`, `clientIP()`, `uri()`, `method()`, `arg()`, `hasArg()`, `args()`, `header()`, `hasHeader()`, `headers()`, `authenticate()`, `requestAuthentication()`, `setBasicAuth()`, `serveStatic()`, `serveStaticFromLittleFS()`
- **WiFi:** `WiFiStatus()`, `WiFiSSID()`, `WiFiLocalIP()`, `WiFiMode()`, `beginWiFi()`, `disconnectWiFi()`, `scanNetworks()`
- **LEDC:** `ledcSetup()`, `ledcAttachPin()`, `ledcWrite()`, `ledcDetachPin()`
- **System:** `getResetReason()`, `getFreeHeap()`, `getChipModel()`, `millis()`, `delay()`, `random()`, `taskWdtReset()`

**Build Verification:**
- ESP32: ✅ RAM: 56,444 bytes (17.2%); Flash: 1,085,009 bytes (82.8%) - Zero errors/warnings
- Desktop Tests: ✅ 2/2 passing (100% pass rate), duration 32.1s, MockHAL verified with all 32 methods
- Minimal memory impact: RAM +8 bytes, Flash +3,128 bytes

**Benefits:**
- Desktop unit testing now possible - Core components can be tested without ESP32 hardware
- Better code organization - Clear separation between hardware abstraction and business logic
- Improved testability - Mock implementations enable comprehensive unit testing
- Enhanced maintainability - Hardware changes isolated to HAL implementation

**Documentation References:**
- HAL Interface: `lib/HAL/IHAL.h`
- ESP32 Implementation: `lib/HAL_ESP32/`
- Mock Implementation: `test/common/mocks/MockHAL.h`
- HAL Analysis: `docs/temp_HAL_Analysis.md`
- Web Server HAL Analysis: `docs/temp_WebServer_HAL_Analysis.md`

---

### Sunrise/Sunset UTC Conversion Fix ✅

**Issue:** Sunrise/sunset calculations were displaying incorrect times due to UTC conversion errors.

**Fix:**
- Implemented proper UTC to local time conversion using timezone offset
- Added automatic recalculation when location or timezone settings change
- Calculations now accurately reflect user's local time
- Web UI displays correct sunrise/sunset times in Status and Settings pages

---

### Light Control Regression Fix ✅

**Issue:** Manual light controls were triggering unwanted fade transitions, making immediate control difficult. Auto mode wasn't properly using sine-wave fades.

**Fix:**
- Manual controls (ON/OFF/Timer) now respond immediately without fade transitions
- Auto mode properly implements sine-wave fade-in/fade-out for natural lighting
- State machine correctly differentiates between MANUAL and AUTO states
- User can now immediately control lights when needed while auto mode provides smooth transitions

---

### Component Naming Refactoring ✅

**Changes:**
- `TempSensor` → `SensorManager` - Better reflects dual-purpose sensor management
- `Buzzer` → `BuzzerController` - Consistent naming with other controllers
- `Light` → `LightController` - Consistent naming with other controllers

**Benefits:**
- Improved code clarity and maintainability
- Consistent naming pattern across all controller classes
- More accurate representation of component responsibilities

---

### WiFi Controller Refactoring ✅

**Issue:** WiFi management code was scattered throughout main.cpp, making it difficult to maintain and understand.

**Fix:**
- Extracted all WiFi functionality from main.cpp to dedicated WifiController class
- Moved WiFi-related global variables into WifiController encapsulation
- Implemented proper initialization through begin() method
- Maintained existing functionality while improving code organization
- Other components now use clean WifiController interface

**Benefits:**
- Improved code organization and separation of concerns
- Easier to maintain and test WiFi functionality
- Consistent controller pattern across all system components
- Reduced complexity in main.cpp

---

### Logger Method Refactoring ✅

**Issue:** Logger used generic log() and logf() methods without clear severity indication, requiring conditional debug checks throughout code.

**Fix:**
- Replaced all logger.log() and logger.logf() calls with level-specific methods
- Introduced logInfo(), logWarning(), logError(), logDebug(), logVerbose() methods
- Removed conditional debug if/then blocks - logger methods now handle filtering internally
- Improved log message clarity and consistency
- All critical events now logged at appropriate severity levels

**Benefits:**
- Clearer code with explicit log severity
- Eliminated repetitive conditional debug checks
- Improved log filtering and organization
- Better debugging and troubleshooting capabilities
- Consistent logging patterns throughout codebase

---

### Sensor Error Handling ✅ (Already Implemented)

**Investigation:** Review of sensor error handling functionality revealed it was already fully implemented in the codebase.

**Findings:**
- `SensorManager` detects DEVICE_DISCONNECTED_C (-127.0°C) and sets `is_connected` to false, `temperature_f` to NAN
- `CoopControllerWebServer` returns nullptr for `temperature_f` when sensor is disconnected
- `Status.tsx` displays "---°F" for null/undefined/NaN temperature values
- Web UI properly shows descriptive error messages when sensors are not detected

**Changes Made:**
- Updated `web/src/types.ts` to change `temperature_f: number` to `temperature_f: number | null` for proper TypeScript type alignment

---

### Remote Syslog Runtime Configuration ✅

**Implemented:** 2026-02-XX
**Status:** Complete and tested

**Summary:**
Moved syslog server/port from compile-time defines to runtime-configurable web UI settings, allowing users to change syslog targets without recompiling firmware.

**Key Changes:**
- Added `syslog_server` (String, default "") and `syslog_port` (int, default 514) to SettingsManager
- Added `reconfigureSyslog(server, port, hostname)` method to Logger
- Runtime reconfiguration in main.cpp after settings load (overrides compile-time defaults)
- `/update_settings` API handler reconfigures syslog when server/port changes
- Web UI Settings page: Syslog Server fieldset with server address and port inputs in 2-column grid
- Empty server string disables syslog output

**Files Modified:**
- `lib/SettingsManager/SettingsManager.h` - Added syslog_server, syslog_port fields
- `lib/SettingsManager/SettingsManager.cpp` - Getters/setters with port clamping (1-65535)
- `lib/Logger/Logger.h` - Added reconfigureSyslog() declaration
- `lib/Logger/Logger.cpp` - Implemented reconfigureSyslog()
- `lib/CoopControllerWebServer/CoopControllerWebServer.cpp` - Handler for syslog setting changes
- `src/main.cpp` - Runtime syslog reconfiguration from saved settings
- `web/src/types.ts` - Added syslog_server, syslog_port to Settings interface
- `web/src/Settings.tsx` - Added syslog configuration UI controls

**Build Verification:**
- ESP32: ✅ RAM: 56,580 bytes (17.3%), Flash: 1,266,841 bytes (96.7%)
- Web UI: ✅ TypeScript compilation successful
- Tests: ✅ 488/488 passing

---

### Configurable Flow Calculation Interval ✅

**Implemented:** 2026-02-XX
**Status:** Complete and tested

**Summary:**
Made the flow calculation interval configurable via web UI instead of hardcoded at 60 seconds.

**Key Changes:**
- Added `flow_calculation_interval_seconds` (unsigned int, default 60, range 5-300) to SettingsManager
- Changed SensorManager `FLOW_CALCULATION_INTERVAL` from static const to configurable member `flowCalculationIntervalMs_`
- Added `setFlowCalculationIntervalSeconds(seconds)` method to SensorManager
- `/update_settings` API handler updates SensorManager when interval changes
- Interval applied on boot in main.cpp after sensor calibration
- Web UI Settings page: Flow Calculation Interval fieldset with number input (5-300 seconds)

**Files Modified:**
- `lib/SettingsManager/SettingsManager.h` - Added flow_calculation_interval_seconds field
- `lib/SettingsManager/SettingsManager.cpp` - Getter/setter with clamping (5-300)
- `lib/SensorManager/SensorManager.h` - Configurable interval member, setter method
- `lib/SensorManager/SensorManager.cpp` - Uses configurable interval in calculateFlowRate()
- `lib/CoopControllerWebServer/CoopControllerWebServer.cpp` - Handler for interval setting
- `src/main.cpp` - Applies interval on boot
- `web/src/types.ts` - Added flow_calculation_interval_seconds to Settings interface
- `web/src/Settings.tsx` - Added flow interval configuration UI controls

**Build Verification:**
- ESP32: ✅ RAM: 56,580 bytes (17.3%), Flash: 1,266,841 bytes (96.7%)
- Web UI: ✅ TypeScript compilation successful
- Tests: ✅ 488/488 passing

---

### NTP WiFi Safety Guard ✅

**Implemented:** 2026-02-XX
**Status:** Complete

**Summary:**
Added network safety check around NTP time configuration to prevent calls when WiFi is not connected.

**Key Changes:**
- Wrapped `configTime()` call in `if (wifiController.isConnected())` check
- Logs warning when NTP configuration is deferred due to no WiFi connection
- Prevents potential crashes or hangs when making network calls without connectivity

**Files Modified:**
- `src/main.cpp` - Added WiFi connection guard around configTime()

---

### Postman API Collection ✅

**Implemented:** 2026-02-XX
**Status:** Complete

**Summary:**
Created comprehensive Postman collection documenting all REST API endpoints for easy testing and sharing.

**Key Changes:**
- Postman v2.1 collection with 37 endpoints organized in folders
- Folders: Public (7), Settings (4), Pump (7), Water Meter (2), Door (7), Light (6), Buzzer (3), System (1)
- Base URL variable (`{{baseUrl}}`) defaults to `http://coopcontroller.local`
- HTTP Basic Auth pre-configured for protected endpoints
- Descriptions for each endpoint with method, expected response format
- Importable JSON file for easy team sharing

**Files Created:**
- `docs/CoopController.postman_collection.json` - Complete Postman collection

---

### Mobile UI Optimization ✅

**Implemented:** 2026-02-XX
**Status:** Complete

**Summary:**
Fixed responsive layout issues for mobile devices in the main App shell.

**Key Changes:**
- Changed `h-screen` to `min-h-screen` to prevent content cutoff on overflow
- Reduced top padding on mobile: `pt-10` → `pt-4 sm:pt-10`
- Added symmetric horizontal padding: `pl-1` → `px-2 sm:pl-1`
- Smaller tab text on mobile: added `text-xs sm:text-sm`
- Tighter content padding on mobile: `p-6` → `px-2 py-4 sm:p-6`

**Files Modified:**
- `web/src/App.tsx` - Updated responsive CSS classes

**Build Verification:**
- Web UI: ✅ TypeScript compilation successful

---

### Web Assets Security Refactoring ✅
- Moved web assets into separate subdirectory within LittleFS
- Adjusted web server root path to serve from the new subdirectory
- Prevents direct access to `user_settings.json` via web requests
- Web assets now served from `/www/` subdirectory, `user_settings.json` no longer web-accessible
- WiFi credentials and API keys protected from unauthorized access via direct file access

---

### PR #2 Issues Fixed ✅
- Github PR #2 had a number of problems that needed to be fixed: "Refactor for unit test #2"
- All 13 issues fixed and verified. See `docs/temp_PR2_fixes.md` for details.

---

### WiFi Status LED ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Heartbeat LED on WIFI_LED_B_PIN when connected, fast blink when disconnected. Configurable via `wifi_led_enabled` setting. Implemented in WifiController.

---

### ESP32 Watchdog ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Watchdog timer implemented in main loop to detect hangs and automatically restart. Uses ESP32 task watchdog API in main.cpp.

---

### Buzzer Alerts Integration ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Buzzer sounds on fault conditions (pump failure, sensor error, door fault). Configurable alert patterns. Web UI silence/test/clear buttons. Persistent until acknowledged. BuzzerController integrated with all controllers in main.cpp.

---

### Automatic Door Close After Sunset ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
Settings `door_auto_close_after_sunset_enabled` and `door_auto_close_after_sunset_minutes` implemented. Door auto-closes X minutes after sunset. Web UI toggle and delay input in Settings page. Logic in DoorController::shouldCloseBySchedule().

---

### Door Progress Calculation ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
`getProgressPercentage()` calculates open/close progress based on elapsed time vs timeout. Progress bar displayed in Status page door section. Updated in real-time via sensor_status endpoint.

---

### Door Lockout Toggle ✅

**Implemented:** 2026-02-06
**Status:** Complete and tested

**Summary:**
Door lockout prevents all door operations when enabled. Useful for maintenance or manual intervention.

**Key Changes:**
- `door_lockout_enabled` setting in SettingsManager (default: false)
- DoorController blocks open(), close(), checkManualSwitch(), checkSchedule() when lockout enabled
- Web API endpoints: `/door/lockout/on` and `/door/lockout/off` (auth required)
- Settings page toggle and status page lock/unlock button with warning banner
- Disabled door control buttons when lockout active
- JSON serialization includes `lockout_enabled` in door status

**Testing:** Unit tests for lockout blocking open, close, schedule, manual switch, and JSON output.

---

### Door Timeout Auto-Calculation ✅

**Implemented:** 2026-02-06
**Status:** Complete and tested

**Summary:**
Automatically adjusts door timeouts based on historical operation durations.

**Key Changes:**
- Circular buffer (10 entries) tracks open/close timing history in DoorController
- `getRecommendedOpenTimeout()` / `getRecommendedCloseTimeout()` return max(history)/1000 + 1 second
- When `autoCalcTimeoutEnabled` is true, timeouts auto-update after each successful operation
- `door_timeout_auto_calc_enabled` setting in SettingsManager
- Settings page toggle control
- JSON serialization includes `auto_calc_timeout_enabled`, `recommended_open_timeout`, `recommended_close_timeout`

**Testing:** Unit tests for timing recording, recommended timeout calculation, circular buffer overflow, auto-update behavior, and JSON output.

---

### Refactor main.cpp WiFi Functions ✅

**Implemented:** 2026-01-XX
**Status:** Complete

**Summary:**
WiFi management code extracted from main.cpp to dedicated WifiController class. All WiFi state and logic encapsulated.

---

## Completed Features (Previously Listed as Planned)

The following features were found to already be implemented during a project-wide audit (February 2026):

### System Status Display ✅

**Status:** Already implemented (discovered during audit)

**Summary:**
System status information is fully displayed in the web UI Status page.

**Implementation:**
- `/system_status` REST endpoint returns heap memory (free/total/percent), uptime (seconds + formatted), chip model, CPU frequency, flash size, WiFi info (RSSI, SSID, IP, MAC, BSSID)
- Status.tsx "System Status" card displays all system info with memory progress bar
- main.cpp logs system status every 10 seconds at VERBOSE level
- Low memory alert triggers buzzer when heap usage > 80%

---

### IP Address and MAC Address Display ✅

**Status:** Already implemented (discovered during audit)

**Summary:**
ESP32 network information displayed in web UI and API.

**Implementation:**
- `/system_status` endpoint returns `wifi_ip`, `wifi_mac`, `wifi_bssid`
- Status.tsx displays IP address, MAC address, and BSSID in the System Status card
- HAL methods: `wifiGetLocalIP()`, `wifiGetMacAddress()`, `wifiGetBSSID()`

---

### Reboot Controls ✅

**Status:** Already implemented (discovered during audit)

**Summary:**
Web UI reboot button with confirmation dialog.

**Implementation:**
- `/reboot` POST endpoint with `confirm=REBOOT` parameter (auth required)
- Settings.tsx reboot button with confirmation dialog
- 3-second delay before restart for response delivery
- Logged at WARNING level

---

### Floating UI Elements for Settings Page ✅

**Status:** Already implemented (discovered during audit)

**Summary:**
Floating notifications, save button, and unsaved changes indicator in Settings page.

**Implementation:**
- **Floating error toast:** Fixed position top-center with z-50, auto-dismiss
- **Floating success toast:** Fixed position top-center with z-50, "Settings saved successfully!"
- **Floating unsaved changes badge:** Fixed position bottom-left with z-50, warning badge
- **Floating save button:** Fixed position bottom-right with z-50, accent button
- All elements use CSS `fixed` positioning with proper z-index
- Responsive across mobile, tablet, desktop

---

### Improved Connection Status ✅

**Status:** Already implemented (discovered during audit)

**Summary:**
Water meter sensors show "connected" only when pulses are actively detected within a configurable timeout.

**Implementation:**
- `isActivelyConnected()` method in SensorManager checks pulse_count > 0 and time since last pulse < configurable timeout
- `water_meter_timeout_seconds` setting (default 300s) controls disconnect threshold
- `actively_connected` field in sensor status API response
- `getTimeSinceLastPulse()` provides elapsed time since last pulse
- Status strings: "Water Meter - Connected (Active)", "Water Meter - Connected (Idle)", "Water Meter - Configured (No Pulses Detected)"

---

### Door Control ✅

**Status:** Already implemented (full DoorController with all sub-features)

**Summary:**
Complete door automation with motor control, hall sensors, fault detection, scheduling, lockout, and auto-calculated timeouts.

**Implementation:**
- Bidirectional motor control using DRV8833 (HAL abstracted)
- Hall effect position sensors for open/closed detection
- Fault detection with timeout and DRV8833 fault pin
- Manual control from web UI (open/close/stop buttons)
- Manual switch input for physical control
- Auto mode with sunrise/sunset scheduling and configurable offsets
- Auto-close after sunset with configurable delay
- Door lockout (blocks all operations when enabled)
- Auto-calculated timeouts from operation history (circular buffer of 10 entries)
- Progress percentage calculation during operations
- Complete REST API with 7 endpoints (all auth-protected)
- 68 unit tests covering all functionality

---

### Network Safety (NTP Guard) ✅

**Status:** Already implemented

**Summary:**
Network calls are guarded against WiFi disconnection for currently implemented services.

**Implementation:**
- `configTime()` NTP call wrapped in `wifiController.isConnected()` check
- Warning logged when NTP sync deferred due to no WiFi
- Syslog uses UDP (fails gracefully without WiFi)
- mDNS only started after WiFi connects
- Web server operates in both STA and AP modes
- Note: Additional guards for future services (Email, Telegram, MQTT, OpenWeather) will be added when those features are implemented

---

### WiFi BSSID Preference ✅

**Implemented:** 2026-02-09
**Status:** Complete and tested

**Summary:**
Added support for connecting to a specific WiFi access point by BSSID (MAC address), useful for mesh networks with multiple APs sharing the same SSID.

**Key Changes:**
- Added `wifiBeginWithBSSID(ssid, password, bssid)` method to HAL interface (IHAL.h)
- Implemented in HAL_ESP32 using `WiFi.begin(ssid, password, 0, bssid)` for channel-agnostic BSSID connection
- Added mock implementation in MockHAL.h for desktop testing
- WifiController parses BSSID preference string ("AA:BB:CC:DD:EE:FF" format) and uses BSSID-aware connection
- `parseBSSID()` helper with hex validation and format checking
- Falls back to auto-select (standard `wifiBegin`) when BSSID is empty or invalid
- BSSID preference used in both initial connection and reconnection paths
- Setting, getter/setter, JSON serialization, web server handler, and Settings UI input field were already in place

**Files Modified:**
- `lib/HAL/IHAL.h` - Added `wifiBeginWithBSSID()` pure virtual method
- `lib/HAL_ESP32/HAL_ESP32.h` - Added override declaration
- `lib/HAL_ESP32/HAL_ESP32.cpp` - Implemented with `WiFi.begin(ssid, password, 0, bssid)`
- `lib/WifiController/WifiController.h` - Added `parseBSSID()` declaration
- `lib/WifiController/WifiController.cpp` - BSSID-aware connection in `wifiSetup()` and `checkWifiConnection()`
- `test/common/mocks/MockHAL.h` - Added mock implementation

**Build Verification:**
- ESP32: ✅ RAM: 56,580 bytes (17.3%), Flash: 1,267,977 bytes (96.7%) - +1,136 bytes
- Web UI: ✅ TypeScript compilation successful (UI already existed)
- Tests: ✅ 488/488 passing

---

### Web UI Theming Based on Logo ✅

**Implemented:** 2026-02-09
**Status:** Complete

**Summary:**
Applied custom DaisyUI theme with warm agricultural color palette inspired by the coop logo, with automatic dark/light mode support.

**Key Changes:**
- Created custom "coop" light theme with warm earthy palette (barn-red primary, forest-green secondary, golden accent, cream base)
- Created custom "coop-dark" dark theme with earthy dark tones
- Both themes use oklch color space for perceptually uniform colors
- Added logo display in App header (logo.webp alongside title)
- Title styled with primary color for brand consistency
- Responsive logo sizing (8x8 mobile, 10x10 desktop)
- Themes automatically switch based on `prefers-color-scheme`

**Theme Colors:**
- **Primary:** Warm barn red-brown (controls, buttons)
- **Secondary:** Forest green (secondary actions)
- **Accent:** Golden amber (highlights, accent elements)
- **Base:** Warm cream (light) / Dark earth (dark)
- **Info/Success/Warning/Error:** Appropriate semantic colors

**Files Modified:**
- `web/src/index.css` - Custom DaisyUI theme definitions
- `web/src/App.tsx` - Logo in header with responsive sizing

**Build Verification:**
- Web UI: ✅ TypeScript compilation successful

---

### NVS Settings Preservation for OTA Updates ✅

**Implemented:** 2026-02-11
**Status:** Complete and tested
**Implementation:** ESP32 NVS (Non-Volatile Storage) based backup/restore

**Summary:**
Settings are automatically backed up to NVS before OTA filesystem updates and restored on next boot. NVS is a separate flash partition from LittleFS, so it survives filesystem flashing. This ensures user configuration (WiFi credentials, pump settings, light schedules, etc.) is preserved across OTA updates.

**Key Changes:**

1. **HAL Layer (IHAL, HAL_ESP32, MockHAL)**
   - Added 3 NVS methods to IHAL interface: `nvsWriteString()`, `nvsReadString()`, `nvsRemove()`
   - HAL_ESP32 implementation uses ESP32 `Preferences` library for NVS access
   - MockHAL uses in-memory `std::map<String, String>` for desktop testing

2. **SettingsManager**
   - `backupToNVS()` - Serializes all settings to JSON and writes to NVS partition
   - `restoreFromNVS()` - Reads NVS backup, applies settings, saves to LittleFS, clears NVS backup
   - `begin()` auto-calls `restoreFromNVS()` on startup to detect and restore any pending backup
   - Direct file writing in restore (bypasses `save()` which calls `loadFile()` that resets settings when no file exists)

3. **UpdateManager**
   - `installUpdate()` calls `settingsManager.backupToNVS()` before filesystem flash (when not skipping filesystem)
   - Warning logged if backup fails but update proceeds

4. **OTA Settings Bug Fix**
   - Fixed missing OTA settings (`auto_update_enabled`, `update_check_interval_hours`, `manifest_url`) in `setFromJsonDoc()` and `toJsonDoc()`
   - Added missing getter/setter implementations with `constrain(hours, 1, 168)` clamping

5. **Config Constants**
   - `NVS_SETTINGS_NAMESPACE` ("settings_bak") - NVS namespace for backup
   - `NVS_SETTINGS_KEY` ("json") - NVS key for settings JSON

**Testing (14 new tests):**
- OTA settings: default values, getter/setter, clamping (1-168), manifest URL, JSON serialization, JSON deserialization
- NVS backup/restore: backup saves JSON, restore restores settings, clears backup after restore, returns false with no backup, auto-restore on begin, normal boot without backup

**Files Modified:**
- `lib/HAL/IHAL.h` - Added NVS pure virtual methods
- `lib/HAL_ESP32/HAL_ESP32.h` - NVS override declarations
- `lib/HAL_ESP32/HAL_ESP32.cpp` - NVS implementations using Preferences library
- `test/common/mocks/MockHAL.h` - In-memory NVS mock with test helpers
- `include/config.h` - NVS namespace/key constants
- `lib/SettingsManager/SettingsManager.h` - backupToNVS/restoreFromNVS declarations
- `lib/SettingsManager/SettingsManager.cpp` - NVS backup/restore + OTA settings fix
- `lib/UpdateManager/UpdateManager.cpp` - Settings backup before filesystem flash
- `test/unit_desktop/test_SettingsManager/test_SettingsManager.cpp` - 14 new tests

**Build Verification:**
- ESP32: ✅ RAM: 57,648 bytes (17.6%), Flash: 1,436,321 bytes (81.2%)
- Web UI: ✅ TypeScript compilation successful
- Tests: ✅ 503/503 passing

---

### OTA Update System ✅

**Implemented:** 2026-02-11
**Status:** Complete and tested

**Summary:**
Full over-the-air firmware and filesystem update system with GitHub Releases integration. Devices check for updates via a version manifest, stream firmware/filesystem binaries with chunked downloads, and flash via ESP32 Update API. Settings are backed up to NVS before filesystem updates and auto-restored on boot.

**Key Changes:**

1. **CI/CD & Build Infrastructure**
   - GitHub Actions release workflow triggered on semantic version tags (`v*.*.*`)
   - Builds firmware and web UI with version injection
   - Generates `version_manifest.json` with SHA256 checksums
   - Creates GitHub Release with firmware.bin, littlefs.bin, firmware_merged.bin, version_manifest.json
   - Build scripts: `generate_version_manifest.py`, `merge_bin.py`

2. **HAL Layer (IHAL, HAL_ESP32, MockHAL)**
   - `HttpDataCallback` - chunked data callback with data pointer, length, bytes downloaded, total bytes
   - `httpGetStream()` - streaming HTTP download with data callback and redirect handling (301-308)
   - `httpGet()` - updated with HTTP redirect handling for GitHub URLs
   - OTA methods: `otaBegin()`, `otaWrite()`, `otaEnd()`, `otaAbort()`, `otaGetError()`
   - HAL_ESP32 uses ESP32 `Update.h` API (U_FLASH/U_SPIFFS)
   - MockHAL with full OTA simulation and tracking variables

3. **UpdateManager (lib/UpdateManager/)**
   - `checkForUpdates()` - fetches manifest JSON, parses with ArduinoJson, compares semantic versions
   - `installUpdate()` - streams firmware via httpGetStream, writes chunks via otaWrite, handles filesystem too
   - `update()` - periodic auto-check based on settings interval
   - `parseVersion()` / `isVersionNewer()` - semantic version comparison (major.minor.patch)
   - NVS settings backup before filesystem flash
   - Status tracking: IDLE, CHECKING, AVAILABLE, CURRENT, DOWNLOADING, INSTALLING, COMPLETE, ERROR

4. **REST API Endpoints**
   - `GET /update/check` (public) - triggers update check, returns manifest info
   - `GET /update/status` (public) - returns current update status/progress
   - `POST /update/install` (auth required) - starts firmware installation
   - OTA settings in `/update_settings`: auto_update_enabled, update_check_interval_hours, manifest_url

5. **main.cpp Integration**
   - UpdateManager instance created and initialized after web server
   - Periodic update eligibility check in main loop (every 60s)

6. **Web UI (Update.tsx, Settings.tsx)**
   - Full OTA UI: check button, version comparison, install with confirmation dialog
   - Progress bar with real-time status polling during download/install
   - Error display and ElegantOTA iframe fallback
   - Settings page: auto-update toggle, check interval input

**Testing (67 tests):**
- Version parsing: valid semver, pre-release suffix, build metadata, invalid formats, edge cases
- Version comparison: newer major/minor/patch, same version, older version, dev version handling
- checkForUpdates: network failure, invalid JSON, missing fields, URL tracking, timing
- installUpdate: success (firmware only, firmware+filesystem), OTA begin/write/end failures, abort behavior, progress tracking, restart verification
- Status/JSON: snapshot correctness, JSON format validation, state transitions
- Auto-update: interval checking, disabled mode, busy guard

**Files Modified/Created:**
- `lib/HAL/IHAL.h` - HttpDataCallback, httpGetStream, OTA methods
- `lib/HAL_ESP32/HAL_ESP32.h/.cpp` - ESP32 implementations with redirect handling
- `test/common/mocks/MockHAL.h` - OTA mock with tracking
- `lib/UpdateManager/UpdateManager.h/.cpp` - Full implementation from stubs
- `lib/CoopControllerWebServer/CoopControllerWebServer.h/.cpp` - OTA endpoints
- `src/main.cpp` - UpdateManager integration
- `web/src/Update.tsx` - Complete OTA UI
- `web/src/Settings.tsx` - OTA settings section
- `web/src/types.ts` - OTA settings types
- `test/unit_desktop/test_UpdateManager/test_UpdateManager.cpp` - 67 tests

**Build Verification:**
- ESP32: ✅ Firmware builds successfully
- Web UI: ✅ TypeScript/Vite compilation successful
- Tests: ✅ 570/570 passing (67 new UpdateManager tests)

---

## In Progress

*No features currently in progress.*

---

### Chart Enhancements (History Page) ✅

**Implemented:** 2026-02-11
**Status:** Complete

**Summary:**
Added interactive zoom/pan, time period filtering, and event-type-specific point style icons to all history charts.

**Key Changes:**
1. **Zoom & Pan** - Added `chartjs-plugin-zoom` for mouse wheel zoom and drag pan on all charts (x-axis only)
2. **Time Period Filter** - Button group (1h, 6h, 24h, All) to filter chart data by time window
3. **Event Type Icons** - Distinct point shapes for each event type: circle (temp), diamond (pump), triangle (flow), star (light), rounded rect (door)
4. **Reset Zoom** - Button to reset all charts to default zoom level
5. **Point Style Legend** - Visual reference card showing event marker symbols

**Files Modified:**
- `web/src/History.tsx` - Zoom plugin, time filter, event icons, point style legend
- `web/package.json` - Added `chartjs-plugin-zoom` dependency

---

### Git Commit SHA on Update Page ✅

**Implemented:** 2026-02-11
**Status:** Complete

**Summary:**
The Update page now displays the git commit SHA as a clickable link to the GitHub commit, alongside firmware version and build timestamps.

**Key Changes:**
- `GIT_COMMIT_SHA_RAW` build flag injected via platformio.ini from `GIT_COMMIT_SHA` env var (set by GitHub Actions)
- `config.h` exposes `gitCommitSha` and `githubRepo` as string constants
- `/version` endpoint returns `git_commit_sha` and `github_repo` in JSON response
- Update.tsx displays short SHA (7 chars) as a link to `https://github.com/{repo}/commit/{sha}`

**Files Modified:**
- `platformio.ini` - Added `GIT_COMMIT_SHA_RAW` build flag
- `include/config.h` - Added `gitCommitSha` and `githubRepo` config variables
- `lib/CoopControllerWebServer/CoopControllerWebServer.cpp` - Updated `/version` endpoint
- `web/src/Update.tsx` - Git SHA display with clickable link

---

## Planned Features

Features organized by priority and implementation status.

> **Flash Constraint Note:** Firmware is at 99.5% flash usage (7,183 bytes remaining). OTA Update System is designed to fit within this constraint using streaming downloads and PROGMEM optimization. Features requiring large external libraries remain blocked until flash optimization is performed.

### High Priority - Monitoring & Notifications (Blocked by Flash)

#### Email Notifications
- SMTP server/port, TLS and credentials configuration in web UI settings
- "From" email address configuration
- Notify on pump/water flow faults
- Notify on temperature sensor failures
- Notify on API failures (OpenWeather, OpenAI)
- Daily status and forecast reports
- Configurable notification times
- **Blocker:** Requires SMTP/TLS library (~20-40KB flash)

#### Telegram Integration
- Bot commands for basic controls (pump on/off, door open/close, get status)
- Status queries (door position, light state, temperature)
- Alert notifications for critical events
- Daily forecast and automation plan
- Approval/confirmation commands for AI door recommendations
- **Blocker:** Requires HTTP client library for Telegram API (~15-30KB flash)

#### External Pushbutton for Manual Pump Cycle
- Add support for external momentary pushbutton
- Single press triggers one complete pump cycle (ON time + OFF time)
- Useful for testing or manual water circulation
- Pin configuration in platformio.ini (requires approval)
- Debouncing and interrupt-driven detection
- Visual/audio feedback when activated

### Medium Priority - Door Automation

#### AI-Powered Door Decisions
- Daily AI recommendation based on:
  - Past weather (snow likely on ground?)
  - Daily forecast (precipitation, temperature)
  - Historical patterns (external configurable database to hold full event and weather history?)
- Confirmation options via:
  - Telegram bot command
  - Email response via link calling webserver
  - Web UI button
  - Home Assistant integration
- Safety override: manual control always available
- **Blocker:** Requires HTTP client for OpenAI API + significant flash

### Medium Priority - UI Improvements

#### Event-Driven Web UI Updates
- Replace polling-based status updates with Server-Sent Events (SSE) or WebSockets
- Push updates only when state changes occur
- Reduces network traffic and improves responsiveness
- Maintain fallback to polling for compatibility
- Implement on ESP32 using AsyncWebServer capabilities

### Medium Priority - API Integrations (Blocked by Flash)

#### OpenWeather API
- API key configuration in web UI settings
- Location configuration (coordinates queries from browser in web UI or zip code setting)
- Daily weather forecast retrieval
- Historical weather data for AI decision making
- Integration with door and pump automation
- **Blocker:** Requires HTTP client + JSON parsing (~15-25KB flash)

#### OpenAI-Compatible API
- Base URL and API key configuration in web UI
- Compatible with OpenAI, Anthropic Claude, or local models
- Decision engine for door recommendations
- Analysis of weather patterns and event history
- Customizable prompts from web UI
- **Blocker:** Requires HTTP client + JSON processing

#### GPS/Location Services
- Enter zip code OR latitude/longitude in web UI
- Request geolocation from browser
- Used for sunrise/sunset calculations (lat/long settings already exist)
- Used for OpenWeather API queries
- Store in settings for persistent use
- Note: Manual lat/long entry already works in Settings page

#### Home Assistant Integration
- MQTT settings configuration in web UI (broker, port, credentials)
- Expose entities: sensors, switches, lights
- Real-time status updates
- Remote control capabilities
- Automation integration
- Alert notifications via HA
- Auto discovery in Home Assistant
- **Blocker:** Requires MQTT library (~20-30KB flash)

#### Interactive Map for Location Setting
- Add interactive map component to Settings page
- Allow users to click/tap to set location coordinates
- Display current location marker on map
- Use lightweight map library (Leaflet or similar)
- Fall back to manual coordinate entry if map unavailable
- Show selected coordinates and approximate address
- **Blocker:** Map library too large for ESP32 LittleFS

### Medium Priority - Code Refactoring

#### Move Globals to Constructor/begin() Parameters
- Refactor global variables to be passed as constructor or begin() parameters
- Improves testability by enabling dependency injection
- Reduces hidden dependencies between components
- Makes component initialization more explicit
- Aligns with HAL refactoring patterns
- Priority components: SensorManager, PumpController, LightController

### Low Priority - Documentation & Clarifications

#### Door Test Mode Documentation
- Document what door test mode is and its purpose (it doesn't seem to do anything currently?)
- Explain when and why to use test mode
- Detail test mode behaviors and safety features
- Add to user documentation and web UI help text
- Include in API documentation
- Does it just need to be removed?

### Low Priority - Enhancements

#### Component Refactoring
- Change enums to enum class for type safety
- Update web server JSON handling to use string states for enum classes instead of numeric values for enums

#### Enhanced Testing
- Add unit tests for key components using Google Test framework for desktop/native testing and UnitTest for embedded testing
- Increase code coverage
- Automated testing in CI/CD pipeline
- Integration tests for API endpoints

#### Documentation
- Add more detailed inline documentation for complex functions
- Improve code comments
- Update diagrams and architecture docs
- Add troubleshooting guides
- Add full method and class Doxygen headers

---

## Notes

- All features are tested with desktop unit tests before deployment
- ESP32 firmware must build successfully before marking complete
- Web UI changes require successful build
- Documentation updated with each completed feature
- Implementation plans stored in `docs/temp_*.md` files
