# Hardware Emulator

The Hardware Emulator is a dedicated ESP32 DevKitC v4 module that physically connects to the main Coop Controller to simulate real hardware sensors and actuators for testing and development. This allows comprehensive testing of signal paths, scenario-based testing, and remote debugging without requiring actual chicken coop hardware.

**Implementation Status:** Complete (Sprints 1-11)

---

## Overview

The emulator acts as a hardware-in-the-loop testing platform, providing:

- **Physical Signal Simulation** - Generates real electrical signals (water pulses, hall sensors, switches, faults)
- **Output Monitoring** - Reads main controller outputs (pump, door motor, light PWM, buzzer, WiFi LED)
- **Scenario Testing** - 7 predefined + up to 8 custom user-created test scenarios
- **Signal Recording** - Record and playback at 0.5x / 1x / 2x speed (max 5 min, max 10 recordings)
- **Temperature Emulation** - 2 configurable DS18B20 sensors with sinusoidal drift
- **Remote Control** - Web UI with 9 pages + 40+ REST API endpoints
- **Settings Persistence** - LittleFS-based JSON config with backup/restore

### Purpose & Use Cases

**1. Development Without Real Hardware**
- Develop and test firmware logic without physical sensors/actuators
- Safe testing environment - no risk of hardware damage
- Rapid iteration - change scenarios instantly via web UI
- Independent development - each developer can have their own emulator

**2. Automated Testing of Signal Paths**
- Verify main controller correctly reads emulated sensor inputs
- Confirm main controller outputs activate emulator monitoring
- Test interrupt-driven water pulse detection
- Validate door position sensor logic with hall effect simulation

**3. Scenario-Based Testing**
- Freeze condition, door stuck, motor fault, frozen water line, pump failure
- Each scenario configures multiple signals simultaneously
- One-click activation via web UI or REST API

**4. Remote Debugging & Diagnostics**
- Access emulator web UI from anywhere on network
- Monitor real-time signal states and transitions
- Inject faults manually to test edge cases
- Record signal timing and behavior for analysis

---

## Architecture

### EmulatorStateManager

Central state machine for door states (CLOSED, OPENING, OPEN, CLOSING, STUCK_OPEN, STUCK_CLOSED, MOTOR_FAULT), water pulse generation, hall sensor output, manual switch and fault signal management.

**Key Responsibilities:**
- Door state machine with automatic transitions
- Water pulse generation when pump is active (configurable pulse rate)
- Hall sensor output control based on door position
- Manual switch state management
- Fault signal injection and management
- Scenario state execution and transitions
- Generates interrupt-compatible water meter pulses
- Updates all output pins based on current state
- Handles manual overrides and scenario activation

### EmulatorWebServer

REST API (40+ endpoints) and static file serving for the SolidJS control interface.

**Key Responsibilities:**
- HTTP server with JSON API endpoints
- Status reporting, door/water/scenario control
- Settings management
- Static file serving for SolidJS web UI
- Real-time status queries and manual state control

### EmulatorSettings

Persistent JSON configuration in LittleFS (`data/emulator_settings.json`). WiFi, water pulse rate, door timing, scenario parameters.

**Key Responsibilities:**
- JSON-based settings storage
- WiFi credentials (SSID, password, AP mode settings)
- Water pulse rate configuration (pulses per minute)
- Door operation timing (open/close duration, stuck delays)
- Scenario-specific parameters
- Automatic save on configuration changes
- Load settings on startup, factory reset capability
- Settings export / import as JSON for backup and restore

### CustomScenarioManager

CRUD storage for up to 8 user-created test scenarios in `/custom_scenarios.json`.

**Key Responsibilities:**
- Save, update, delete, and load custom scenarios by name or index
- JSON serialisation / deserialisation of all scenario parameters
- Thread-safe access from web server request handlers

### LogRecorder

Records all 14 signals every 100ms into `SignalSnapshot` structs. Streaming JSON write for memory efficiency.

