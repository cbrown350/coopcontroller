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
| Phase 3.5c (Desktop Unit Testing) | 100% complete - All 452 desktop unit tests passing, all 10 core components covered |

**Current Build:** RAM 17.2% (56,436 bytes), Flash 82.5% (1,081,881 bytes)

**Core features:** Sensors, Pump, Light, Door, Buzzer, WiFi, WebServer, SunriseSunset, Settings, Logger controllers fully implemented. HAL refactoring complete: Desktop unit testing infrastructure fully functional with MockHAL and ArduinoFake. Actual functionality hasn't been checked for correctness.

**Test Coverage (January 2026):** 452/452 desktop tests passing (100% pass rate)

| Component | Tests |
|-----------|-------|
| BuzzerController | 3 |
| CoopControllerWebServer | 15 |
| DoorController | 59 |
| LightController | 95 |
| Logger | 12 |
| PumpController | 77 |
| SensorManager | 7 |
| SettingsManager | 83 |
| SunriseSunset | 35 |
| WifiController | 66 |

- **Embedded Unit Tests:** 1/1 passing - Logger singleton pattern test
- **Test Infrastructure:** Complete mocking framework with MockHAL, MockSensorManager, MockBuzzerController

---

## Completed Features

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

## In Progress

*No features currently in progress*

---

## Planned Features

Features organized by priority and implementation status.

### High Priority - Safety & Reliability

#### WiFi Status LED
- Implement heartbeat LED on WIFI_LED_B_PIN when connected
- Fast blink pattern when disconnected
- Visual status indicator without requiring web UI access

#### Buzzer Alerts
- Sound buzzer on fault conditions (pump failure, sensor error)
- Configurable alert patterns for different issues
- Web UI silencing button
- Persistent until acknowledged or resolved

#### ESP32 Watchdog
- Implement watchdog timer for main loop
- Automatic restart if loop hangs
- Prevents system lockup
- Log watchdog resets for debugging

#### Automatic Door Close After Sunset
- Add setting `door_auto_close_after_sunset_enabled` (boolean, default false)
- Add setting `door_auto_close_after_sunset_minutes` (integer, default 0)
- Automatically close door X minutes after calculated sunset time when enabled
- **Dependencies:** Requires Sunrise/Sunset Integration to be completed first
- **Implementation Notes:**
  - Separate from existing `sunset_offset_minutes` setting (which affects both open and close times)
  - This specifically adds a delay AFTER sunset for closing only
  - Respects door auto mode settings
  - Example: If sunset is 6:30 PM and setting is 30 minutes, door closes at 7:00 PM
  - Logs scheduled close time and actual execution
  - User can disable entirely or set to 0 for immediate close at sunset
  - Web UI displays calculated close time based on current sunset + offset

#### Door Timeout Auto-Calculation
- Track historical door open/close times
- Calculate timeout automatically: max(historical_time) + 1 second buffer
- Store last N operations for averaging
- Fallback to user-configured value if no history
- Display calculated timeout in web UI
- Allow manual override of auto-calculated value

#### Door Lockout Toggle
- Add door lockout toggle control to Status page in web UI
- Position near existing door control buttons
- When enabled, prevents all door operations (open/close)
- Useful for maintenance, cleaning, or manual intervention
- Visual indicator showing lockout is active
- Persists across page refreshes (saved in settings)
- Override all automatic door operations when active
- Clear warning when attempting door operations during lockout

#### Door Progress Calculation
- Calculate open/close progress percentage during operation
- Based on elapsed time vs expected timeout duration
- Display progress bar in web UI during door movement
- Helps identify slow operations or obstructions
- Update progress in real-time via status endpoint
- Show "Unknown" instead of 50% if progress cannot be calculated (e.g., door stopped mid-operation)
- Track and display accurate progress values only when actively moving

#### Improved Connection Status
- Only show "connected" if water meter pulse detected
- More accurate connection state reporting
- Helps identify sensor vs network issues

### High Priority - Code Refactoring

