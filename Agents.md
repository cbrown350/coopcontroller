# AGENTS.md - Coop Controller

> ESP32-based intelligent chicken coop automation system. Controls temperature monitoring, water pump freeze prevention, door automation, lighting, and remote monitoring via web UI.

## Quick Reference

| Item | Value |
|------|-------|
| **Platform** | ESP32 DevKit C v4 (ESP32-WROOM-32) |
| **Framework** | Arduino via PlatformIO |
| **C++ Standard** | C++17 (`-std=gnu++17`) |
| **Web UI** | SolidJS + Tailwind CSS + DaisyUI |
| **Build (firmware)** | `pio run` |
| **Build (web)** | `cd web && npm run build` |
| **Run tests** | `pio test` |
| **Current build** | RAM 17.6% (57,704 bytes), Flash 83.4% (1,474,941 bytes) |
| **Tests** | 576/576 passing (11 components) |
| **Web access** | `http://coopcontroller.local` |

---

## Documentation Index

| Document | Description |
|----------|-------------|
| [Architecture](docs/architecture.md) | System architecture, component descriptions, HAL, project structure, dependencies |
| [Hardware](docs/hardware.md) | Hardware requirements, pin configuration, wiring, PlatformIO change approval process |
| [API Reference](docs/api-reference.md) | All REST API endpoints (main controller) |
| [Development Guide](docs/development-guide.md) | Setup, build commands, coding standards, testing guide, ArduinoFake mock setup |
| [Feature Tracker](docs/feature-tracker.md) | Completed features, in-progress work, planned features roadmap |
| [Hardware Emulator](docs/hardware-emulator.md) | Emulator architecture, pin mapping, wiring, 40+ API endpoints, test scenarios |
| [Contributing](docs/contributing.md) | PR workflow, code review, branch management, security/sensitive files |
| [Troubleshooting](docs/troubleshooting.md) | Common issues, debug tools, recovery procedures |

---

## Critical Rules for AI Agents

### Before Writing Code
1. **Read before modifying** - Always read existing code before suggesting changes
2. **Clarify ambiguities** - Ask before assuming requirements
3. **Check existing patterns** - Follow established conventions in the codebase

### Compilation & Testing (Mandatory)
- **C++ changes:** `pio run` must pass with zero errors
- **Web changes:** `cd web && npm run build` must pass with zero TypeScript errors
- **Tests:** `pio test` must pass - no regressions
- **Subtasks are NOT complete until code compiles and tests pass**

### PlatformIO Configuration
**MANDATORY APPROVAL:** Any changes to `platformio.ini` (pins, libraries, build flags) require:
1. Documented justification
2. Pin conflict analysis
3. Explicit user approval before implementation

### Settings & Configuration
- All user-configurable values through SettingsManager (not hardcoded)
- Settings stored in `data/user_settings.json` on LittleFS
- Settings backed up to NVS before OTA filesystem updates (auto-restored on boot)
- New settings must be added to: SettingsManager, web UI Settings.tsx, `/get_settings` and `/update_settings` endpoints

### HAL Pattern
All ESP32-specific functions use the HAL abstraction (`IHAL.h`). New hardware interactions must go through HAL, not direct ESP32 API calls. This enables desktop unit testing via MockHAL.

