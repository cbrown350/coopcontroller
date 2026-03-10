# Feature Implementation Tracker

This document tracks in-progress and planned features. For completed features, see [feature-tracker-finished.md](feature-tracker-finished.md).

---

## Implementation Status Summary

| Phase | Status |
|-------|--------|
| Phase 3 (Hardware I/O) | 100% complete |
| Phase 3.5 (Critical Refactoring) | 100% complete - HAL refactoring complete, all ESP32-specific functions abstracted |
| Phase 3.5a (Sunrise/Sunset Integration) | 100% complete with accurate UTC to local time conversion |
| Phase 3.5b (Light Control with Web UI) | 100% complete |
| Phase 3.5c (Desktop Unit Testing) | 100% complete - All 590 desktop unit tests passing, all 12 core components covered |
| Phase 4 (Notifications) | In progress - Email & Telegram alerts implemented |

**Current Build:** RAM 29.8%, Flash 83.9%

**Latest Build (2026-03-09):** Firmware and web UI builds successful

**Core features:** Sensors, Pump, Light, Door, Buzzer, WiFi, WebServer, SunriseSunset, Settings, Logger, UpdateManager, HistoricalDataManager, NotificationManager controllers fully implemented. HAL refactoring complete: Desktop unit testing infrastructure fully functional with MockHAL and ArduinoFake. NVS-based settings preservation for OTA filesystem updates. OTA update system complete with SHA256 verification. Notification system with Telegram Bot API and HTTP-based email API integration.

**Test Coverage (March 2026):** 590/590 desktop tests passing (100% pass rate)

| Component | Tests |
|-----------|-------|
| BuzzerController | 3 |
| CoopControllerWebServer | 16 |
| DoorController | 68 |
| LightController | 102 |
| Logger | 11 |
| NotificationManager | 14 |
| PumpController | 83 |
| SensorManager | 33 |
| SettingsManager | 150 |
| SunriseSunset | 36 |
| UpdateManager | 71 |
| WifiController | 1 |

- **Embedded Unit Tests:** 1/1 passing - Logger singleton pattern test; HAL httpPost test created (requires device)
- **Test Infrastructure:** Complete mocking framework with MockHAL, MockSensorManager, MockBuzzerController

---

## Completed Features

**Total: 43 completed features**

All completed features have been moved to [feature-tracker-finished.md](feature-tracker-finished.md) for better navigation and reduced file size.

### Quick Reference Table