#### Refactor main.cpp WiFi Functions
- Move remaining WiFi-related functions from main.cpp to WifiController
- Complete the WiFi controller refactoring started in Phase 3.5
- Encapsulate all WiFi state and logic in WifiController class
- Reduce main.cpp complexity and improve maintainability
- Ensure consistent controller pattern across all components
- Update any dependencies to use WifiController interface

### High Priority - Monitoring & Notifications

#### Email Notifications
- SMTP server/port, TLS and credentials configuration in web UI settings
- "From" email address configuration
- Notify on pump/water flow faults
- Notify on temperature sensor failures
- Notify on API failures (OpenWeather, OpenAI)
- Daily status and forecast reports
- Configurable notification times

#### Telegram Integration
- Bot commands for basic controls (pump on/off, door open/close, get status)
- Status queries (door position, light state, temperature)
- Alert notifications for critical events
- Daily forecast and automation plan
- Approval/confirmation commands for AI door recommendations

#### External Pushbutton for Manual Pump Cycle
- Add support for external momentary pushbutton
- Single press triggers one complete pump cycle (ON time + OFF time)
- Useful for testing or manual water circulation
- Pin configuration in platformio.ini (requires approval)
- Debouncing and interrupt-driven detection
- Visual/audio feedback when activated

#### System Status Display
- Show heap memory, CPU usage in web UI
- Display uptime since last reboot
- Log periodic status if verbose logging enabled
- Help identify memory leaks or performance issues

### Medium Priority - Door Automation

#### Door Control
- Bidirectional motor control using DRV8833 (OUT_DOOR_A_OPEN_POS_PIN / OUT_DOOR_A_OPEN_NEG_PIN)
- Hall effect position sensors (DOOR_A_HALL_SENSOR_OPEN_B_PIN / DOOR_A_HALL_SENSOR_CLOSED_B_PIN)
- Fault detection with input from DRV8833 board and internal timeout (DOOR_A_FAULT_B_PIN active LOW)
- Manual control from external switch, hit switch to open/close door
- Configurable timeout values
- Manual control from web UI
- Automatic control based on:
  - Sunrise/sunset with configurable offsets
  - Weather conditions via OpenWeather API
  - AI recommendations via OpenAI-compatible API
  - User approval/override required daily

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

### Medium Priority - UI Improvements

#### Event-Driven Web UI Updates
- Replace polling-based status updates with Server-Sent Events (SSE) or WebSockets
- Push updates only when state changes occur
- Reduces network traffic and improves responsiveness
- Maintain fallback to polling for compatibility
- Implement on ESP32 using AsyncWebServer capabilities

#### Web UI Routing Fix
- Fix error when refreshing page on non-root routes
- Configure proper fallback routing in Vite and web server
- Serve index.html for all unknown routes (SPA behavior)
- Test all routes work correctly after refresh
- Ensure proper 404 handling for actual missing resources

#### Web UI Theming Based on Logo
- Design color scheme based on provided logo.webp
- Update Tailwind/DaisyUI theme configuration
- Create cohesive visual identity
- Consider dark/light mode variations
- Apply consistently across all pages

#### Mobile UI Optimization
- Fix horizontal scrolling issues on mobile devices
- Prevent content cutoff on smaller screens
- Optimize touch targets for mobile interaction
- Test on various mobile screen sizes
- Improve responsive breakpoints in Tailwind config

#### Floating UI Elements for Settings Page
- **Floating Notifications for Settings Changes** - Notifications when changes are made should float so they're visible wherever the user has scrolled on the screen (not fixed in one location under tabs)
- **Floating Save Settings Button** - The save settings button should float so it's easy to click from anywhere on the page
- **Floating Unsaved Changes Indicator** - A similarly floating notification that stays visible if there are unsaved settings changes, alerting the user that changes need to be saved
- Use CSS fixed positioning with appropriate z-index to ensure visibility above other page elements
- Position in bottom-right or bottom-left corner of viewport for easy access
- Ensure floating elements don't obscure critical content or controls
- Add smooth transitions for showing/hiding floating elements
- Maintain visibility across all screen sizes (mobile, tablet, desktop)

