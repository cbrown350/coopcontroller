# Coop Controller

![Coop Controller logo](web/logo.webp)

ESP32-based automation for chicken coop management: temperature monitoring, freeze prevention, pump control, lighting automation, WiFi management, and a SolidJS web UI for configuration and status.

## Highlights

- Automated pump control with freeze prevention, hysteresis, and flow monitoring
- Dual-purpose sensor inputs (Dallas temp or water meter) with auto-detection
- Smooth PWM lighting control with manual and scheduled modes
- Async web server with OTA updates, logs, and real-time status dashboard
- All settings stored in LittleFS and configurable via web UI

## Firmware Installation (ESP32)

1. Flash firmware and filesystem (web tool or PlatformIO Upload Filesystem Image).
2. Connect to the temporary WiFi AP `CoopController` (password `coopycontroller`).
3. Open [http://192.168.4.1](http://192.168.4.1) to load the UI, enter your WiFi SSID/password, and save settings.
4. After restart, access the device at [http://coopcontroller.local](http://coopcontroller.local) on your network.

## Web UI Development (SolidJS)

- `cd web && npm i`
- `npm run dev` to run with the mock API server.
- `npm run build` then upload the filesystem image to copy assets into `data/`.

## Project Resources

- Full project guide and hardware/feature details: Agents and architecture notes in [Agents.md](Agents.md)
- PlatformIO configuration and pin definitions: [platformio.ini](platformio.ini)
