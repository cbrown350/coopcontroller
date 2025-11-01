# AGENTS.md

## Project Overview

- This is the Coop Controller project, designed to manage and monitor a chicken coop environment. It uses an ESP32 microcontroller with Platform.io to handle temperature sensors, water meters, and control outputs like pumps and lights. The project includes a web-based user interface built with SolidJS for easy configuration and monitoring.
- See the ESP32 pin functions in the [`platformio.ini`](platformio.ini:45) and [`README.md`](README.md:1) files.
- The system reads two configurable sensor inputs (TEMP_METER_PIN and TEMP_METER_2_PIN) that can automatically detect and operate as either Dallas temperature sensors or water meter pulse inputs.
- When the temperature goes below a threshold set in settings (default 34°F), the pump activates in cycling mode to prevent water from freezing, with intervals set in settings (default 5 minutes on/10 minutes off).
- The system includes comprehensive water flow error detection - if no water flow is detected when the pump is running for a configurable timeout period, it logs an error, stops the pump, and retries after a delay.
- The WiFi connection management includes automatic fallback to AP mode if connection fails, with configurable retry parameters and AP duration.
- The web interface provides real-time status updates, manual pump controls, and comprehensive settings management through a modern SolidJS interface with Tailwind CSS styling.
- The project maintains a modular structure with separate components for temperature sensing, pump control, settings management, web server, and logging.

## TODO list, next phase

- Wifi heartbeat LED (WIFI_LED_B_PIN) when connected, fast blink when not connected
- Door Control, both automatic based on weather, AI prompt set from web UI, sunrise/sunset offsets, and manual control from web UI
- OUT_DOOR_A_OPEN_POS_PIN/OUT_DOOR_A_OPEN_NEG_PIN drive the door open for door A and with polarity reersed to close; DOOR_A_HALL_SENSOR_OPEN_PIN and DOOR_A_HALL_SENSOR_CLOSED_PIN are inputs from hall effect sensors to detect fully open and fully closed positions; OUT_DOOR_A_FAULT_B_PIN is an output that goes low on fault (timeout trying to open/close)
- Light Control, both automatic based on sunrise/sunset with offsets for on and off and manual control from web UI
- Light output on OUT_LIGHT_PIN should be active high to turn on light, PWM dimming with manual control with timer or auto mode should simulate the light gradually coming on/off over a sine curve over the settings interval at brightnesses levels set for on/off from web UI
- Setup for email and Telegram servers, passwords, ports, etc in settings on web UI
- Setup for OpenWeather API key and location in settings on web UI
- Setup for OpenAI compatible API base URL, key, etc. settings on web UI
- GPS coordinates set from zip code entry or lat/long in settings requested from browser on web UI
- Notify email/Telegram when pump/water flow fault
- Notify email/Telegram when temp sensor not found
- Notify email/Telegram when any API (OpenWeather, OpenAI) fails if the values are set in settings
- Notify email/Telegram daily status and plan for automated functions of door/light and pump forecast based on weather forecast, time of day to send set from web UI
- BUZZER_PIN to sound on fault conditions with pump or temp sensor with silencing button on web UI
- Add reboot button on web UI
- Add ESP32 watchdog for main loop
- Change logging to allow levels, set in settings and web UI, methods in Logging.h (info, warning, error, debug, verbose), replace code with debug if/then to logX funcs
- Change connection status to only show connected if there is a pulse from a water meter, otherwise not connected yet
- Show heap/cpu etc. in web UI status, logs periodically if verbose?
- Add syslog remote server/port settings to web UI, replacing #defines from compile time
- Restart or retry if no temp sensor found after email/Telegram notification?
- Change FLOW_CALCULATION_INTERVAL = 60000 to be configurable in settings in the web UI, replacing const
- Display message when Dallas sensor not found on either sensor and show unknown temperature (---°F) in web UI instead of just 0°F
- Add unit tests for key components using Google Test framework
- Add more detailed inline documentation for complex functions and classes
- Display uptime since last reboot in web UI status
- Make sure no web-related calls are made when not connected to WiFi (e.g., OpenWeather, OpenAI)
- Optimize web UI for mobile devices
- Make water meter pulse calibration factor configurable from web UI settings instead of hardcoded as pulseToGallons in constructor of TempSensor.cpp
- Rename TempSensor to SensorManager since it handles both temperature and water meter sensors
- Change enums to enum class for better type safety and make sure web components are updated accordingly to be compatible with the web server JSON handling using string states instead of numeric values

## Implemented Features

### Core Components

- **TempSensor**: Handles dual-purpose sensor inputs that automatically detect Dallas temperature sensors or water meter pulse inputs
- **PumpController**: Manages pump operation with automatic temperature-based cycling, manual control, and flow error detection
- **SettingsManager**: Persistent configuration storage of all user values in user_settings.json with JSON-based settings in LittleFS
- **WebServer**: Async web server providing REST API endpoints and serving the SolidJS web interface
- **Logger**: Comprehensive logging system with in-memory buffer and optional syslog support

### Sensor Management
- Automatic sensor type detection on startup (Dallas temperature vs water meter)
- Temperature readings in Fahrenheit with configurable thresholds
- Water flow rate calculation and pulse counting
- Real-time sensor status monitoring and error detection

