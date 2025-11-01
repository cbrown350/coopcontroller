# AGENTS.md

## Project Overview

- This is the Coop Controller project, designed to manage and monitor a chicken coop environment. Currently, it uses an ESP32 microcontroller in Platform.io to handle temperature sensors and control outputs like pumps and lights. The project includes a web-based user interface built with SolidJS for easy configuration and monitoring. The project structure is based on another project and needs to be updated and changed for the new functions.
- See the ESP32 pin functions in the platformio.ini and README.md files.
- Currently, we need a quick-and-dirt implementation to read two temperature sensors and control a pump based on those readings.
- When the temperature goes below a certain threshold set in settings (default 34F), the pump should activate go into a cycling mode to prevent water from freezing, at intervals set in the settings (default 5 minutes on/10 minutes off).
- The web server status and devserver code includes a lot of entries from the old project and needs many deletions and additions.
- The other files mainly need additions and modifications.
- The Wifi should attempt to connect and revert to AP mode if it cannot connect after a certain number of tries. However, the functionality of temperature reading and pump control is the priority and must continue regardless of Wifi status.
- The TEMP_METER_PIN and TEMP_METER_2_PIN define the pins for the two temperature sensor and the water meter pulse inputs. Initially, the first one should be probed to see if a Dallas temperature sensor is connected, and if not, it should default to a water meter input. The same for the second pin.
- The OUT_PUMP_PIN defines the pin that controls the pump output. The pump should be activated based on the temperature readings and the cycling intervals set in the settings.
- The OUT_LIGHT_PIN defines the pin that controls the light output. This can be used for future functionality, such as turning on a light based on sunrise/sunset times or manual control via the web interface.
- The water meter pulse input pins (WATER_METER_PIN) should be configured to read the water flow rate and update the settings accordingly. It should also detect an error situation since if there is no flow when the pump is on, there is likely a blockage or other issue. When there's a flow error detected, it should log the error and possibly notify the user via the web interface. It should stop the pump to prevent damage and then retry after a certain interval in settings (default 2 minutes).
- The project should maintain a modular structure, allowing for easy addition of new features and components in the future.
- The web interface should provide real-time updates on temperature readings, pump status, and water flow rate, along with configuration options for the various settings.
- The code should be well-documented, with clear explanations of the functionality and any important design decisions.

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
- References:
  - In project root, .url files link to important resources, ESP32 pinout images, and related documentation.
  - Platform.io Documentation: https://docs.platformio.org
  - Platform.io registry for libraries: https://registry.platformio.org
  - ESP32 Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
  - SolidJS Documentation: https://www.solidjs.com/docs/latest
  - Use MCP tools as necessary, including brave_search, to retrieve up-to-date information and coding assistance on the web.

## Coding Style Guidelines

- Naming conventions: camelCase for variables and functions, PascalCase for classes in C++, kebab-case for files in the web project
- Formatting rules: 4 spaces for indentation, max line length of 100 characters
- Commenting standards: Use JSDoc for JavaScript/TypeScript, Doxygen for C++
- Commit message conventions: Use imperative mood, include issue references
- Preferred technologies and libraries: ESP32 with Platform.io for firmware, SolidJS for web interface; make sure C++ code is compatible with ESP32 environment, but modern C++ features are encouraged, std=c++17 for most and std=c++11 where needed for compatibility
- Error handling approaches: Use try-catch blocks where appropriate, validate inputs, and provide meaningful error messages
- Linting: Use ESLint for JavaScript/TypeScript, cpplint for C++; ensure code passes linting before committing
- Testing: Write unit tests for critical functions, use Jest for JavaScript/TypeScript testing, and Google Test for C++ testing
- Documentation: Maintain up-to-date README.md and inline documentation

## Restricted or Sensitive Files

- data/user_settings.json

## Component / Feature Creation Rules

- Instructions for component structure and design:
  - Follow the existing project structure for new components.
  - Ensure components are modular and reusable.
  - Adhere to coding style guidelines outlined above.
- Testing requirements: Write unit tests for new components, use Jest for JavaScript/TypeScript testing, and Google Test for C++ testing.
- Documentation needs: Update README.md and inline documentation for new components; ensure clear explanations of functionality and design decisions; fully document code with comments.

## Pull Request and Collaboration Guidelines

<!-- - PR title formatting
- Testing and linting requirements before merge
- Review process notes -->

## Additional Notes

<!-- - Any other important info to guide AI coding agents -->

---

<!-- This outline is based on best practices for AGENTS.md files used to provide context and instructions to AI coding assistants [web:2][web:12]. -->
