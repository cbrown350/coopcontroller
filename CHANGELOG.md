# Changelog

All notable changes to the Coop Controller firmware and web UI.

Versions are git tags (`vMAJOR.MINOR.PATCH`); the release workflow injects the
tag as `FIRMWARE_VERSION_RAW` at build time. The device reports `dev-<timestamp>`
for local/un-tagged builds. Entries below record what shipped in each release,
plus any deployed-but-untagged builds.

---

## [v0.9.1] — unreleased

Bumped for the pump/light GPIO swap. **Not yet tagged**: the release workflow
only publishes tags that are ancestors of `master`, and this change sits on
`diagnose/async-request-header-crash`, which also carries the diagnostics
instrumentation. Tag on master once that branch is merged.

### Fixed

- **Pump moved to the working MOSFET (GPIO 25/26 swap).** The MOSFET driving
  the pump failed. The pump was rewired to the working light MOSFET, so the
  two output assignments were swapped to match:

  | Constant | Was | Now |
  |---|---|---|
  | `OUT_PUMP_PIN` | 26 | **25** (former light MOSFET) |
  | `OUT_LIGHT_PIN` | 25 | **26** (failed pump MOSFET, unused) |

  Both are compile-time `-D` macros; no runtime setting, API field, or web UI
  control stores a GPIO number. Verified in the flashed ELF: `setup()` loads
  25 before `PumpController::begin`, and `LightController::begin` loads 26
  before `pwmAttachPin`. Flashed firmware-only (filesystem untouched) as
  `dev-2026-11-07T194722Z`; all 91 settings survived unchanged.

  The pins must stay distinct — the light drives its pin through LEDC PWM, and
  `ledcAttach()` reroutes the pad's output away from the GPIO output register,
  so a shared pin would let the light silently steal the pump's pin.

- Emulator `EMU_READ_PUMP_PIN` / `EMU_READ_LIGHT_PIN` comments updated to match;
  physical emulator harness cables between main-controller GPIOs 25 and 26 must
  be swapped by hand if that rig is used.

### Changed

- `test_LightController` now asserts `OUT_LIGHT_PIN` instead of the literal `25`,
  so the test tracks the macro rather than needing an edit on every pin change.

### Known issues carried into this release

- **Image size.** 1,734,640 bytes = 98% of the 0x1B0000 OTA slot (34,832 bytes
  free), up from 1,629,472 deployed. The increase came from a local toolchain
  reinstall, not from the pin swap, and is not root-caused. Very little growth
  headroom remains.
- **Response heap gate differs from master.** This branch has
  `WEB_RESP_MIN_MAX_ALLOC = 8192`; `master` and `v0.9.0` have `32768`. The
  deployed build uses 8192 — that is the value that keeps the board answering
  on this hardware, where the largest contiguous ordinary-malloc block sits at
  ~32,756 bytes (just under 32768, which would 500 every response). Reconcile
  deliberately before merging; do not release master's 32768 to this board
  without verifying it.
- **Build environment (host, not repo).** `pio run` fails at link with
  `ModuleNotFoundError: No module named 'SCons.Tool.FortranCommon'` when the
  pioarduino platform's `tool-scons` spec (`4.40801.0`) disagrees with
  PlatformIO core's (`~4.41101.0`). Their installers alternate and delete
  `~/.platformio/packages/tool-scons` mid-build. Fix: set
  `packages."tool-scons".package-version` to `4.41101.0` in the active
  platform's `platform.json`, then
  `pio pkg install -g --tool "platformio/tool-scons@~4.41101.0"`. Must be
  reapplied after a platform reinstall or `pio pkg update`.

---

## [v0.9.0] — 2026-07-23

Framework migration to pioarduino (arduino-esp32 3.3.9 / ESP-IDF 5.5.4 /
mbedtls 3.x). Targeted the long-standing `async_tcp` `std::bad_alloc` panic
under concurrent web + TLS load, whose root cause was ESPAsyncWebServer's own
throwing `new` in its TCP-accept path — below any app-level handler and
therefore unfixable by app-level heap gates alone. Also patched the library's
allocation to nothrow and fixed two contributing issues (an oversized Logger
static buffer, an SNTP/DNS boot race).

Validated with a realistic 1–2 concurrent user load test: 26 minutes, zero
crashes, stable heap. 846/846 desktop tests passed.

### Added

- Lean mbedtls/IDF config (`build_scripts/lean_mbedtls.sdkconfig`): Bluetooth
  disabled, TLS out-buffer 16 KB → 4 KB, peer cert not retained.
- LittleFS read cache 512 → 4096 bytes to cut cross-core flash-read stalls.

### Changed

- `async_tcp` pinned to the same core as `loopTask`
  (`CONFIG_ASYNC_TCP_RUNNING_CORE=1`) to avoid cross-core scheduler suspends
  during static-asset flash reads.
- Serialized outbound TLS plus a network watchdog to convert a soft-wedge into
  a self-recovering panic.

### Known issues

- OTA `/update/install` still stalls on large TLS downloads under IDF 5.5.4
  lwIP; `/update/check` works.

---

## [v0.8.3] — 2026-07-21

Diagnostics and hardening, **not** a complete crash fix.

- Added `ESP.getMaxAllocHeap()` (largest contiguous block) to the 10-second
  telemetry line and `/system_status`.
- Capped outbound syslog UDP to one packet per 200 ms so a log burst can no
  longer amplify into a runaway lwIP `ENOMEM` flood. Serial and in-RAM `/logs`
  stay complete.

Known remaining issue at the time: under concurrent web + TLS load the board
could still panic via `std::bad_alloc` inside ESPAsyncWebServer `addHeader()` on
`async_tcp` — confirmed with remote syslog fully disabled, which pointed at a
framework-level fix (the pioarduino migration released as v0.9.0).

---

## Earlier releases

See `git tag -l -n20` for tag messages on v0.4.x through v0.8.2, and
`docs/feature-tracker-finished.md` for the feature history.