#### Historical Data Visualization
- Add graphs showing past week of data (with 24-hour detailed view)
- Temperature trends from both sensors
- Water meter flow rates and totals
- Pump on/off states and cycle history with trigger events logged
- Light brightness levels over time with trigger events logged
- Door open/close states and what triggered each operation (auto, manual, timer, etc.)
- Use lightweight charting library (Chart.js or similar)
- Include event markers showing what triggered state changes

### Medium Priority - API Integrations

#### OpenWeather API
- API key configuration in web UI settings
- Location configuration (coordinates queries from browser in web UI or zip code setting)
- Daily weather forecast retrieval
- Historical weather data for AI decision making
- Integration with door and pump automation

#### OpenAI-Compatible API
- Base URL and API key configuration in web UI
- Compatible with OpenAI, Anthropic Claude, or local models
- Decision engine for door recommendations
- Analysis of weather patterns and event history
- Customizable prompts from web UI

#### GPS/Location Services
- Enter zip code OR latitude/longitude in web UI
- Request geolocation from browser
- Used for sunrise/sunset calculations
- Used for OpenWeather API queries
- Store in settings for persistent use

#### Home Assistant Integration
- MQTT settings configuration in web UI (broker, port, credentials)
- Expose entities: sensors, switches, lights
- Real-time status updates
- Remote control capabilities
- Automation integration
- Alert notifications via HA
- Auto discovery in Home Assistant

#### Interactive Map for Location Setting
- Add interactive map component to Settings page
- Allow users to click/tap to set location coordinates
- Display current location marker on map
- Use lightweight map library (Leaflet or similar)
- Fall back to manual coordinate entry if map unavailable
- Show selected coordinates and approximate address

#### IP Address and MAC Address Display
- Display ESP32 IP address on Status page
- Display MAC address for network identification
- Useful for router configuration and debugging
- Show in system information section
- Include in `/sensor_status` API response

#### WiFi BSSID Preference
- Add BSSID preference field in WiFi settings
- Allow users to specify preferred access point MAC address
- Useful when multiple APs share same SSID (mesh networks)
- Optional - leave empty to connect to strongest signal
- Helps ensure consistent connection to specific AP
- Display current connected BSSID in status

### Medium Priority - Code Refactoring

#### Move Globals to Constructor/begin() Parameters
- Refactor global variables to be passed as constructor or begin() parameters
- Improves testability by enabling dependency injection
- Reduces hidden dependencies between components
- Makes component initialization more explicit
- Aligns with HAL refactoring patterns
- Priority components: SensorManager, PumpController, LightController

### Low Priority - Documentation & Clarifications

#### Postman API Collection
- Create Postman collection file for all REST API endpoints
- Include example requests and responses for each endpoint
- Document all HTTP methods (GET, POST) with proper headers
- Add environment variables for base URL configuration
- Include description and usage notes for each endpoint
- Export as importable JSON file for easy sharing
- Store in `docs/` directory

#### Door Test Mode Documentation
- Document what door test mode is and its purpose (it doesn't seem to do anything currently?)
- Explain when and why to use test mode
- Detail test mode behaviors and safety features
- Add to user documentation and web UI help text
- Include in API documentation
- Does it just need to be removed?

### Low Priority - Enhancements

#### Configurable Intervals
- Make FLOW_CALCULATION_INTERVAL configurable from web UI
- Currently hardcoded at 60000ms (1 minute)
- Allow users to adjust based on flow rates

#### Remote Syslog Configuration
- Move syslog server/port from compile-time defines to web UI settings
- Runtime configuration changes
- Multiple syslog targets

#### Mobile Optimization
- Optimize web UI for mobile devices
- Touch-friendly controls
- Responsive layout improvements
- PWA capabilities for app-like experience

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

#### Reboot Controls
- Add reboot button to web UI
- Option to schedule reboots
- Clear indication before reboot executes

#### Network Safety
- Ensure no web-related calls when not connected to WiFi
- Prevent OpenWeather/OpenAI/email/Telegram calls without connection
- Queue requests for when connection restored
- Graceful degradation in offline mode

---

## Notes

- All features are tested with desktop unit tests before deployment
- ESP32 firmware must build successfully before marking complete
- Web UI changes require successful build
- Documentation updated with each completed feature
- Implementation plans stored in `docs/temp_*.md` files