**Key Responsibilities:**
- Samples all 14 monitored + emulated signals every 100 ms
- Streaming JSON write to LittleFS with compact short keys (`t`, `pa`, `la`, etc.)
- Start / stop / pause recording; max 5 minutes per recording, max 10 recordings stored
- Playback drives emulated outputs via override mode; supports 0.5x / 1x / 2x speed
- Auto-removes oldest recording when storage limit is reached
- Recording index persisted in `/recordings/_index.json`

### TempSensorEmulator

Logical temperature emulator (not 1-Wire slave - unreliable on ESP32 due to microsecond timing constraints). 2 independent sensors, -40 to +60 C, configurable sinusoidal drift, disconnect simulation.

**Key Responsibilities:**
- 2 independent sensor slots, each with enable/disable, configurable temperature
- Disconnect simulation (reports sensor absent to the main controller)
- Sinusoidal drift: configurable amplitude (0.1-10 C) and period (5 s - 5 min)
- `update()` called from main loop to advance drift; JSON serialisation for state persistence

---

## Pin Configuration

### Emulator Input Pins (Reading Main Controller Outputs)

| Pin | Constant | Main Controller Pin | Purpose |
|-----|----------|---------------------|---------|
| 34 | EMU_READ_PUMP_PIN | OUT_PUMP_PIN (26) | Monitor pump control |
| 35 | EMU_READ_LIGHT_PIN | OUT_LIGHT_PIN (25) | Monitor light PWM |
| 36 | EMU_READ_DOOR_POS_PIN | OUT_DOOR_A_OPEN_POS (13) | Monitor door motor + |
| 39 | EMU_READ_DOOR_NEG_PIN | OUT_DOOR_A_OPEN_NEG (12) | Monitor door motor - |
| 32 | EMU_READ_BUZZER_PIN | BUZZER_B_PIN (15) | Monitor buzzer |
| 33 | EMU_READ_LED_PIN | WIFI_LED_B_PIN (23) | Monitor WiFi LED |

*Pins 34, 35, 36, 39 are input-only GPIO - ideal for non-interfering monitoring.*

### Emulator Output Pins (Driving Main Controller Inputs)

| Pin | Constant | Main Controller Pin | Purpose |
|-----|----------|---------------------|---------|
| 26 | EMU_WATER_PULSE1_PIN | TEMP_METER_PIN (32) | Water pulses sensor 1 |
| 25 | EMU_WATER_PULSE2_PIN | TEMP_METER_2_PIN (33) | Water pulses sensor 2 |
| 13 | EMU_HALL_OPEN_PIN | HALL_OPEN_B (36) | Door fully open (active LOW) |
| 12 | EMU_HALL_CLOSE_PIN | HALL_CLOSED_B (39) | Door fully closed (active LOW) |
| 27 | EMU_MANUAL_SW_PIN | DOOR_MANUAL_SW_B (16) | Manual door switch (active LOW) |
| 14 | EMU_DOOR_FAULT_PIN | DOOR_FAULT_B (34) | Motor fault (active LOW) |

### Status Pins

| Pin | Constant | Purpose |
|-----|----------|---------|
| 2 | EMU_STATUS_LED_PIN | Built-in LED status |
| 4 | EMU_WIFI_LED_PIN | WiFi connection indicator |

---

## Wiring Diagram

