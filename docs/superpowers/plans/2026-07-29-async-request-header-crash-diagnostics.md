# Async Request-Header Crash Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and run test-only retained crash diagnostics that distinguish heap corruption from transient request-parser allocation exhaustion on the sacrificial USB board.

**Architecture:** A fixed RTC no-init breadcrumb ring records numeric allocation and lifecycle boundaries without heap allocation. The existing reproducible ESPAsyncWebServer patch script injects guarded request-parser hooks, while application hooks cover update/TLS boundaries and a main-loop pressure controller enables bounded experiments. Project-local Python tools capture serial/status and reproduce the one-user Update-plus-Logs interaction before a realistic overnight soak.

**Tech Stack:** C++17, Arduino-ESP32 3.3.9, ESP-IDF 5.5.4 heap APIs, ESPAsyncWebServer 3.10.1, PlatformIO, Python 3 standard library, pyserial from PlatformIO's environment.

## Global Constraints

- Only `/dev/cu.usbserial-0001` may be flashed, erased, reset, or intentionally crashed.
- Production `192.168.2.8` must not receive test traffic or deployment actions.
- Do not modify `platformio.ini` during the diagnostic phase.
- Preserve `WEB_RESP_MIN_MAX_ALLOC = 8192` in `lib/HAL_ESP32/HAL_ESP32.cpp`.
- Keep ESPAsyncWebServer pinned at 3.10.1.
- Diagnostic behavior must compile out unless `COOP_CRASH_DIAGNOSTICS=1` is supplied at build time.
- Heap-integrity scans require the additional `COOP_HEAP_INTEGRITY_DIAGNOSTICS=1` define.
- Retained records contain numeric metadata only; never retain headers, URLs, bodies, credentials, or tokens.
- Preserve the exact diagnostic ELF and SHA-256 for every flashed build.
- The test board is fully sacrificial; its existing firmware, filesystem, and NVS need not be preserved.
- No production deployment without separate explicit approval.

---

### Task 1: Retained breadcrumb subsystem

**Files:**
- Create: `include/CrashBreadcrumbs.h`
- Create: `src/CrashBreadcrumbs.cpp`
- Modify: `src/main.cpp:326-330`
- Test: `test/unit_desktop/test_CrashBreadcrumbs/test_CrashBreadcrumbs.cpp`

**Interfaces:**
- Produces: `enum class CrashBreadcrumbEvent : uint16_t`
- Produces: `void crashBreadcrumbRecord(CrashBreadcrumbEvent event, uint32_t arg0 = 0, uint32_t arg1 = 0)`
- Produces: `void crashBreadcrumbRequestDelta(int delta, uint32_t arg0 = 0, uint32_t arg1 = 0)`
- Produces: `void crashBreadcrumbReport(void (*logFunc)(const char *msg))`
- Produces: `void crashBreadcrumbPressureRequest(uint32_t targetMaxBlock, uint32_t durationMs)`
- Produces: `void crashBreadcrumbPressureUpdate()`
- Produces: `CrashPressureStatus crashBreadcrumbPressureStatus()`

- [ ] **Step 1: Write desktop tests for disabled-build no-ops and pressure parameter clamping**

Create tests that compile without `COOP_CRASH_DIAGNOSTICS` and assert the public API is callable, pressure status remains inactive, and no log callback is invoked.

```cpp
TEST(CrashBreadcrumbsTest, DisabledBuildIsNoOp) {
    int lines = 0;
    crashBreadcrumbRecord(CrashBreadcrumbEvent::UPDATE_REQUESTED, 1, 2);
    crashBreadcrumbPressureRequest(1, UINT32_MAX);
    auto status = crashBreadcrumbPressureStatus();
    EXPECT_FALSE(status.active);
    crashBreadcrumbReport([&lines](const char *) { ++lines; });
    EXPECT_EQ(lines, 0);
}
```

Use a plain function-pointer test callback rather than a capturing lambda if the compiler rejects conversion.

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `pio test -e unit-test-desktop -f test_CrashBreadcrumbs`

Expected: FAIL because `CrashBreadcrumbs.h` and its API do not exist.

- [ ] **Step 3: Define the guarded public API**

In `include/CrashBreadcrumbs.h`, define the event enum, pressure status struct, and declarations when diagnostics are enabled. Provide inline no-op implementations when diagnostics are disabled so desktop and release builds add no state or behavior.

Required event groups:

