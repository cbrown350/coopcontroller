# REST API Reference

All endpoints return JSON unless otherwise specified. Authentication is optional (disabled by default).

When authentication is enabled, state-modifying endpoints require HTTP Basic Auth. Read-only endpoints remain public.

---

## Settings Management

### GET `/get_settings`

Get current system settings. **Public** (password field excluded for security).

**Response:**

```json
{
  "ssid": "MyNetwork",
  "ap_mode": false,
  "enabled": true,
  "has_connected": true,
  "temp_threshold_on_f": 34.0,
  "temp_threshold_off_f": 36.0,
  "pump_on_time_seconds": 300,
  "pump_off_time_seconds": 600,
  "pump_auto_mode": true,
  "light_auto_mode": false,
  "light_on_hour": 6,
  "light_off_hour": 21,
  "debug_enabled": false,
  "water_flow_error_timeout_seconds": 120,
  "wifi_max_retries": 5,
  "wifi_retry_delay_seconds": 30,
  "wifi_ap_duration_minutes": 10
}
```

### POST `/update_settings` (Protected)

Update system settings. Only provided fields are updated.

**Request Body:**

```json
{
  "ssid": "NewNetwork",
  "passwd": "NewPassword",
  "temp_threshold_on_f": 32.0,
  "pump_auto_mode": true,
  "debug_enabled": true
}
```

**Response:** `200 OK` with "ok" text.

**Note:** WiFi settings (ssid, passwd, ap_mode) trigger system restart after save.

---

## Status & Monitoring (Public)

### GET `/sensor_status`

Get real-time sensor and pump status.

**Response:**

```json
{
  "sensor1": {
    "type": "DALLAS_TEMP",
    "connected": true,
    "temperature_f": 35.2,
    "flow_rate": 0.0,
    "pulse_count": 0,
    "status": "Connected"
  },
  "sensor2": {
    "type": "WATER_METER",
    "connected": true,
    "temperature_f": 0.0,
    "flow_rate": 2.5,
    "pulse_count": 1234,
    "status": "Connected - Active Flow"
  },
  "pump": {
    "state": "AUTO_ON",
    "is_active": true,
    "temperature_f": 35.2,
    "temperature_below_threshold": true,
    "flow_error": false,
    "current_cycle_time": 120,
    "time_until_next_switch": 180,
    "total_on_time": 3600,
    "total_off_time": 7200,
    "total_cycles": 10,
    "time_until_retry": 0
  },
  "system": {
    "temp_threshold_on_f": 34.0,
    "temp_threshold_off_f": 36.0,
    "pump_on_time_seconds": 300,
    "pump_off_time_seconds": 600,
    "pump_auto_mode": true,
    "light_auto_mode": false,
    "light_on_hour": 6,
    "light_off_hour": 21,
    "debug_enabled": false,
    "water_flow_error_timeout_seconds": 120
  }
}
```

### GET `/system_status`

System health metrics. **Public.**

### GET `/logs`

Get system log entries. **Public.**

**Response:**

```json
{
  "logs": [
    {
      "uuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "timestamp": 1698765432,
      "message": "System initialization complete"
    },
    {
      "uuid": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      "timestamp": 1698765492,
      "message": "Pump turned on - temperature below threshold"
    }
  ]
}
```

### GET `/version`

Firmware version and build information. **Public.**

**Response:**

```json
{
  "firmware_version": "1.0.0",
  "chip_family": "ESP32-WROOM",
  "build_date": "Nov  1 2025",
  "build_time": "12:34:56"
}
```

### GET `/sun/times`

Current sunrise/sunset times based on configured location. **Public.**

### GET `/settings/backup`

Download settings backup as JSON. **Public.**

---

## Pump Control (Protected)

### GET `/pump/on`

Force pump ON (override auto mode).

**Response:** `200 OK` with "Pump turned on" text

### GET `/pump/off`

Force pump OFF (override auto mode).

**Response:** `200 OK` with "Pump turned off" text

### GET `/pump/auto`

Enable automatic pump control based on temperature.

**Response:** `200 OK` with "Pump set to auto mode" text

### GET `/pump/force_cycle`

Force an immediate pump cycle (useful for testing).

**Response:** `200 OK` with "Pump cycle forced" text

### GET `/pump/clear_error`

Clear pump flow error state and allow retry.

**Response:** `200 OK` with "Pump flow error cleared" text

### GET `/pump/clear_off_flow_detected`

Clear pump-off flow warning.

**Response:** `200 OK`

### GET `/pump/reset_stats`

Reset pump statistics (on/off time, cycle counts).

**Response:** `200 OK` with "Pump statistics reset" text

---

## Water Meter Control (Protected)

### GET `/water/reset/1`

Reset pulse count for water meter on sensor 1.

**Response:** `200 OK` with "Water meter 1 reset" text

### GET `/water/reset/2`

Reset pulse count for water meter on sensor 2.

**Response:** `200 OK` with "Water meter 2 reset" text

---

## Light Control (Protected)

| Endpoint | Description |
|----------|-------------|
| GET `/light/on` | Force light ON |
| GET `/light/off` | Force light OFF |
| GET `/light/fade_in` | Start fade-in |
| GET `/light/fade_out` | Start fade-out |
| GET `/light/set_brightness` | Set brightness level |
| GET `/light/reset_stats` | Reset light statistics |

---

## Door Control (Protected)