### Pump Control Logic
- Temperature-based automatic cycling mode with hysteresis (ON/OFF thresholds)
- Manual ON/OFF control modes
- Flow error detection with automatic pump shutdown and retry logic
- Comprehensive statistics tracking (total on/off time, cycle counts)
- Configurable cycling intervals (on/off times)

### Web Interface
- Real-time status dashboard with sensor readings and pump state
- Manual pump controls with immediate feedback
- Settings management for all system parameters
- System logs viewing and management
- Responsive design using Tailwind CSS and DaisyUI components
- Auto-refreshing status updates every 2.5 seconds

### WiFi Management
- Automatic connection with configurable retry parameters
- Fallback to AP mode when connection fails
- mDNS support for local discovery (coopcontroller.local)
- Persistent WiFi credentials storage

## Development Environment

- Setup instructions
  1. Install Platform.io IDE or use VSCode with the Platform.io extension.
  2. Clone the repository from GitHub.
  3. Open the project in Platform.io.
  4. Install any required libraries via Platform.io Library Manager.
  5. Use available MCP tools, such as Context7 for the latest references for AI-assisted coding, playwright for web testing, and Kilo Code for code generation and completion.
- Key commands for building, running, and testing
  - Build: `pio run`
  - Upload: `pio run --target upload`
  - Monitor: `pio device monitor`
  - Web development: `cd web && npm i && npm run dev` (for UI development)
  - Web build: `cd web && npm run build` (builds and copies to /data for ESP32)
- References:
  - In project root, .url files link to important resources, ESP32 pinout images, and related documentation.
  - Platform.io Documentation: https://docs.platformio.org
  - Platform.io registry for libraries: https://registry.platformio.org
  - ESP32 Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
  - SolidJS Documentation: https://www.solidjs.com/docs/latest
  - Use MCP tools as necessary, including brave_search, to retrieve up-to-date information and coding assistance on the web.
- Any ambiguities or questions should be directed clarified before coding.

## Pin Configuration

- TEMP_METER_PIN (32): Primary sensor input - auto-detects Dallas temperature or water meter
- TEMP_METER_2_PIN (33): Secondary sensor input - auto-detects Dallas temperature or water meter  
- OUT_PUMP_PIN (26): Pump control output
- OUT_LIGHT_PIN (25): Light control output (future functionality)
- Door control and sense pins are defined in platformio.ini 

## Coding Style Guidelines

- Naming conventions: camelCase for variables and functions, PascalCase for classes in C++, kebab-case for files in the web project
- Formatting rules: 4 spaces for indentation, max line length of 100 characters
- Commenting standards: Use JSDoc for JavaScript/TypeScript, Doxygen for C++
- Commit message conventions: Use imperative mood, include issue references
- Preferred technologies and libraries: ESP32 with Platform.io for firmware, SolidJS for web interface; make sure C++ code is compatible with ESP32 environment, but modern C++ features are encouraged, std=c++11
- Error handling approaches: Use try-catch blocks where appropriate, validate inputs, and provide meaningful error messages
- Linting: Use ESLint for JavaScript/TypeScript, cpplint for C++; ensure code passes linting before committing
- Testing: Write unit tests for critical functions, use Jest for JavaScript/TypeScript testing, and Google Test for C++ testing
- Documentation: Maintain up-to-date README.md and inline documentation
- Follow professional coding practices and ensure code is clean, not overly-complicated, maintainable, and well-documented and follows SOLID principles.

## Restricted or Sensitive Files

- data/user_settings.json (contains WiFi/email/API/etc. credentials and system configuration); ensure this file is excluded from version control (.gitignore) and handle with care; user_settings.example.json is to be provided as a template without user/password/key data, but rather placeholder values.

## Component / Feature Creation Rules

- Instructions for component structure and design:
  - Follow the existing project structure for new components.
  - Ensure components are modular and reusable.
  - Adhere to coding style guidelines outlined above.
- Testing requirements: Write unit tests for new components, use Jest for JavaScript/TypeScript testing, and Google Test for C++ testing.
- Components and related functionality should be written in their own classes and files as appropriate to maintain separation of concerns.
- Documentation needs: Update README.md and inline documentation for new components; ensure clear explanations of functionality and design decisions and commands to set up and build the project; fully document code with comments.

## Pull Request and Collaboration Guidelines

<!-- - PR title formatting
- Testing and linting requirements before merge
- Review process notes -->

## Additional Notes

- The system uses LittleFS for persistent storage of settings and web assets
- Web interface is built with SolidJS, Vite, and Tailwind CSS
- The project includes build scripts to automatically build and compress web assets for ESP32 deployment
- Temperature sensors use DallasTemperature library with OneWire communication
- Water meter inputs use interrupt-based pulse counting with atomic operations for thread safety
- The pump controller includes comprehensive error handling and retry logic
- All settings are configurable through the web interface with immediate persistence
- Library attributions are to be listed in About.tsx in the web project so as to be available in the web UI

---

<!-- This outline is based on best practices for AGENTS.md files used to provide context and instructions to AI coding assistants [web:2][web:12]. -->