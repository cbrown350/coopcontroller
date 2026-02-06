# CLAUDE.md - Coop Controller

This is the entry point for Claude Code and AI agent context.

## Project Context

See [Agents.md](Agents.md) for full project documentation including:

- **Quick reference** - Platform, build commands, test status
- **Documentation index** - Links to all detailed subdocuments in `docs/`
- **Critical rules** - Compilation requirements, PlatformIO approval process, HAL pattern, testing requirements
- **Active features** - Current system capabilities and recent completions
- **Project structure** - Codebase organization overview

## Documentation Structure

```
Agents.md                          # Project overview, rules, and doc index
docs/
├── architecture.md                # System design, components, HAL, dependencies
├── hardware.md                    # Hardware requirements, pin configuration
├── api-reference.md               # REST API endpoints (main controller)
├── development-guide.md           # Setup, coding standards, testing guide
├── feature-tracker.md             # Completed/planned features roadmap
├── hardware-emulator.md           # Emulator docs (architecture, API, scenarios)
├── contributing.md                # PR workflow, code review, branch management
└── troubleshooting.md             # Common issues, debug tools, recovery
```

## Quick Commands

```bash
pio run                            # Build firmware
pio test                           # Run unit tests
cd web && npm run build            # Build web UI
```