```
Main Controller ESP32          Emulator ESP32
====================          ================

Outputs -> Emulator Inputs:
--------------------------
GPIO 26 (OUT_PUMP_PIN)     ->  GPIO 34 (EMU_READ_PUMP_PIN)
GPIO 25 (OUT_LIGHT_PIN)    ->  GPIO 35 (EMU_READ_LIGHT_PIN)
GPIO 13 (OUT_DOOR_POS)     ->  GPIO 36 (EMU_READ_DOOR_POS_PIN)
GPIO 12 (OUT_DOOR_NEG)     ->  GPIO 39 (EMU_READ_DOOR_NEG_PIN)
GPIO 15 (BUZZER_B_PIN)     ->  GPIO 32 (EMU_READ_BUZZER_PIN)
GPIO 23 (WIFI_LED_B_PIN)   ->  GPIO 33 (EMU_READ_LED_PIN)

Inputs <- Emulator Outputs:
--------------------------
GPIO 32 (TEMP_METER_PIN)   <-  GPIO 26 (EMU_WATER_PULSE1_PIN)
GPIO 33 (TEMP_METER_2_PIN) <-  GPIO 25 (EMU_WATER_PULSE2_PIN)
GPIO 36 (HALL_OPEN_B)      <-  GPIO 13 (EMU_HALL_OPEN_PIN)
GPIO 39 (HALL_CLOSED_B)    <-  GPIO 12 (EMU_HALL_CLOSE_PIN)
GPIO 16 (DOOR_MANUAL_SW_B) <-  GPIO 27 (EMU_MANUAL_SW_PIN)
GPIO 34 (DOOR_FAULT_B)     <-  GPIO 14 (EMU_DOOR_FAULT_PIN)

Common:
-------
GND  <->  GND (shared ground essential)
```

**Notes:**
- Both ESP32s powered independently via USB or dedicated 5V supply
- All signals 3.3V - no level shifting needed
- Keep wires under 12 inches to minimize noise/capacitance
- Emulator can be disconnected without affecting main controller operation
- Main Controller pins: `platformio.ini` lines 37-48
- Emulator pins: `platformio.ini` lines 156-179

---

## Signal Details

**Pump Actuation Detection:**
- Emulator monitors OUT_PUMP_PIN from main controller
- When HIGH detected, emulator activates water pulse generation
- Pulse rate configurable via settings (default ~440 pulses/min)
- Simulates realistic water flow when pump is active

**Water Meter Pulse Generation:**
- Generates interrupt-compatible pulses on EMU_WATER_PULSE1_PIN and EMU_WATER_PULSE2_PIN
- Pulse width: 50ms ON, 50ms OFF (configurable)
- Only generates pulses when pump is detected as active
- Supports scenario-based failures (frozen water line = no pulses despite pump ON)

**Dallas Temperature Sensor Emulation:**
- Implemented as logical REST-based emulator (see TempSensorEmulator architecture section)
- 2 independently configurable DS18B20 sensor slots (-40 to +60 C)
- Sinusoidal drift simulation: configurable amplitude and period
- Disconnect simulation per sensor
- Controlled via `/emulator/temperature/*` REST endpoints and Temperature web UI page

**Door Motor Control Detection:**
- Monitors both EMU_READ_DOOR_POS_PIN and EMU_READ_DOOR_NEG_PIN
- Detects motor direction based on polarity:
  - POS=HIGH, NEG=LOW -> Door opening
  - POS=LOW, NEG=HIGH -> Door closing
  - Both LOW -> Motor stopped
- Updates internal door state machine based on detected commands
- Simulates realistic door movement timing

**Door Hall Effect Sensors:**
- EMU_HALL_OPEN_PIN driven LOW when door reaches fully open state
- EMU_HALL_CLOSE_PIN driven LOW when door reaches fully closed state
- Both pins HIGH during door movement (between positions)
- Supports stuck scenarios (pin never goes LOW)

**Manual External Door Switch:**
- EMU_MANUAL_SW_PIN normally HIGH (inactive)
- Web UI button triggers momentary LOW pulse (active)
- Simulates physical switch press by user
- Useful for testing manual door control logic

**Door Fault Signal:**
- EMU_DOOR_FAULT_PIN normally HIGH (no fault)
- Pulled LOW when motor fault scenario is active
- Simulates DRV8833 driver fault detection
- Tests main controller fault handling logic

**Buzzer Detection:**
- Monitors EMU_READ_BUZZER_PIN for active LOW signal
- Web UI displays buzzer state in real-time
- Verifies main controller activates buzzer for alerts

**WiFi LED Detection:**
- Monitors EMU_READ_LED_PIN for heartbeat patterns
- Web UI displays LED state for connection monitoring
- Helps verify main controller WiFi status indication

---

## Web UI Pages

### 1. Status Page

Real-time display of emulator and main controller states.

