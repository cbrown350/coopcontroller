# Development Guide

## Setup Instructions

1. **Install PlatformIO**
   - Option A: Install PlatformIO IDE (standalone)
   - Option B: Install VSCode + PlatformIO extension (recommended)

2. **Clone Repository** and `cd coop_controller`

3. **Install Dependencies**
   - PlatformIO auto-downloads required libraries on first build
   - For web development: `cd web && npm install`

4. **Configure Build Environment**
   - Set firmware version in `platformio.ini`
   - Optional: Set OTA and AP passwords in environment or `platformio.ini`

5. **MCP Tools Setup** (for AI-assisted development)
   - Use Context7 for latest library documentation references
   - Use Playwright for web UI testing
   - Use appropriate MCP tools as needed (Brave Search for web lookups, etc.)

---

## Key Commands

### Firmware

```bash
pio run                         # Build firmware
pio run --target upload         # Upload to device
pio device monitor              # Serial monitor (115200 baud)
pio run -t clean                # Clean build files
pio test                        # Run unit tests
pio run --target uploadfs       # Upload filesystem
```

### Web UI

```bash
cd web
npm install                     # Install dependencies
npm run dev                     # Dev server with mock API
npm run build                   # Build and copy to /data for ESP32
```

### Upload Port

Set in `platformio.ini`:

```ini
upload_port = COM22              # Windows
; upload_port = /dev/ttyUSB0     # Linux
; upload_port = /dev/cu.usbserial-*  # macOS
```

Or use OTA:

```ini
upload_protocol = espota
upload_port = coopcontroller.local
upload_flags = --auth=<password>
```

---

## Compilation & Testing Requirements

**All code changes MUST be verified before marking complete:**

| Change Type | Command | Must Pass |
|-------------|---------|-----------|
| C++ firmware | `pio run` | Zero errors, acceptable RAM/Flash |
| Web UI | `cd web && npm run build` | Zero TypeScript errors |
| Unit tests | `pio test` | All tests pass |

**Quality Standards:**

- No code should be submitted without successful compilation
- Build errors indicate incomplete implementation
- Subtasks are NOT complete until code compiles successfully
- Test basic functionality when possible
- Warnings should be reviewed and addressed if relevant
- Verify flash and RAM usage remain acceptable

---

## Coding Style

### C++ Standards

- **Formatting:** clang-format, 2-space indent, 120 char max line
- **Naming:** camelCase (variables), PascalCase (classes), UPPER_SNAKE_CASE (constants)
- **Headers:** `#pragma once`, forward declarations, organized includes (stdlib -> ESP32 -> project). Document all public methods with Doxygen-style comments.
- **Source Files:** Include corresponding header first, use descriptive variable names, add comments for complex logic, use `const` and `constexpr` where appropriate, prefer range-based for loops
- **Memory:** RAII, avoid raw pointers prefer smart pointers, minimize `String` usage (fragmentation risk), be mindful of ESP32 memory constraints
- **Errors:** Return codes for recoverable errors, assertions for programmer errors, level-appropriate logging, handle ESP32-specific error conditions
- **C++ Standard:** C++11 (`-std=gnu++11`)
- All code must use the standards for the latest versions of libraries and frameworks
- Must not use deprecated APIs, features, functions, or methods

### JavaScript/TypeScript Standards

- **Formatting:** Prettier, 2-space indent, 100 char max line
- **SolidJS:** Use signals for state, createEffect for side effects, JSX templates, follow SolidJS best practices for performance
- **TypeScript:** Strict mode, interfaces for object shapes, explicit return types, generic types where appropriate

### Git Commits