```cpp
enum class CrashBreadcrumbEvent : uint16_t {
    BOOT = 1,
    WEB_ACCEPT = 10,
    WEB_REQUEST_ALLOCATED = 11,
    WEB_DATA = 12,
    WEB_HEADER_PARSE = 13,
    WEB_HEADER_INSERT_BEFORE = 14,
    WEB_HEADER_INSERT_AFTER = 15,
    WEB_HEADERS_COMPLETE = 16,
    WEB_REQUEST_DESTROYED = 17,
    UPDATE_REQUESTED = 30,
    UPDATE_START = 31,
    UPDATE_HTTP_BEFORE = 32,
    UPDATE_HTTP_AFTER = 33,
    UPDATE_JSON_BEFORE = 34,
    UPDATE_JSON_AFTER = 35,
    UPDATE_END = 36,
    TLS_ALLOC_BEFORE = 40,
    TLS_ALLOC_SUCCESS = 41,
    TLS_ALLOC_REFUSED = 42,
    TLS_ALLOC_EXCEPTION = 43,
    PRESSURE_REQUESTED = 50,
    PRESSURE_ACTIVE = 51,
    PRESSURE_RELEASED = 52,
};
```

- [ ] **Step 4: Implement the ESP32 retained ring**

In `src/CrashBreadcrumbs.cpp`, under `#if defined(COOP_CRASH_DIAGNOSTICS) && defined(ESP_PLATFORM)`:

- use `RTC_NOINIT_ATTR` for a header and 64 fixed records;
- validate a fixed magic, version, and record capacity on first use;
- reserve sequence numbers with `__atomic_fetch_add`;
- snapshot tick count, event, arguments, free/default heap, largest default block, minimum heap, active request count, core, and current-task stack high-water mark;
- when integrity diagnostics are enabled, call `heap_caps_check_integrity(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL, false)` and encode pass/fail;
- write the record commit sequence last with release ordering;
- report records with a stack `char line[240]` and the supplied callback;
- clear the magic only after reporting.

Do not call `String`, logger, `printf` to heap-backed streams, `new`, or `malloc` while recording.

- [ ] **Step 5: Implement bounded main-loop heap pressure**

Use a fixed array of at most 96 pointers. Clamp target max block to `8192..65536`, duration to `1000..60000` ms, use chunks no larger than 4096 bytes, and stop if free default heap would fall below 50000 bytes. `crashBreadcrumbPressureUpdate()` allocates or releases only from the main loop. Every pressure request automatically expires.

- [ ] **Step 6: Report retained breadcrumbs during boot**

In `src/main.cpp`, call `crashBreadcrumbReport()` immediately after `crashDiagnosticsCheck()` with the same logger callback, then record `BOOT`. Call `crashBreadcrumbPressureUpdate()` once per main loop after `updateManager.update()`.

- [ ] **Step 7: Run tests and default firmware build**

Run:

```bash
pio test -e unit-test-desktop -f test_CrashBreadcrumbs
pio run -e esp32-dev
```

Expected: tests PASS; firmware build PASS without diagnostic state linked into behavior.

- [ ] **Step 8: Commit**

```bash
git add include/CrashBreadcrumbs.h src/CrashBreadcrumbs.cpp src/main.cpp test/unit_desktop/test_CrashBreadcrumbs
 git commit -m "feat(diag): retain allocation breadcrumbs across crashes"
```

### Task 2: Framework and update/TLS boundary hooks

**Files:**
- Modify: `build_scripts/patch_async_webserver.py`
- Modify: `lib/UpdateManager/UpdateManager.cpp:144-276,553-558`
- Modify: `lib/HAL_ESP32/HAL_ESP32.cpp:874-940`
- Test: `test/unit_desktop/test_UpdateManager/test_UpdateManager.cpp`

**Interfaces:**
- Consumes: `crashBreadcrumbRecord()` and `crashBreadcrumbRequestDelta()` from Task 1.
- Produces: idempotently patched `WebServer.cpp` and `WebRequest.cpp` diagnostic hooks guarded by `COOP_CRASH_DIAGNOSTICS`.

- [ ] **Step 1: Add a patch-script contract test**

Create a Python unittest in `test/build_scripts/test_patch_async_webserver.py` around pure helper functions extracted from the patch script. Feed minimal 3.10.1 `WebServer.cpp` and `WebRequest.cpp` fixtures and assert:

- accept `new` becomes `new(std::nothrow)` once;
- diagnostic include is inserted once;
- header-before/header-after hooks surround `_headers.emplace_back`;
- request destruction decrements the active count;
- applying transforms twice is byte-identical.

- [ ] **Step 2: Run the script test and verify it fails**

Run: `python3 -m unittest test.build_scripts.test_patch_async_webserver -v`

Expected: FAIL because the transform helpers and request-parser hooks do not exist.

- [ ] **Step 3: Refactor and extend the patch script**

Keep the current accept patch. Add deterministic transforms for `WebRequest.cpp` and guarded `#include "CrashBreadcrumbs.h"` insertion. Inject numeric hooks at:

