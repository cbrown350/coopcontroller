# AGENTS.md

## Project Overview

- This is the Coop Controller project, designed to manage and monitor a chicken coop environment. Currently, it uses an ESP32 microcontroller to handle temperature sensors and control outputs like pumps and lights. The project includes a web-based user interface built with SolidJS for easy configuration and monitoring. The project structure is based on another project and needs to be updated and changed for the new functions.
- See the ESP32 pin functions in the platformio.ini and README.md files.
- Currently, we need a quick-and-dirt implementation to read two temperature sensors and control a pump based on those readings.
- When the temperature goes below a certain threshold set in settings (default 34F), the pump should activate go into a cycling mode to prevent water from freezing, at intervals set in the settings (default 5 minutes on/10 minutes off).
- The web server status and devserver code includes a lot of entries from the old project and needs many deletions and additions.
- The other files mainly need additions and modifications.
- The Wifi should attempt to connect and revert to AP mode if it cannot connect after a certain number of tries. However, the functionality of temperature reading and pump control is the priority and must continue regardless of Wifi status.
- The TEMP_METER_PIN and TEMP_METER_2_PIN define the pins for the two temperature sensor and the water meter pulse inputs. Initially, the first one should be probed to see if a Dallas temperature sensor is connected, and if not, it should default to a water meter input. The same for the second pin.
- The OUT_PUMP_PIN defines the pin that controls the pump output. The pump should be activated based on the temperature readings and the cycling intervals set in the settings.
- The OUT_LIGHT_PIN defines the pin that controls the light output. This can be used for future functionality, such as turning on a light based on sunrise/sunset times or manual control via the web interface.
- The water meter pulse input pins (WATER_METER_PIN) should be configured to read the water flow rate and update the settings accordingly. It should also detect an error situation since if there is no flow when the pump is on, there is likely a blockage or other issue.
- The project should maintain a modular structure, allowing for easy addition of new features and components in the future.
- The web interface should provide real-time updates on temperature readings, pump status, and water flow rate, along with configuration options for the various settings.
- The code should be well-documented, with clear explanations of the functionality and any important design decisions.


## Development Environment

<!-- - Setup instructions
- Key commands for building, running, and testing
- Any environment variables or dependencies -->

## Coding Style Guidelines

<!-- - Naming conventions
- Formatting rules
- Preferred technologies and libraries
- Error handling approaches -->

## Restricted or Sensitive Files

<!-- - List files or directories that AI agents should not access or modify. -->

## Component / Feature Creation Rules

<!-- - Instructions for component structure and design
- Testing requirements
- Documentation needs -->

## Pull Request and Collaboration Guidelines

<!-- - PR title formatting
- Testing and linting requirements before merge
- Review process notes -->

## Additional Notes

<!-- - Any other important info to guide AI coding agents -->


---

<!-- Fill in each section with concise and clear instructions for AI tools like Kilo Code to follow, ensuring your project conventions and restrictions are respected.

This outline is based on best practices for AGENTS.md files used to provide context and instructions to AI coding assistants [web:2][web:12]. -->