### Testing Requirements
- Desktop tests use Google Test + ArduinoFake + MockHAL
- **CRITICAL:** Follow the exact ArduinoFake mock setup order (see [Development Guide](docs/development-guide.md#critical-arduinofake-mock-setup-order))
- WiFi, filesystem, web server interactions must be mocked for desktop tests

---

## Active Features

### Core Systems (All Active)
- **SensorManager** - Dual-purpose inputs (Dallas temp / water meter), auto-detection, per-pulse flow calculation
- **PumpController** - Temperature-based cycling, flow error detection, OFF-flow monitoring, scheduled maintenance
- **LightController** - PWM with sine-curve fades, manual/auto modes, timer functionality
- **DoorController** - Motor control, hall sensors, fault detection (hardware pending)
- **WifiController** - Auto-connect, AP fallback, mDNS
- **CoopControllerWebServer** - REST API, SPA serving, optional HTTP Basic Auth
- **SettingsManager** - JSON persistence in LittleFS, NVS backup/restore for OTA updates
- **Logger** - Level-based logging, syslog, circular buffer
- **SunriseSunset** - Location-based calculations with timezone support

### Recent Completions
- SHA256 OTA Verification (incremental SHA256 checksum verification during streaming downloads, aborts on mismatch)
- Configurable Device Hostname (user-editable setting in web UI, stored in SettingsManager, used for mDNS/WiFi AP/syslog/ArduinoOTA, device restart required, defaults: "CoopController" or "CoopHWEmulator")
- OTA Update System Complete (full OTA: manifest check, streaming firmware/filesystem download, ESP32 Update.h flash, NVS settings backup, redirect handling, REST API endpoints, web UI with progress, force reinstall option, 71 tests)
- NVS Settings Preservation for OTA Updates (backup to NVS before filesystem flash, auto-restore on boot, 3 new HAL NVS methods, OTA settings serialization fix, 14 new tests)
- Git Commit SHA on Update Page (clickable link to GitHub commit, build flag pipeline)
- Chart Enhancements (zoom/pan via chartjs-plugin-zoom, time period filter 1h/6h/24h/all, event-type point icons, reset zoom, point style legend)
- Historical Data Visualization - Event-Based (pure event-driven capture: pump/light/door on any change, temp at >=0.5°F delta with 60s min, flow at >0.001GPM with 10s min; granular trigger sources with 15 enum values; configurable intervals via web UI; Chart.js charts with event-type markers; CSV export)
- API Authentication (31 protected + 9 public endpoints)
- Factory Reset on Boot (hardware button)
- Scheduled Pump Maintenance Cycles
- Hardware Emulator (7 scenarios + custom + recordings + temp emulation)
- WiFi BSSID Preference (connect to specific access point)
- Web UI Theming (custom DaisyUI theme with logo)
- System Status Display, IP/MAC Display, Reboot Controls (audited & documented)

See [Feature Tracker](docs/feature-tracker.md) for full details and planned roadmap.

---

## Recent Security Updates

### ✅ SHA256 OTA Verification - February 13, 2026

**Critical security fix implemented:**
- **Issue:** SHA256 checksums were generated but never verified during OTA updates (CVSS 7.5 - HIGH severity)
- **Risk:** Vulnerable to corrupted downloads, MITM attacks, and malicious firmware injection
- **Fix:** Implemented incremental SHA256 verification during streaming downloads
- **Impact:** Firmware and filesystem updates now verified before installation; abort on mismatch
- **Files Changed:** `lib/UpdateManager/UpdateManager.h`, `lib/UpdateManager/UpdateManager.cpp`
- **Performance:** +265 bytes RAM, ~130ms verification overhead (negligible)
- **Testing:** ✅ Builds passing (firmware + web UI + desktop unit tests), ready for deployment
- **Documentation:** See [SHA256 Security Audit](docs/SHA256_SECURITY_AUDIT_2026-02-13.md) for full analysis

### ✅ Desktop Unit Test SHA256 Mock - February 13, 2026

**Issue:** Desktop unit tests failed to compile after SHA256 verification was added to UpdateManager, as the `mbedtls/sha256.h` header is ESP32-specific and unavailable in the native desktop test environment.

**Fix:** Created a portable SHA256 mock implementation at `test/unit_desktop/desktop_mocks/mbedtls/sha256.h` that provides the same API as mbedTLS for desktop testing.

**Result:** All 576 desktop unit tests now pass, including test_CoopControllerWebServer and test_UpdateManager.

**Remaining Security Work:**
- ⚠️ Fix TLS certificate validation (currently disabled via `setInsecure()`)
- 📋 Add code signing (RSA/ECDSA) for defense-in-depth
- 📋 Implement rollback protection

---

## Project Structure (Summary)

```
coop_controller/
├── lib/                    # Firmware components (each in own directory)
│   ├── HAL/IHAL.h          # Hardware abstraction interface
│   ├── HAL_ESP32/           # ESP32 HAL implementation
│   ├── CoopControllerWebServer/
│   ├── PumpController/
│   ├── SensorManager/
│   ├── LightController/
│   ├── DoorController/
│   ├── SettingsManager/
│   ├── WifiController/
│   ├── Logger/
│   ├── SunriseSunset/
│   ├── UpdateManager/
│   ├── HistoricalDataManager/
│   └── BuzzerController/
├── src/main.cpp             # Entry point
├── web/                     # SolidJS web application
├── test/                    # Unit tests (desktop + embedded)
│   └── common/mocks/MockHAL.h
├── emulate_hardware/        # Hardware emulator (separate ESP32)
├── build_scripts/           # CI/CD build helpers (manifest gen, binary merge)
├── .github/workflows/       # GitHub Actions (release pipeline)
├── docs/                    # Documentation subdocuments
├── platformio.ini           # Build configuration
├── Agents.md                # This file
└── CLAUDE.md                # Claude Code entry point
```

---

## Key References

- ESP32 pin layout: `docs/esp32_devkitC_v4_pinlayout.png`
- Pin definitions: `platformio.ini` (line 45+)
- Settings template: `data/user_settings.example.json`
- Setup instructions: `README.md`