**Displays:**
- Door current state (CLOSED, OPENING, OPEN, CLOSING, STUCK, FAULT)
- Water pulse generation status (active/inactive, current rate)
- Main controller output monitoring (pump, light PWM level, door motor direction)
- Hall sensor current states (open/closed detected)
- Fault injection status (door fault, manual switch)
- Buzzer and WiFi LED detection status
- System information (IP address, uptime, firmware version)

**Features:**
- Auto-refresh every 2 seconds for real-time updates
- Visual indicators for active signals
- Color-coded state displays (green=normal, yellow=transitioning, red=fault)

### 2. Door Control Page

Manual control of door state for testing.

**Controls:**
- Manual state selection: Closed, Opening, Open, Closing, Stuck Open, Stuck Closed, Motor Fault
- Timing configuration: Open duration, close duration, stuck delay

**Use Cases:**
- Test door controller logic independently
- Verify hall sensor reading accuracy
- Test fault detection and recovery
- Measure door operation timing requirements

### 3. Water Control Page

Configuration of water pulse generation for flow simulation.

**Controls:**
- Pulse rate adjustment (0-1000 pulses per minute)
- Enable/disable toggle for pulse generation
- Pulse pattern selection: Continuous, Intermittent, No Flow (frozen line scenario)
- Pulse width configuration (default 50ms ON / 50ms OFF)

**Features:**
- Real-time pulse count display
- Pulse rate calculation (GPM equivalent)
- Visual pulse indicator (LED flash on pulse)
- Separate control for sensor 1 and sensor 2 if needed

**Use Cases:**
- Calibrate water meter detection sensitivity
- Test flow error detection logic
- Simulate frozen water lines
- Verify pulse counting accuracy

### 4. Manual Controls Page

Direct manipulation of individual output pins for advanced testing.

**Controls:**
- Pin state overrides: Hall Open/Close Sensor, Door Fault Signal, Manual Switch Signal, Water Pulse 1/2 (force HIGH/LOW)
- Input monitoring: Pump signal, Light PWM (0-255), Door Motor Positive/Negative, Buzzer, WiFi LED

**Features:**
- Real-time pin state visualization
- Manual override of automated behavior
- Signal injection for edge case testing
- Diagnostic mode for troubleshooting wiring

**Use Cases:**
- Test individual signal paths
- Verify wiring connections
- Debug communication issues
- Inject specific fault conditions
- Bypass automatic scenarios for custom tests

### 5. Scenarios Page