```
type(scope): description
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

---

## Component/Feature Creation Rules

### New Component Checklist

1. **Plan:** Define interfaces, consider memory/performance, plan testing
2. **Implement:** Follow existing patterns, comprehensive error handling, appropriate logging, dependency injection
3. **Integrate:** Update `main.cpp`, add SettingsManager config, implement web UI controls, add REST endpoints
4. **Test:** Unit tests, integration tests, hardware testing when applicable

### Class Design Principles

- **Single Responsibility** - One clear purpose per class; avoid god classes with too many responsibilities
- **Interface Segregation** - Minimal, focused interfaces; avoid forcing clients to depend on unused methods
- **Composition over Inheritance** - Dependency injection for testability; minimize coupling between components
- **RAII** - Clean resource management, handle ESP32 constraints; clean up resources in destructors

### Settings Integration

- All user-configurable values through SettingsManager
- Use meaningful setting names with reasonable default values
- Add to `Settings.tsx` with appropriate input types (number, toggle, select)
- Provide clear labels and descriptions in web UI
- Include in `/get_settings` and `/update_settings` endpoints
- Validate input values before saving
- Trigger restart only for WiFi changes

### API Integration

- Include new settings in `/get_settings` response
- Handle updates in `/update_settings` endpoint
- Validate settings before applying
- Trigger restart only if necessary (usually only for WiFi changes)

---

## PlatformIO Configuration Changes

**MANDATORY APPROVAL REQUIREMENT:** Any modifications to `platformio.ini` (build flags, pins, libraries, etc.) must be proposed first with detailed justification and require explicit user approval before implementation. This prevents unintended hardware conflicts or configuration changes.

**Required Approval Process:**

1. **Documentation Update First:** Propose changes in documentation updates before any code implementation
2. **Detailed Justification:** Clearly explain the purpose, hardware implications, and necessity of each change
3. **Conflict Analysis:** Verify no conflicts with existing pin assignments or ESP32 reserved pins
4. **User Confirmation:** Get explicit approval before implementing platformio.ini changes

**Examples Requiring Approval:**

- Pin definitions: `-DBUZZER_B_PIN=27` or `-DOUT_DOOR_A_OPEN_POS_PIN=14`
- Library additions or version changes
- Build flag modifications affecting compilation
- Upload protocol or port configuration changes

**Temporary Implementation Guidelines:**

- For development: All pins should be defined in platformio.ini using `-D` flags
- For production: Propose platformio.ini build flags after approval
- Request approval via orchestrator before finalizing any platformio.ini modifications

### Hardware Pin Approval Guidelines

Before adding new hardware pins, developers must:

1. **Pin Availability Verification:** Check against existing pin assignments and ESP32 reserved pins
2. **Documentation Update:** Add proposed pins to "Pins Defined for Future Implementation" table in hardware.md first
3. **User Approval Request:** Get explicit approval for platformio.ini changes
4. **Conflict Prevention:** Avoid boot pins (GPIO 0, 2, 15) and SPI flash pins (GPIO 6-11)
5. **Development Guidelines:** For temporary development, pin changes should be defined in platformio.ini with conflicts commented out with approval; production implementation requires full platformio.ini approval

---

## Factory Reset Procedure

**To perform a factory reset:**

### 1. Via Hardware (Recommended)

Hold the door manual switch for 20 seconds during boot. WiFi LED rapid blinks to confirm. Clears all settings and WiFi credentials.

### 2. Via Web UI

- Navigate to Settings page
- Click "Factory Reset" button
- Confirm action
- Device will restart in AP mode with default settings

### 3. Via Serial Console

- Connect to device via serial monitor
- Send command (when implemented): `FACTORY_RESET`
- Confirm action
- Device will restart

### 4. Manual Filesystem Erase

```bash
pio run --target erase
pio run --target uploadfs
```

### Reset Behavior

- Clears all settings in `user_settings.json`
- Removes WiFi credentials
- Sets AP mode active
- Reverts to default values for all configurable parameters
- Preserves firmware and web UI files
- Creates new AP network `CoopController`

---

## Testing & Quality Assurance

### Unit Testing Framework

- **Desktop tests:** Google Test framework with MockHAL and ArduinoFake
- **Embedded tests:** UnitTest framework
- **Coverage target:** >80% of public methods and edge cases
- WiFi, filesystem, web server and any interactions not covered by ArduinoFake should be mocked for desktop tests

### Test Organization

- One test file per source component
- Descriptive test names, grouped in test suites
- Setup/teardown for common initialization

**Example Test Structure:**

```cpp
// test/test_pump_controller.cpp
#include <gtest/gtest.h>
#include "PumpController.h"

class PumpControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Test cleanup
    }

    PumpController pump_;
};