| Feature | Date | Section |
|---------|------|---------|
| Release Date Display on Update Page | 2026-02-13 | [Link](feature-tracker-finished.md#release-date-display-on-update-page-) |
| Auto-Close After Sunset Schedule Bug Fix | 2026-02-12 | [Link](feature-tracker-finished.md#auto-close-after-sunset-schedule-bug-fix-) |
| Configurable Device Hostname | 2026-02-12 | [Link](feature-tracker-finished.md#configurable-device-hostname-) |
| Historical Data Visualization (Event-Based) | 2026-02-10 | [Link](feature-tracker-finished.md#historical-data-visualization-event-based-) |
| API Authentication for Critical Endpoints | 2026-02-06 | [Link](feature-tracker-finished.md#api-authentication-for-critical-endpoints-) |
| Factory Reset on Boot | 2026-01-XX | [Link](feature-tracker-finished.md#factory-reset-on-boot-) |
| Water Meter Calibration | 2026-01-XX | [Link](feature-tracker-finished.md#water-meter-calibration-) |
| Scheduled Pump Maintenance Cycles | 2026-01-XX | [Link](feature-tracker-finished.md#scheduled-pump-maintenance-cycles-) |
| Pump Flow Per-Pulse Calculation | 2026-01-XX | [Link](feature-tracker-finished.md#pump-flow-per-pulse-calculation-) |
| Pump OFF Flow Monitoring | 2026-01-XX | [Link](feature-tracker-finished.md#pump-off-flow-monitoring-) |
| SPA Routing Fix | 2026-01-XX | [Link](feature-tracker-finished.md#spa-routing-fix-) |
| HAL Refactoring | 2026-01-XX | [Link](feature-tracker-finished.md#hal-refactoring-) |
| Sunrise/Sunset UTC Conversion Fix | 2026-01-XX | [Link](feature-tracker-finished.md#sunrisesunset-utc-conversion-fix-) |
| Light Control Regression Fix | 2026-01-XX | [Link](feature-tracker-finished.md#light-control-regression-fix-) |
| Component Naming Refactoring | 2026-01-XX | [Link](feature-tracker-finished.md#component-naming-refactoring-) |
| WiFi Controller Refactoring | 2026-01-XX | [Link](feature-tracker-finished.md#wifi-controller-refactoring-) |
| Logger Method Refactoring | 2026-01-XX | [Link](feature-tracker-finished.md#logger-method-refactoring-) |
| Remote Syslog Runtime Configuration | 2026-02-XX | [Link](feature-tracker-finished.md#remote-syslog-runtime-configuration-) |
| Configurable Flow Calculation Interval | 2026-02-XX | [Link](feature-tracker-finished.md#configurable-flow-calculation-interval-) |
| NTP WiFi Safety Guard | 2026-02-XX | [Link](feature-tracker-finished.md#ntp-wifi-safety-guard-) |
| Postman API Collection | 2026-02-XX | [Link](feature-tracker-finished.md#postman-api-collection-) |
| Mobile UI Optimization | 2026-02-XX | [Link](feature-tracker-finished.md#mobile-ui-optimization-) |
| Web Assets Security Refactoring | 2026-01-XX | [Link](feature-tracker-finished.md#web-assets-security-refactoring-) |
| WiFi Status LED | 2026-01-XX | [Link](feature-tracker-finished.md#wifi-status-led-) |
| ESP32 Watchdog | 2026-01-XX | [Link](feature-tracker-finished.md#esp32-watchdog-) |
| Buzzer Alerts Integration | 2026-01-XX | [Link](feature-tracker-finished.md#buzzer-alerts-integration-) |
| Automatic Door Close After Sunset | 2026-01-XX | [Link](feature-tracker-finished.md#automatic-door-close-after-sunset-) |
| Door Progress Calculation | 2026-01-XX | [Link](feature-tracker-finished.md#door-progress-calculation-) |
| Door Lockout Toggle | 2026-02-06 | [Link](feature-tracker-finished.md#door-lockout-toggle-) |
| Door Timeout Auto-Calculation | 2026-02-06 | [Link](feature-tracker-finished.md#door-timeout-auto-calculation-) |
| Refactor main.cpp WiFi Functions | 2026-01-XX | [Link](feature-tracker-finished.md#refactor-maincpp-wifi-functions-) |
| System Status Display | 2026-02-XX | [Link](feature-tracker-finished.md#system-status-display-) |
| IP Address and MAC Address Display | 2026-02-XX | [Link](feature-tracker-finished.md#ip-address-and-mac-address-display-) |
| Reboot Controls | 2026-02-XX | [Link](feature-tracker-finished.md#reboot-controls-) |
| Floating UI Elements for Settings Page | 2026-02-XX | [Link](feature-tracker-finished.md#floating-ui-elements-for-settings-page-) |
| Improved Connection Status | 2026-02-XX | [Link](feature-tracker-finished.md#improved-connection-status-) |
| Door Control (Full) | 2026-02-XX | [Link](feature-tracker-finished.md#door-control-) |
| Network Safety (NTP Guard) | 2026-02-XX | [Link](feature-tracker-finished.md#network-safety-ntp-guard-) |
| WiFi BSSID Preference | 2026-02-09 | [Link](feature-tracker-finished.md#wifi-bssid-preference-) |
| Web UI Theming Based on Logo | 2026-02-09 | [Link](feature-tracker-finished.md#web-ui-theming-based-on-logo-) |
| NVS Settings Preservation for OTA Updates | 2026-02-11 | [Link](feature-tracker-finished.md#nvs-settings-preservation-for-ota-updates-) |
| OTA Update System | 2026-02-11 | [Link](feature-tracker-finished.md#ota-update-system-) |
| Chart Enhancements (History Page) | 2026-02-11 | [Link](feature-tracker-finished.md#chart-enhancements-history-page-) |
| Git Commit SHA on Update Page | 2026-02-11 | [Link](feature-tracker-finished.md#git-commit-sha-on-update-page-) |

---

## In Progress

*No features currently in progress.*

## Recently Completed (Not Yet Moved to Finished)

### Email & Telegram Notification Alerts (2026-03-09)
- **Telegram**: Bot token & chat ID configuration, HTTPS POST to Telegram Bot API
- **Email**: HTTP-based email API integration (smtp2go, Mailgun, etc.) — avoids heavy SMTP library
- **Alert preferences**: Per-alert-type toggles (pump error, sensor error, door fault, WiFi disconnect, system error)
- **Rate limiting**: 60-second per-alert-type cooldown prevents notification flooding
- **HAL addition**: `httpPost()` method added to IHAL/HAL_ESP32 using WiFiClientSecure raw sockets
- **Web UI**: Full settings UI with Telegram/Email cards, test buttons, alert preference checkboxes
- **Settings**: 17 new fields persisted via SettingsManager with NVS backup
- **Web Server**: 3 new endpoints (`/notifications/test/telegram`, `/notifications/test/email`, `/notifications/status`)
- **Tests**: 14 desktop unit tests covering all notification scenarios
- **Files**: `lib/NotificationManager/`, IHAL.h, HAL_ESP32, SettingsManager, CoopControllerWebServer, main.cpp, web/src/Settings.tsx

---

## Planned Features

Features organized by priority and implementation status.

> **Flash Constraint Note:** Firmware is at 83.9% flash usage. Features requiring large external libraries (MQTT) may still be constrained by flash. Email uses HTTP-based API (not SMTP) and Telegram uses direct HTTPS POST to keep flash usage minimal.

### High Priority - Monitoring & Notifications (Partially Complete)

#### Email Notifications - DONE (basic alerts via HTTP email API)
- Remaining: Daily status/forecast reports, configurable notification times, API failure alerts

#### Telegram Integration - DONE (basic alert notifications)
- Remaining: Bot commands for controls, status queries, daily forecast, approval commands

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
- Requires HTTP client for OpenAI API + significant flash

#### External Pushbutton for Manual Pump Cycle
- Add support for external momentary pushbutton
- Single press triggers one complete pump cycle (ON time + OFF time)
- Useful for testing or manual water circulation
- Pin configuration in platformio.ini (requires approval); use one with an internal pull-up resistor option
- Debouncing and interrupt-driven detection
- Visual/audio feedback when activated

### Medium Priority - UI Improvements

#### Event-Driven Web UI Updates
- Replace polling-based status updates with Server-Sent Events (SSE) or WebSockets
- Push updates only when state changes occur
- Reduces network traffic and improves responsiveness
- Maintain fallback to polling for compatibility
- Implement on ESP32 using AsyncWebServer capabilities
- OTA Update System - show update-available badge when auto-checking is enabled and new version is detected without user needing to check anything
- Door has option for auto-close after sunset and also auto open/close for sunrise/sunset; the sunset portion is redundant and should be broken into two separate sections, one for sunrise (auto-open) and leave the one for sunset (auto-close)

#### Enhance UI
- Hide text boxes and settings for all items that are disabled (e.g. hide pump on/off time settings when pump is disabled)
- Add tooltips or help text for each setting to explain what it does

### Medium Priority - API Integrations

#### OpenWeather API
- API key configuration in web UI settings
- Location configuration (coordinates queries from browser in web UI or zip code setting)
- Daily weather forecast retrieval
- Historical weather data for AI decision making
- Integration with door and pump automation
- Requires HTTP client + JSON parsing (~15-25KB flash)

#### OpenAI-Compatible API
- Base URL and API key configuration in web UI
- Compatible with OpenAI, Anthropic Claude, or local models
- Decision engine for door recommendations
- Analysis of weather patterns and event history to make a recommendation (open/close/keep)
- Customizable prompts from web UI
- Requires HTTP client + JSON processing

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
- Requires MQTT library (~20-30KB flash)

#### Interactive Map for Location Setting
- Add interactive map component to Settings page
- Allow users to click/tap to set location coordinates
- Display current location marker on map
- Use lightweight map library (Leaflet or similar)
- Fall back to manual coordinate entry if map unavailable
- Show selected coordinates and approximate address
- Lib available: celliesprojects/OpenStreetMap-esp32

### Medium Priority - Code Refactoring

#### Move Globals to Constructor/begin() Parameters - may be mostly complete when refactored for unit testing?
- Refactor global variables to be passed as constructor or begin() parameters
- Improves testability by enabling dependency injection
- Reduces hidden dependencies between components
- Makes component initialization more explicit
- Aligns with HAL refactoring patterns
- Priority components: SensorManager, PumpController, LightController

#### OTA Enhancements
- Is there a way to verify works after update and reboot and revert if doesn't meet certain criteria?

#### History Visualization
- Add option in UI to add external SQL server to post data remotely and visualize locally with filters; serve a page that embedsh Grafana or similar?; would require implementing a client for the chosen database (InfluxDB, MySQL, etc.) and adding configuration options in the web UI

#### Mobile App
- Optimize web UI for mobile devices or create a dedicated mobile app using React Native or Flutter; would require significant UI adjustments and testing across different screen sizes and platforms
- Implement push notifications for critical alerts (pump failure, door fault) using Firebase Cloud Messaging or similar service; would require integration with a push notification service and handling device tokens
- Chat interface for AI door recommendations and status queries; would require implementing a chat UI and connecting it to the OpenAI-compatible API

### Low Priority - Documentation & Clarifications

#### Door Test Mode Documentation
- Document what door test mode is and its purpose (it doesn't seem to do anything currently?)
- Explain when and why to use test mode
- Detail test mode behaviors and safety features
- Add to user documentation and web UI help text
- Include in API documentation
- Does it just need to be removed?

### Low Priority - Enhancements

#### Door Operation
- Door calculated progress doesn't seem to work, progress indicator is either 0% or 100%; maybe door moves too fast?

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