Predefined test scenarios that simulate real-world conditions. See [Test Scenarios](#test-scenarios) section for full details on all 7 predefined scenarios.

**Features:**
- One-click scenario activation
- Automatic state configuration
- Scenario duration display
- Reset to normal operation button
- Scenario status indicators

**Custom Scenarios:**
- "Create Custom Scenario" button opens the ScenarioEditor form
- Configure: name (31-char limit), description (127-char limit), door position/state, water flow rate, fault injection, and advanced override toggles
- Up to 8 custom scenarios stored persistently in `/custom_scenarios.json` on LittleFS
- Custom scenarios appear alongside predefined scenarios with Edit / Delete / Apply controls
- Apply a custom scenario directly by name via `POST /emulator/scenarios/custom/apply`

### 6. Recordings Page

Record all signal states over time and replay them for regression testing and debugging.

**Recording Controls:**
- Label - Assign a human-readable name before starting
- Record / Pause / Stop & Save - Full lifecycle control with live sample-count and elapsed-time display
- Sampling interval: 100 ms; maximum recording duration: 5 minutes
- Up to 10 recordings stored on LittleFS; oldest is auto-removed when limit is reached

**Playback Controls:**
- Select any saved recording and click Play to replay all signal states
- Progress bar shows current position vs total duration
- Speed selector: 0.5x, 1x, 2x playback
- Pause / Resume / Stop controls
- Playback drives all emulated outputs via the override system

**Recording Management:**
- Download any recording as a JSON file for offline analysis
- Delete individual recordings or clear all at once
- 1-second polling keeps the UI in sync with recording / playback state

**Use Cases:**
- Capture a known-good signal sequence for later comparison
- Replay a captured fault condition for debugging without re-wiring
- Automate regression testing by replaying saved sequences

### 7. Temperature Page

Configure and monitor emulated DS18B20 temperature sensors.

**Per-Sensor Controls (Sensor 1 and Sensor 2):**
- Enable / Disable toggle - only enabled sensors appear in status responses
- Temperature slider (-40 to 60 C, 0.5 C steps) plus numeric input; large readout shows both C and F
- Disconnect Simulation toggle with red badge; reports the sensor as absent to the main controller
- Drift Simulation (collapsible panel):
  - Enable / disable drift
  - Amplitude slider: 0.1-10 C peak swing
  - Period slider: 5 seconds to 5 minutes (sinusoidal cycle)

**Design Note:** Drift uses `sinf()` over the configured period for smooth, continuous oscillation around the set temperature. This accurately models real-world sensor behaviour in changing environments.

**Use Cases:**
- Test temperature-based pump cycling thresholds
- Simulate gradual temperature changes (freeze / thaw) with drift
- Test sensor-failure handling by disconnecting a sensor
- Verify the main controller reacts correctly to multi-sensor disagreements

### 8. Settings Page

Configuration persistence and system settings.

**Settings Categories:**

- **WiFi:** SSID, password, AP mode, AP SSID, retry attempts and delays
- **Water Emulation:** Default pulse rate, pulse width, separate enable for sensor 1/2, noise simulation, flow calculation interval
- **Door Emulation:** Open/close duration, stuck delay, fault injection enable/disable, hall sensor debounce
- **System:** Hostname (mDNS), debug logging, syslog server address/port, NTP server, status LED brightness

**Features:**
- Save settings button with confirmation
- Reset to defaults option
- Import/export settings as JSON
- Settings validation before save
- Restart required indicator

### 9. About Page

System information display.

---

## Test Scenarios

The emulator provides 7 predefined test scenarios for comprehensive system testing.

### 1. Normal Operation

**Purpose:** Baseline testing with all systems functional.

**Configuration:**
- Door: Opens and closes normally with proper hall sensor feedback
- Water: Pulses generated when pump is active
- Pump: Responds to temperature control logic
- Faults: None injected

**Expected Main Controller Behavior:**
- Temperature monitoring operational
- Pump cycles based on temperature thresholds
- Water flow detected during pump cycles
- Door operates normally with position confirmation
- All sensors report correctly

**Validation:**
- Verify pump ON triggers water pulses
- Confirm door reaches open/closed positions
- Check hall sensors indicate correct positions
- Monitor pump cycle timing matches settings

### 2. Freeze Condition

**Purpose:** Simulate sub-zero temperatures requiring pump cycling.

**Configuration:**
- Simulated temperature: Below threshold (implied by emulator state)
- Water: Continuous pulse generation while pump active
- Pump: Should cycle ON/OFF per configuration
- Door: Normal operation

**Expected Main Controller Behavior:**
- Pump activates in cycling mode (5 min ON, 10 min OFF by default)
- Water flow detected during pump ON periods
- Flow error NOT triggered (pulses present)
- Temperature readings trigger pump activation

**Validation:**
- Confirm pump cycling behavior
- Verify water pulses generated during pump ON
- Check pump OFF periods have no pulses (emulator stops generating)
- Monitor total pump runtime accumulation

### 3. Door Stuck Open

**Purpose:** Test timeout and fault detection when door won't close.

**Configuration:**
- Door: Receives close command but hall open sensor stays active (LOW)
- Hall Close Sensor: Never activates
- Motor: Responds to command but door doesn't move
- Timeout: Should trigger after configured close duration + buffer

**Expected Main Controller Behavior:**
- Initiates door close sequence
- Monitors for hall close sensor activation
- Timeout occurs after expected close duration
- Fault logged and user notified
- Automatic retry or manual intervention required

**Validation:**
- Verify timeout detection accuracy
- Check fault logging and notification
- Confirm door state reported as "STUCK" or error
- Test recovery after manual intervention

### 4. Door Stuck Closed

**Purpose:** Test fault handling when door won't open.

**Configuration:**
- Door: Receives open command but hall close sensor stays active (LOW)
- Hall Open Sensor: Never activates
- Motor: Responds to command but door doesn't move
- Timeout: Should trigger after configured open duration + buffer

**Expected Main Controller Behavior:**
- Initiates door open sequence
- Monitors for hall open sensor activation
- Timeout occurs after expected open duration
- Fault logged and user notified
- System prevents chickens from being locked out

**Validation:**
- Verify timeout detection timing
- Check fault reporting mechanisms
- Confirm safety features engage (door stays closed if can't open)
- Test manual override functionality

### 5. Motor Fault

**Purpose:** Test DRV8833 driver fault detection.

**Configuration:**
- Door: Receives open/close command
- Motor Fault Signal: Active (LOW) from emulator
- Hall Sensors: No position changes
- Motor: Simulates driver fault condition

**Expected Main Controller Behavior:**
- Detects fault signal immediately
- Stops motor command sequence
- Logs hardware fault
- Notifies user of motor driver issue
- Prevents repeated attempts that could damage hardware

**Validation:**
- Confirm immediate fault detection (not timeout-based)
- Verify motor commands cease when fault active
- Check error message clarity
- Test recovery after fault cleared

### 6. Frozen Water Line

**Purpose:** Test flow error detection when pump runs but no flow.

**Configuration:**
- Pump: Active (HIGH) from main controller
- Water Pulses: None generated (emulator stops pulses)
- Temperature: Below threshold (pump should be ON)
- Flow Error Timeout: Should trigger after configured duration (default 120s)

**Expected Main Controller Behavior:**
- Activates pump due to low temperature
- Monitors water flow via pulse counting
- No pulses detected despite pump running
- Flow error triggered after timeout period
- Pump shut down to prevent damage
- Automatic retry after configured delay

**Validation:**
- Verify flow error timeout accuracy
- Confirm pump shutdown on error
- Check retry logic and timing
- Monitor error logging and notifications
- Test manual flow error clear function

### 7. Pump Failure

**Purpose:** Test detection when pump doesn't activate despite command.

**Configuration:**
- Temperature: Below threshold (pump command should be sent)
- Pump Output: Never goes HIGH (emulator detects no signal)
- Water Pulses: None (no pump = no flow)
- Fault: Simulates relay failure or power loss

**Expected Main Controller Behavior:**
- Temperature controller sends pump ON command
- Monitors for flow indication
- Flow error timeout triggers (no pulses)
- System identifies pump not responding
- Logs pump failure
- Alerts user to hardware issue

**Validation:**
- Verify pump command sent by main controller
- Confirm flow error detection
- Check error reporting indicates pump (not water system) fault
- Test that system doesn't repeatedly retry broken pump
- Validate notification systems engage

### Scenario Testing Workflow

1. **Preparation:** Connect main controller and emulator, upload latest firmware to both, access both web UIs
2. **Execution:** Navigate to emulator Scenarios page, click scenario to activate, monitor main controller Status page, observe emulator Status page for signal states, check main controller Logs for errors/warnings
3. **Validation:** Compare actual behavior to expected behavior, verify all required signals generated correctly, check timing meets specifications, confirm error handling and notifications, document any deviations
4. **Reset:** Click "Normal Operation" scenario, verify system returns to baseline, ready for next test

---

## API Documentation

The emulator exposes 40+ REST endpoints. All responses are JSON unless noted.

### Status Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/emulator/status` | Full status: monitored signals, emulated outputs, config, active scenario |
| GET | `/emulator/monitored` | Monitored signals only (pump, light, motor, buzzer, LED) |
| GET | `/emulator/emulated` | Emulated outputs only (water, door, hall, switch, fault) |
| GET | `/emulator/system` | System info: uptime, heap, chip model, WiFi, firmware version |

### Door Control

| Method | Path | Body / Notes |
|--------|------|--------------|
| POST | `/emulator/door/position` | `{ "position": 0-100 }` - set door position directly |
| POST | `/emulator/door/open` | Move door to fully open |
| POST | `/emulator/door/close` | Move door to fully closed |
| POST | `/emulator/door/fault/inject` | `{ "active": true/false }` - inject / clear fault |
| POST | `/emulator/door/fault/clear` | Clear door fault |
| POST | `/emulator/door/config` | `{ "travel_time_ms": N, "auto_simulate": bool }` |

### Water Control

| Method | Path | Body / Notes |
|--------|------|--------------|
| POST | `/emulator/water/config` | `{ "flow_rate_gpm": N, "auto_generate_pulses": bool, "pulses_per_gallon": N }` |
| POST | `/emulator/water/pulse` | Generate a single water pulse on both channels |
| POST | `/emulator/water/reset` | Reset pulse counters to zero |
| POST | `/emulator/water/frozen` | `{ "frozen": true/false }` - simulate frozen water line |

### Manual Switch

| Method | Path | Body / Notes |
|--------|------|--------------|
| POST | `/emulator/manual_switch/press` | Assert switch active (hold) |
| POST | `/emulator/manual_switch/release` | Release switch |
| POST | `/emulator/manual_switch/pulse` | `{ "duration_ms": N }` - momentary press (default 200 ms) |
| POST | `/emulator/manual_switch/long_press` | `{ "duration_ms": N }` - long press (default 2000 ms) |
| POST | `/emulator/manual_switch/config` | `{ "short_press_ms": N, "long_press_ms": N }` |

### Override Mode

| Method | Path | Body / Notes |
|--------|------|--------------|
| POST | `/emulator/override/enable` | Enable global manual override |
| POST | `/emulator/override/disable` | Disable global manual override |
| POST | `/emulator/override/set` | `{ "hall_open": bool, "hall_close": bool, ... }` - set individual overrides |
| POST | `/emulator/override/clear_all` | Reset all overrides |

### Scenarios

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/emulator/scenarios` | List all 7 predefined scenarios with full details |
| GET | `/emulator/scenario/active` | Currently active scenario |
| POST | `/emulator/scenario/apply` | `{ "id": 0-6 }` - apply predefined scenario by ID |
| POST | `/emulator/scenario/custom` | Apply an ad-hoc custom scenario from a JSON body |

### Custom Scenarios (Persistent)

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/emulator/scenarios/custom` | List all saved custom scenarios |
| POST | `/emulator/scenarios/custom/save` | Save or update a custom scenario (name + full config) |
| DELETE | `/emulator/scenarios/custom` | `?name=ScenarioName` - delete by name |
| POST | `/emulator/scenarios/custom/apply` | `{ "name": "..." }` - apply a saved custom scenario |

### Recordings

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/emulator/recordings` | List all stored recordings (metadata) |
| GET | `/emulator/recordings/status` | Current recording + playback state |
| POST | `/emulator/recordings/start` | `{ "label": "..." }` - start a new recording |
| POST | `/emulator/recordings/stop` | Stop and save the current recording |
| POST | `/emulator/recordings/pause` | Toggle recording pause |
| POST | `/emulator/recordings/playback/start` | `{ "id": "...", "speed_percent": 100 }` |
| POST | `/emulator/recordings/playback/stop` | Stop playback |
| POST | `/emulator/recordings/playback/pause` | Toggle playback pause |
| POST | `/emulator/recordings/playback/speed` | `{ "speed_percent": 50/100/200 }` |
| DELETE | `/emulator/recordings/:id` | Delete one recording |
| DELETE | `/emulator/recordings/all` | Delete all recordings |
| GET | `/emulator/recordings/download/:id` | Download recording JSON file |

### Temperature Sensors

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/emulator/temperature` | State of both sensors |
| POST | `/emulator/temperature/set` | `{ "sensor1": { ... }, "sensor2": { ... } }` - set temp / options |
| POST | `/emulator/temperature/enable` | `{ "sensor": 1, "enabled": true }` |
| POST | `/emulator/temperature/disconnect` | `{ "sensor": 1, "disconnected": true }` |
| POST | `/emulator/temperature/drift` | `{ "sensor": 1, "enabled": true, "amplitude_c": 2.0, "period_ms": 30000 }` |

### Settings & System

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/get_settings` | Current settings (WiFi, emulation defaults, log level) |
| POST | `/update_settings` | Update settings (partial JSON; password only if changing) |
| GET | `/emulator/settings/export` | Download full settings as JSON with export metadata |
| POST | `/emulator/settings/import` | Restore settings from a previously exported JSON file |

### Fault Injection

| Method | Path | Body / Notes |
|--------|------|--------------|
| POST | `/emulator/fault/door_stuck` | `{ "stuck": true/false }` - simulate door stuck |
| POST | `/emulator/fault/clear_all` | Clear all injected faults |

### System

| Method | Path | Notes |
|--------|------|-------|
| POST | `/reboot` | Reboot the emulator ESP32 |
| POST | `/factory_reset` | Wipe all settings and recordings, reboot |

---

## Building & Deploying

```bash
pio run -e esp32-emulate-hardware                  # Build firmware
pio run -e esp32-emulate-hardware --target upload   # Upload firmware
cd emulate_hardware/web && npm run build            # Build web UI
pio run -e esp32-emulate-hardware --target uploadfs  # Upload filesystem
```

**Environment:** `[env:esp32-emulate-hardware]` in `platformio.ini` (line 137+). Source filter: `emulate_hardware/src/`. Custom web dir: `emulate_hardware/web`.

**Access:** Main controller at `http://coopcontroller.local`, emulator at `http://hwemulator.local`.

---

## Build Statistics

- **RAM:** 47,044 bytes (14.4%)
- **Flash:** 1,144,865 bytes (87.3%)
- Build time: 15-25s (clean), 3-8s (incremental)
- Web UI build: 8-12s (36 modules transformed, gzip-optimised assets)

**Comparison to Main Controller:**
- Main Controller RAM: 56,436 bytes (17.2%)
- Main Controller Flash: 1,081,881 bytes (82.5%)
- Emulator uses less RAM due to simpler logic (no sensor libraries)
- Emulator uses more flash due to recording and temperature emulation features

---

## File Structure

```
emulate_hardware/
├── src/
│   ├── main.cpp                    # Entry point
│   ├── config.h                    # Pin definitions
│   ├── EmulatorStateManager.h/cpp  # State machine
│   ├── EmulatorWebServer.h/cpp     # REST API
│   ├── EmulatorSettings.h/cpp      # Settings persistence
│   ├── CustomScenarioManager.h/cpp # Custom scenario CRUD
│   ├── LogRecorder.h/cpp           # Signal recording/playback
│   └── TempSensorEmulator.h/cpp    # Temperature emulation
└── web/
    ├── src/
    │   ├── index.tsx, App.tsx, types.ts
    │   ├── Status.tsx, DoorControl.tsx, WaterControl.tsx
    │   ├── ManualControls.tsx, Scenarios.tsx, ScenarioEditor.tsx
    │   ├── Recordings.tsx, Temperature.tsx, Settings.tsx
    ├── package.json
    └── vite.config.ts
```

---

## Usage Tips

**Development Best Practices:**

1. Start with Normal Operation scenario before testing faults
2. Monitor both web UIs side-by-side for real-time correlation
3. Check serial logs for timing/state info not visible in web UI
4. Use Manual Controls sparingly - automated scenarios are more reliable for regression testing
5. Document custom scenarios with pin states and timing in comments

**Troubleshooting:**

- **No Signal Detection:** Check wiring connections and ground continuity
- **Erratic Behavior:** Verify both devices sharing common ground
- **Web UI Not Loading:** Check WiFi, verify IP, restart emulator
- **Main Controller Not Responding:** Verify emulator outputs driving correct voltage levels (3.3V)
- **Timing Issues:** Adjust scenario timing parameters to match controller expectations

**Advanced Features:**

- Programmatic control via REST API from automation scripts for CI/CD integration
- Signal recording for post-analysis and logging all pin states
- Batch testing - run multiple scenarios sequentially via API for automated regression
- Remote access via VPN or port forwarding for remote debugging