TEST_F(PumpControllerTest, TurnsOnWhenTemperatureBelowThreshold) {
    // Test implementation
}
```

### CRITICAL: ArduinoFake Mock Setup Order

When writing desktop unit tests, the mock setup order prevents segfaults. **MUST follow this sequence in `SetUp()`:**

```cpp
void SetUp() override {
    // 1. Create HAL mock FIRST
    mockHal = new MockHAL();

    // 2. Reset ArduinoFake
    ArduinoFakeReset();

    // 3. Mock ALL Arduino functions BEFORE any component init
    When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return currentMillis; });
    When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), digitalWrite)).AlwaysDo([this](uint8_t pin, uint8_t val) {
        pinStates[pin] = val;
    });
    When(Method(ArduinoFake(), digitalRead)).AlwaysDo([this](uint8_t pin) -> int {
        return pinStates[pin];
    });

    // 4. CRITICAL: Mock interrupt functions (segfault without these)
    When(OverloadedMethod(ArduinoFake(), attachInterrupt,
        void(uint8_t, void(*)(), int))).AlwaysReturn();
    When(Method(ArduinoFake(), detachInterrupt)).AlwaysReturn();

    // 5. Initialize Logger AFTER Arduino mocks
    Logger::getInstance().begin(mockHal);

    // 6. Create component mocks and test instances
    // ...
}
```

**Why this order matters:**

- **Arduino Function Mocks First**: Any component initialization (including Logger) may call Arduino functions like `millis()`, `digitalWrite()`, etc. These MUST be mocked before they are called.
- **Logger After millis()**: `Logger::getInstance().begin(mockHal)` MUST be called AFTER `millis()` is mocked because Logger instantiation calls `millis()`. Calling it before will cause a crash.
- **Interrupt Functions**: `attachInterrupt()` and `detachInterrupt()` are caught by ArduinoFake's FunctionFake.cpp. If not mocked, they will cause segmentation faults (Program errored with code 3221225477 on Windows).
- **All FunctionFake Functions**: Any function caught by ArduinoFake's FunctionFake.cpp must be mocked or it will cause runtime errors. Common ones include:
  - `millis()`, `micros()`
  - `delay()`, `delayMicroseconds()`
  - `pinMode()`, `digitalWrite()`, `digitalRead()`, `analogRead()`, `analogWrite()`
  - `attachInterrupt()`, `detachInterrupt()`
  - `Serial.begin()`, `Serial.print()`, etc.
- **Null HAL Pointer**: Causes assertion failure (error code 3)

**Common Pitfalls:**

- WRONG: `Logger::getInstance().begin(mockHal)` before millis mock -> CRASH
- WRONG: Missing interrupt mocks before DoorController.begin() -> SEGFAULT (code 3221225477)
- WRONG: HAL not set / null pointer -> error code 3

**Reference:** See `test/desktop/test_DoorController/test_DoorController.cpp` for complete working example.

### Integration Testing

- Test all REST endpoints with automated tools
- Verify request/response formats
- Test error conditions and status codes
- Test with actual sensors and actuators
- Verify timing and reliability
- Test under various environmental conditions
- Use the [Hardware Emulator](hardware-emulator.md) for scenario-based testing

### Web UI Testing

- Use Playwright for end-to-end testing
- Test all user workflows
- Verify responsive design on different devices
- Test error handling and user feedback
- Test on actual ESP32 device
- Verify real-time updates work correctly
- Test OTA update process

### Continuous Integration

**Automated Checks:**

- Run unit tests on every commit
- Check code formatting with clang-format
- Verify build succeeds on all targets
- Run static analysis tools

**Quality Gates:**

- All tests must pass before merge
- Code coverage requirements
- No critical security vulnerabilities
- Documentation must be updated

---

## Development Notes

- **Ambiguities:** Any ambiguities or questions should be clarified before coding
- **Settings:** Store in `data/user_settings.json`, NOT hardcoded in firmware
- **Build Process:** `build_web.py` automatically builds and compresses web assets
- **Compatibility:** Ensure C++ code is ESP32-compatible while using modern features where appropriate
- **C++ Standard:** Code uses C++11 (`-std=gnu++11` in platformio.ini)
- **MCP Tools:** Use Context7 for library docs, Playwright for web testing

### Important References

- PlatformIO docs: https://docs.platformio.org
- PlatformIO Library Registry: https://registry.platformio.org
- ESP32 docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- SolidJS docs: https://www.solidjs.com/docs/latest
- SolidJS tutorial: https://www.solidjs.com/tutorial
- Pin reference: `docs/esp32_devkitC_v4_pinlayout.png`
- `.url` files in project root link to important resources
- Library documentation via Context7 MCP tool
