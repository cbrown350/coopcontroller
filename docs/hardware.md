# Hardware Requirements & Pin Configuration

## Microcontroller

- **Board:** ESP32 DevKit C v4
- **Chip:** ESP32-WROOM-32
- **Flash Memory:** 4MB (for LittleFS storage)
- **RAM:** 520KB SRAM
- **Clock:** 240MHz dual-core
- **WiFi:** 802.11 b/g/n 2.4GHz

## Peripheral Modules

### Active Implementation

**Dallas DS18B20 Temperature Sensors** (1-Wire protocol)
- Operating range: -55°C to +125°C (-67°F to +257°F)
- Resolution: 9-12 bit configurable
- Pullup resistor required (4.7kΩ typical)

**Water Flow Meter** (Pulse output)
- Example: YF-S201 or similar
- Configurable pulses-per-gallon ratio
- Interrupt-driven pulse counting

**MosFET** (Pump control)
- N-Channel on ground
- Current capacity suitable for pump

### Planned Implementation

- **DRV8833 Dual H-Bridge** - Door motor control (bidirectional, fault detection)
- **Hall Effect Sensors** - Door position sensing (fully open/closed)
- **LED Lighting** - PWM-dimmable strips/bulbs via MOSFET
- **Buzzer** - Active/passive, 3.3V/5V compatible

## Power Supply

- ESP32: 3.3V regulated (onboard from 5V USB/VIN)
- Peripherals: 5V for relays, sensors
- Motor: Per door motor specs (via DRV8833)
- Light: 12VDC

---

## Pin Configuration

All pins defined in `platformio.ini` as build flags. Inputs/outputs are active high unless marked with "_B" (active low).

### Currently Configured Pins

| Pin | Constant | Function | Type | Notes |
|-----|----------|----------|------|-------|
| 32 | TEMP_METER_PIN | Sensor 1 Input | Input+Pullup | Auto-detects Dallas temp or water meter |
| 33 | TEMP_METER_2_PIN | Sensor 2 Input | Input+Pullup | Auto-detects Dallas temp or water meter |
| 26 | OUT_PUMP_PIN | Pump Control | Output | Relay control for water pump |
| 25 | OUT_LIGHT_PIN | Light Control | PWM Output | LEDC channel 0, 5kHz, 8-bit resolution |
|  | WIFI_LED_B_PIN | WiFi Status LED | Output | Heartbeat when connected, active low |
|  | BUZZER_B_PIN | Alert Buzzer | Output | Active low |
|  | OUT_DOOR_A_OPEN_POS_PIN | Door Open Positive | Output | DRV8833 motor control |
|  | OUT_DOOR_A_OPEN_NEG_PIN | Door Open Negative | Output | DRV8833 motor control |
|  | DOOR_A_FAULT_B_PIN | Door Fault Input | Input | Active LOW from DRV8833 |
|  | DOOR_MANUAL_SWITCH_B_PIN | Manual Door Switch | Input | Momentary switch |
|  | DOOR_A_HALL_SENSOR_OPEN_B_PIN | Door Open Sensor | Input | Hall effect, active low |
|  | DOOR_A_HALL_SENSOR_CLOSED_B_PIN | Door Closed Sensor | Input | Hall effect, active low |

### Pin Safety Rules

- **Avoid boot pins:** GPIO 0, 2, 15
- **Avoid SPI flash pins:** GPIO 6-11
- **Input-only pins:** GPIO 34, 35, 36, 39 (no internal pullups)

---

## PlatformIO Configuration Changes

**MANDATORY APPROVAL REQUIREMENT:** Any modifications to `platformio.ini` (build flags, pins, libraries) must be proposed first with detailed justification and require explicit user approval before implementation.

**Required Process:**
1. Document proposed changes with justification
2. Verify no conflicts with existing pins or ESP32 reserved pins
3. Get explicit user approval before implementing
4. Update this document before modifying `platformio.ini`

---

## Wiring Reference

See `docs/esp32_devkitC_v4_pinlayout.png` for detailed pin layout and capabilities.

For hardware emulator wiring, see [docs/hardware-emulator.md](hardware-emulator.md#wiring-diagram).