```cpp
crashBreadcrumbRecord(CrashBreadcrumbEvent::WEB_DATA, len, _parseState);
crashBreadcrumbRecord(CrashBreadcrumbEvent::WEB_HEADER_PARSE, _headers.size(), _temp.length());
crashBreadcrumbRecord(CrashBreadcrumbEvent::WEB_HEADER_INSERT_BEFORE, _headers.size(), _temp.length());
_headers.emplace_back(std::move(header));
crashBreadcrumbRecord(CrashBreadcrumbEvent::WEB_HEADER_INSERT_AFTER, _headers.size(), _temp.length());
```

Record header completion and request destruction. Increment active requests only after request-object allocation succeeds. Keep all calls under `#ifdef COOP_CRASH_DIAGNOSTICS`.

If an expected 3.10.1 source block is absent, terminate the build with a clear exception rather than printing a warning and silently shipping unpatched code.

- [ ] **Step 4: Add application boundary hooks**

Instrument `UpdateManager::requestCheck()` and `checkForUpdates()` before/after HTTP fetch, before/after JSON parse, and at every return path. Instrument `HAL_ESP32::createSecureClient()` before allocation and on refusal, success, and exception. Numeric arguments are lengths, return/result codes, and heap values only.

- [ ] **Step 5: Run patch tests, UpdateManager tests, and firmware build**

Run:

```bash
python3 -m unittest test.build_scripts.test_patch_async_webserver -v
pio test -e unit-test-desktop -f test_UpdateManager
pio run -e esp32-dev
```

Expected: all PASS and the build log confirms both vendored files are already patched or newly patched.

- [ ] **Step 6: Commit**

```bash
git add build_scripts/patch_async_webserver.py lib/UpdateManager/UpdateManager.cpp lib/HAL_ESP32/HAL_ESP32.cpp test/build_scripts
 git commit -m "feat(diag): trace async request and TLS boundaries"
```

### Task 3: Diagnostic-only pressure endpoints

**Files:**
- Modify: `lib/CoopControllerWebServer/CoopControllerWebServer.cpp:1200-1405`
- Test: `test/unit_desktop/test_CoopControllerWebServer/test_CoopControllerWebServer.cpp`

**Interfaces:**
- Consumes: pressure controller API from Task 1.
- Produces: `POST /diagnostics/heap_pressure?target_max_block=<bytes>&duration_ms=<ms>` and `GET /diagnostics/heap_pressure` only in diagnostic builds.

- [ ] **Step 1: Write handler registration and validation tests**

Add diagnostic-build tests that assert the route queues numeric pressure settings, rejects missing/non-numeric parameters with 400, and returns status fields `active`, `target_max_block`, `expires_in_ms`, `allocated_blocks`, `free_heap`, and `max_block`.

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `pio test -e unit-test-desktop -f test_CoopControllerWebServer`

Expected: FAIL because the diagnostic routes are not registered.

- [ ] **Step 3: Implement guarded endpoints**

Under `#ifdef COOP_CRASH_DIAGNOSTICS`, register the two routes. The POST handler parses only integer query parameters, invokes `crashBreadcrumbPressureRequest()`, and responds immediately. Allocation remains in the main-loop updater. The GET handler serializes the fixed numeric status.

- [ ] **Step 4: Run focused tests and firmware build**

Run:

```bash
pio test -e unit-test-desktop -f test_CoopControllerWebServer
pio run -e esp32-dev
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/CoopControllerWebServer/CoopControllerWebServer.cpp test/unit_desktop/test_CoopControllerWebServer
 git commit -m "feat(diag): add bounded heap pressure controls"
```

### Task 4: Project-local capture and reproduction tools

**Files:**
- Create: `tools/crash_diagnostics/serial_capture.py`
- Create: `tools/crash_diagnostics/run_workload.py`
- Create: `tools/crash_diagnostics/README.md`
- Test: `test/tools/test_crash_diagnostics_tools.py`

**Interfaces:**
- Produces: `serial_capture.py --port /dev/cu.usbserial-0001 --output <path>`.
- Produces: `run_workload.py --host <test-ip> --duration <seconds> --clients <1|2> --output <directory> [--pressure-target <bytes>]`.

- [ ] **Step 1: Write parser/scheduler tests**

Test URL rejection, especially `192.168.2.8`; status JSON extraction; update-status polling termination; bounded client count; and deterministic five-second Logs scheduling using a fake clock and fake HTTP transport.

- [ ] **Step 2: Run tool tests and verify they fail**

Run: `python3 -m unittest test.tools.test_crash_diagnostics_tools -v`

Expected: FAIL because the tools do not exist.

- [ ] **Step 3: Implement serial capture**

Use `serial.Serial(port, 115200, timeout=1)`, append timestamped decoded lines, reconnect after disconnect, flush every line, and handle SIGTERM by closing the port. Refuse any port other than `/dev/cu.usbserial-0001` unless `--allow-port` explicitly matches the provided port.

- [ ] **Step 4: Implement bounded workload runner**