| Endpoint | Description |
|----------|-------------|
| GET `/door/open` | Open door |
| GET `/door/close` | Close door |
| GET `/door/stop` | Stop door movement |
| GET `/door/clear_fault` | Clear door fault |
| GET `/door/reset_stats` | Reset door statistics |

---

## Buzzer Control (Protected)

| Endpoint | Description |
|----------|-------------|
| GET `/buzzer/test` | Test buzzer |
| GET `/buzzer/silence` | Silence active buzzer |
| GET `/buzzer/clear` | Clear buzzer alert |

---

## System Control (Protected)

| Endpoint | Description |
|----------|-------------|
| POST `/factory_reset` | Factory reset device |
| POST `/settings/restore` | Restore settings from backup |
| POST `/reboot` | Reboot device |

---

## OTA Updates

### Web Interface: `/update`

Web-based OTA update interface via ElegantOTA.

- Supports firmware (.bin) uploads
- Supports filesystem (.bin) uploads
- Optional authentication (if OTA_PASSWD set)
- Progress indicators
- Automatic restart after successful update

### Network OTA: ArduinoOTA

- Available at `coopcontroller.local:3232`
- Requires Arduino IDE or PlatformIO OTA upload
- Optional password authentication (OTA_PASSWD)
- Binary upload for firmware updates

**OTA Configuration (platformio.ini):**

```ini
upload_protocol = espota
upload_port = coopcontroller.local
upload_flags = --auth=<password>
```

---

## Static Files

| Path | Description |
|------|-------------|
| GET `/` | SolidJS SPA from LittleFS |
| GET `/assets/*` | Static assets (gzip compressed) |

---

## Historical Data (Public/Protected)

### GET `/data/history`

Get historical sensor and controller data. **Public** (read-only).

Returns an array of data points collected at configured sample interval (default: 60 seconds) plus event-triggered captures when door, pump, or flow states change between samples.

**Response:**

```json
[
  {
    "timestamp": 1673892000,
    "temperature_f": 35.2,
    "pump_active": true,
    "flow_rate": 2.5,
    "light_brightness": 80,
    "door_state": "OPEN",
    "door_position": "OPEN",
    "pump_trigger": "auto",
    "door_trigger": "auto",
    "light_trigger": "manual",
    "is_event": false
  },
  {
    "timestamp": 1673892030,
    "temperature_f": 35.3,
    "pump_active": false,
    "flow_rate": 0.0,
    "light_brightness": 80,
    "door_state": "CLOSING",
    "door_position": "PARTIAL",
    "pump_trigger": "auto",
    "door_trigger": "auto",
    "light_trigger": "manual",
    "is_event": true
  }
]
```

**Fields:**
- `timestamp` - Unix timestamp (seconds since epoch, or boot time if NTP not synced)
- `temperature_f` - Temperature in Fahrenheit (NaN if no sensor)
- `pump_active` - Pump state (true = ON, false = OFF)
- `flow_rate` - Water flow rate in GPM
- `light_brightness` - Light brightness percentage (0-100)
- `door_state` - Door operational state string (OPEN, CLOSED, OPENING, CLOSING, IDLE, FAULT)
- `door_position` - Door physical position string (OPEN, CLOSED, PARTIAL, UNKNOWN)
- `pump_trigger` - What triggered last pump state change (unknown, manual, web, api, auto, sensor, etc.)
- `door_trigger` - What triggered last door state change (unknown, manual, web, api, auto, sensor, etc.)
- `light_trigger` - What triggered last light state change (unknown, manual, web, api, auto, sensor, etc.)
- `is_event` - Whether this data point was captured by event detection (true) or periodic sampling (false)

**Notes:**
- Buffer size configurable via settings (default: 1440 samples = 24 hours at 60s interval)
- Circular buffer automatically overwrites oldest data when full
- Data stored in RAM only (cleared on reboot)
- Events are captured immediately when door state/position, pump state, or flow activity changes
- Events share the same buffer as periodic samples

### GET `/data/export_csv`

Download historical data as CSV file. **Public** (read-only).

Returns CSV file with headers for download.

**Response:** CSV file with `Content-Disposition: attachment` header

```csv
timestamp,temperature_f,pump_active,flow_rate,light_brightness,door_state,door_position,pump_trigger,door_trigger,light_trigger,is_event
1673892000,35.2,true,2.5,80,OPEN,OPEN,auto,auto,manual,false
1673892030,35.3,false,0.0,80,CLOSING,PARTIAL,auto,auto,manual,true
```

**File format:**
- Comma-separated values
- Headers included
- Boolean values as `true`/`false`
- Floating point with appropriate precision
- Filename: `coop_history.csv`

### POST `/data/clear` (Protected)

Clear all historical data from buffer.

**Response:**

```json
{
  "success": true
}
```

**Note:** This operation cannot be undone. Data is permanently cleared from RAM.

---

## Authentication

When `api_auth_enabled` is true in settings:

- **Protected endpoints (31):** All state-modifying endpoints listed above
- **Public endpoints (9):** `/sensor_status`, `/system_status`, `/logs`, `/version`, `/sun/times`, `/get_settings`, `/settings/backup`, `/data/history`, `/data/export_csv`
- **Method:** HTTP Basic Authentication (Base64-encoded credentials)
- **401 Response:** Includes `WWW-Authenticate: Basic realm="Coop Controller"` header

For emulator API endpoints, see [docs/hardware-emulator.md](hardware-emulator.md#api-documentation).
