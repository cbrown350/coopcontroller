# AGENTS.md - Coop Controller

> ESP32-based intelligent chicken coop automation system. Controls temperature monitoring, water pump freeze prevention, door automation, lighting, and remote monitoring via web UI.

## Quick Reference

| Item | Value |
|------|-------|
| **Platform** | ESP32 DevKit C v4 (ESP32-WROOM-32) |
| **Framework** | Arduino via PlatformIO |
| **C++ Standard** | C++11 (`-std=gnu++11`) |
| **Web UI** | SolidJS + Tailwind CSS + DaisyUI |
| **Build (firmware)** | `pio run` |
| **Build (web)** | `cd web && npm run build` |
| **Run tests** | `pio test` |
| **Current build** | RAM 17.2%, Flash 82.5% |
| **Tests** | 469/469 passing (10 components) |
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
- **SettingsManager** - JSON persistence in LittleFS
- **Logger** - Level-based logging, syslog, circular buffer
- **SunriseSunset** - Location-based calculations with timezone support

### Recent Completions
- API Authentication (30 protected + 7 public endpoints)
- Factory Reset on Boot (hardware button)
- Scheduled Pump Maintenance Cycles
- Hardware Emulator (7 scenarios + custom + recordings + temp emulation)

See [Feature Tracker](docs/feature-tracker.md) for full details and planned roadmap.

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
│   └── BuzzerController/
├── src/main.cpp             # Entry point
├── web/                     # SolidJS web application
├── test/                    # Unit tests (desktop + embedded)
│   └── common/mocks/MockHAL.h
├── emulate_hardware/        # Hardware emulator (separate ESP32)
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