Use Python standard-library `urllib.request` and `threading`. Hard-refuse host `192.168.2.8`, `coopcontroller.local`, or any host resolving to `192.168.2.8`. Cap clients at two and header-count experiments at 32. Implement the exact sequence:

```text
GET /version
GET /update/check
GET /update/status every 0.5 s until not checking
GET /update/check_result
GET /logs immediately and every 5 s
GET /system_status every 2 s
```

Write JSONL events with monotonic and wall timestamps, HTTP status, latency, uptime, reset reason, heap fields, and exception class. Do not store response bodies except parsed status/version fields. Detect reboot, persistent 500, and a 30-second HTTP wedge.

- [ ] **Step 5: Document exact commands and safety boundaries**

README commands must name only `/dev/cu.usbserial-0001` and a discovered test-board IP. State that production `192.168.2.8` is blocked in code.

- [ ] **Step 6: Run tool tests**

Run: `python3 -m unittest test.tools.test_crash_diagnostics_tools -v`

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add tools/crash_diagnostics test/tools
 git commit -m "test(diag): add bounded crash reproduction harness"
```

### Task 5: Build, flash, validate, and launch soak

**Files:**
- Create at runtime: `diagnostics/runs/<run-id>/` (gitignored)
- Modify: `.gitignore` to ignore `diagnostics/runs/`
- Preserve at runtime: diagnostic `firmware.elf`, `firmware.bin`, SHA-256, serial log, workload JSONL, and run metadata.

**Interfaces:**
- Consumes: Tasks 1-4.
- Produces: an accelerated diagnostic report and a running/completed overnight soak scoped to the USB test board.

- [ ] **Step 1: Add runtime-output ignore rule**

Add only `diagnostics/runs/` to `.gitignore`; keep diagnostic source and documentation tracked.

- [ ] **Step 2: Run complete pre-flash verification**

Run:

```bash
python3 -m unittest discover -s test/build_scripts -v
python3 -m unittest discover -s test/tools -v
pio test -e unit-test-desktop
pio run -e esp32-dev
```

Expected: all tests pass and default firmware builds.

- [ ] **Step 3: Build diagnostic firmware without editing PlatformIO configuration**

Use PlatformIO's command-line project option to append defines to the existing environment:

```bash
pio run -e esp32-dev \
  -O 'build_flags=${env:esp32-dev.build_flags} -DCOOP_CRASH_DIAGNOSTICS=1'
```

Before flashing, inspect `pio run ... -t envdump` and the built symbols to verify normal inherited flags remain and diagnostic symbols are present.

- [ ] **Step 4: Preserve the exact build artifacts**

Create a run directory using a UTC timestamp. Copy `.pio/build/esp32-dev/firmware.elf` and `firmware.bin`; write `shasum -a 256` output, git commit, git diff, build command, serial port, and an explicit `production_target=untouched` marker.

- [ ] **Step 5: Start serial capture and flash only the USB board**

Start `serial_capture.py` in the background, then run:

```bash
pio run -e esp32-dev \
  -O 'build_flags=${env:esp32-dev.build_flags} -DCOOP_CRASH_DIAGNOSTICS=1' \
  --target upload --upload-port /dev/cu.usbserial-0001
```

Do not use OTA upload or any network upload target.

- [ ] **Step 6: Discover and verify the test-board IP**

Parse serial for the assigned address. Reject `192.168.2.8`. Verify `/version` matches the preserved ELF build and `/system_status` responds. If Wi-Fi credentials are absent, provision only the USB board using its AP or serial-supported setup; do not copy production settings files into the filesystem image.

- [ ] **Step 7: Run ten-minute baseline**

Verify idle heap, normal `/logs`, update check, breadcrumb boot output, no secret-bearing records, and no instrumentation-induced reset.

- [ ] **Step 8: Run 30-60 minute accelerated workload**

Run one client first. If clean, tighten overlap and run two clients. If still clean, use measured pressure targets with automatic expiry. Stop escalation after the first diagnostic-quality panic, integrity failure, wedge, or reproducible admission boundary.

- [ ] **Step 9: Decode and classify any reset**

Use only the preserved ELF. Record whether the last committed breadcrumb shows integrity good or bad, active request/header ordinal, update/TLS stage, and heap snapshot. Do not implement a hardening fix during this run.

- [ ] **Step 10: Launch overnight realistic soak**

Start one-client `run_workload.py` for at least eight hours with serial capture still active. Run in a harness-tracked background command so completion re-enters the session. Store PID/process metadata in the run directory.

- [ ] **Step 11: Commit source and ignore rule before waiting**

```bash
git add .gitignore include src lib build_scripts tools test docs/superpowers/plans
 git commit -m "feat(diag): instrument async request allocation failures"
```

Do not add runtime logs or firmware artifacts to git.
