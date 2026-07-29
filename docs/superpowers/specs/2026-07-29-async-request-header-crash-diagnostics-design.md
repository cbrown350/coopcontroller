# Async Request-Header Crash Diagnostics Design

**Date:** 2026-07-29  
**Status:** Approved design; implementation not started  
**Target:** Sacrificial USB ESP32 test board on `/dev/cu.usbserial-0001`  
**Production:** Must remain untouched

## 1. Purpose

Determine why ESPAsyncWebServer's per-header list-node allocation failed on the production board while sampled heap appeared healthy, then use that evidence to choose the narrowest robust fix.

The diagnostic phase must distinguish between:

1. a transient but valid heap-exhaustion or fragmentation window; and
2. heap corruption that occurred before the request parser exposed it.

No framework hardening will be selected merely because it reduces crash frequency in one stress run.

## 2. Confirmed incident mechanism

The production coredump was decoded against its byte-matching ELF (`01ba69a6...`). The confirmed path was:

```text
async_tcp
  -> AsyncWebServerRequest::_onData()
  -> AsyncWebServerRequest::_parseLine()
  -> AsyncWebServerRequest::_parseReqHeader()
  -> std::list<AsyncWebHeader>::emplace_back()
  -> operator new()
  -> __cxa_allocate_exception
  -> std::terminate()
  -> panic reset
```

The failing operation is ESPAsyncWebServer 3.10.1's inbound-header insertion in `WebRequest.cpp`. It executes before route dispatch and therefore cannot be protected by the application's response wrappers, `/logs` chunk callback, or handler-level exception guards.

The deployed 8 KiB `WEB_RESP_MIN_MAX_ALLOC` response gate is a separate response-path safeguard. It must remain unchanged during diagnosis.

## 3. Scope

### In scope

- Test-only diagnostic instrumentation on the current stack.
- Flashing, erasing, resetting, and intentionally crashing the USB test board.
- Reproducing the production interaction pattern with one browser-equivalent client.
- Controlled escalation to two clients and bounded heap pressure.
- Retained pre-crash breadcrumbs and exact ELF preservation.
- An accelerated 30-60 minute run followed by a realistic overnight soak.
- An evidence-gated recommendation for corruption repair, framework hardening, or further diagnosis.

### Out of scope for the diagnostic phase

- Any production-board flash, reset, load test, or configuration change.
- Any `platformio.ini` change.
- Upgrading or forking ESPAsyncWebServer.
- Deploying a hardening patch before the diagnostic result identifies the appropriate path.
- Changing `WEB_RESP_MIN_MAX_ALLOC` from 8192.
- Bundling the separate uninitialized `VersionInfo::size_bytes` fix unless it interferes with measurements.

## 4. Constraints and authorization

- The USB board is fully sacrificial. Its current firmware, filesystem, NVS settings, and crash history do not need to be preserved.
- Production remains untouched until a later, explicit deployment approval.
- The user selected diagnostics first. Any later pinned-fork or other `platformio.ini` proposal requires a new approval based on test evidence.
- The current working-tree change that lowers `WEB_RESP_MIN_MAX_ALLOC` from 32768 to 8192 must be preserved.
- Every decoded reset must use a preserved, byte-matching ELF.

## 5. Diagnostic build architecture

### 5.1 Compile-time isolation

Instrumentation will be enabled only when the build command supplies a diagnostic define, for example through `PLATFORMIO_BUILD_FLAGS`, without editing `platformio.ini`.

Two levels are required:

- `COOP_CRASH_DIAGNOSTICS=1`: lightweight numeric breadcrumbs and heap snapshots.
- `COOP_HEAP_INTEGRITY_DIAGNOSTICS=1`: the lightweight hooks plus targeted `heap_caps_check_integrity()` calls.

Normal builds compile all diagnostic hooks out. Framework source hooks injected by `build_scripts/patch_async_webserver.py` must also be guarded by these defines.

### 5.2 Retained breadcrumb ring

A fixed-size ring in RTC no-init memory will retain recent events across panic/software reset. It must use no dynamic allocation and must tolerate a partially written final record.

Each record contains numeric fields only:

- monotonically increasing sequence number;
- event identifier;
- `millis()` or equivalent monotonic timestamp;
- current task/core identifier;
- free ordinary heap;
- largest ordinary-malloc block;
- minimum free heap;
- active inbound request count;
- request-local header ordinal and line length, when applicable;
- update/TLS stage, when applicable;
- async task stack high-water mark, when available;
- heap-integrity result in the integrity build.

A record is valid only after its commit sequence is written. Ring reservation uses an atomic increment; event recording does not take application mutexes, format strings, log, or allocate memory.

The ring must never store:

- header names or values;
- authorization data;
- request URLs or bodies;
- Wi-Fi credentials, API keys, or tokens.

### 5.3 Boot-time reporting

After the logger and syslog are available, boot diagnostics will:

1. emit the existing coredump summary;
2. emit retained breadcrumb records in sequence order;
3. identify the diagnostic build level and ELF hash prefix;
4. clear or advance the retained generation only after reporting completes.

The preserved ring supplements the coredump; it must not replace or erase the coredump before the existing summary is read.

### 5.4 Instrumented boundaries

Lightweight records will cover these boundaries:

#### ESPAsyncWebServer / AsyncTCP

- accepted TCP client before request allocation;
- successful request-object allocation;
- `_onData()` entry;
- immediately before parsing a non-empty header line;
- immediately before `_headers.emplace_back()`;
- immediately after successful insertion;
- end of headers;
- handler dispatch;
- request abort/disconnect/destruction;
- active request count changes.

#### Update and TLS

- update check requested;
- `UpdateManager::checkForUpdates()` start;
- before and after `httpGet()`;
- before and after manifest deserialization;
- version comparison complete;
- update check end/error;
- secure-client allocation start/success/failure;
- TLS connect start/end;
- secure-client destruction.

The heavier build checks ordinary/default-capability heap integrity at the same targeted boundaries. It will not scan the entire heap continuously in the first diagnostic run because that could materially change task timing.

### 5.5 Test-only heap-pressure controller

A diagnostic-only endpoint will enqueue a pressure request; allocation itself occurs on the main loop, not in `async_tcp`.

Requirements:

- accept a target largest-block value and maximum hold duration;
- allocate into a fixed compile-time array of pointers using bounded chunk sizes;
- stop before a conservative minimum free-heap floor;
- automatically release all allocations at the deadline, on cancellation, or when Wi-Fi becomes unhealthy;
- expose current pressure state numerically;
- remain disabled until explicitly triggered;
- compile out of normal builds.

The endpoint only queues numeric parameters and responds immediately. It must not run the pressure loop in the async handler.

## 6. Reproduction harness

The previous `scratchpad` scripts are not present in the repository. The new diagnostic harness must be checked in under a stable tools directory and use bounded, reproducible workloads.

The harness will:

- discover or accept the test board's IP;
- continuously capture `/system_status`, `/version`, uptime, reset reason, and request failures;
- model one browser-equivalent client with normal browser headers;
- request `/update/check`;
- poll `/update/status` every 500 ms until completion;
- request `/update/check_result`;
- request `/logs` immediately and every five seconds;
- preserve timestamps and response status without storing credentials;
- detect reboot, wedge, persistent 500 state, and connection recovery;
- preserve the exact firmware ELF and its SHA-256 alongside each run's results.

Serial capture runs concurrently using PlatformIO's Python environment, which already supplies serial-port support.

## 7. Test sequence

### Stage 1: Board baseline

1. Build the lightweight diagnostic firmware from the current source and current ESPAsyncWebServer 3.10.1 stack.
2. Flash `/dev/cu.usbserial-0001`.
3. Capture serial continuously and discover the assigned IP.
4. Verify firmware/ELF identity, reset reason, heap baseline, `/logs`, update check, and ordinary UI behavior.
5. Observe idle operation for approximately ten minutes.

Baseline acceptance criteria:

- normal requests succeed;
- breadcrumbs are emitted without malformed or sensitive fields;
- idle heap remains bounded;
- no unexpected integrity or watchdog failure appears solely from instrumentation.

### Stage 2: Exact realistic trigger

For 30-60 minutes, repeatedly execute the production-like sequence with one browser-equivalent client:

1. load update metadata;
2. request an update check;
3. poll update status while outbound TLS is active;
4. read the completed check result;
5. open `/logs` and continue five-second polling.

Capture serial, syslog, status, uptime, HTTP failures, heap extrema, active request count, and retained breadcrumbs.

### Stage 3: Controlled escalation

Only if Stage 2 remains clean:

1. tighten the overlap between update polling and `/logs` requests;
2. add one additional browser-equivalent client;
3. activate bounded heap pressure in measured steps;
4. send requests using normal captured header counts;
5. then test bounded larger header counts to locate the admission boundary.

This stage is not an unbounded flood or denial-of-service test. It is a local, authorized test against the sacrificial board with fixed concurrency, header limits, timeouts, and automatic pressure release.

### Stage 4: Integrity discrimination

- If a panic occurs while all preceding integrity checks are good, classify the observed event as framework allocation exhaustion/fragmentation under a clean heap.
- If integrity changes from good to bad, narrow the interval until the earliest corrupting boundary is identified.
- If the lightweight build lacks enough evidence, repeat the smallest reproducing scenario with the integrity build.
- Do not combine unrelated fixes while narrowing the boundary.

### Stage 5: Overnight realistic soak

After the accelerated phase, run the one-user realistic sequence overnight.

Pass criteria:

- no panic or watchdog reset;
- no network wedge;
- no heap-integrity failure;
- no persistent `500 low memory` state;
- active requests return to baseline after each cycle;
- free heap and largest block recover to a bounded plateau after each update/TLS cycle;
- `/logs`, update status, weather/LLM, and normal controller loop remain responsive.

A clean soak defines the tested envelope; it does not by itself prove the production incident fixed.

## 8. Evidence gate and resulting implementation paths

### Outcome A: Heap corruption confirmed

If integrity fails before request-header insertion:

1. identify the earliest good-to-bad boundary;
2. create the smallest reproduction for that component;
3. write a failing regression test or test-board scenario;
4. fix the corruption source only;
5. rerun the realistic sequence, controlled escalation, and overnight soak.

The parser allocation must not be masked with another threshold while corruption remains unexplained.

### Outcome B: Clean heap with parser exhaustion confirmed

If integrity remains good and header insertion fails at a reproducible headroom/concurrency boundary, the hardening plan will provide graceful load shedding before throwing allocations. Candidate requirements are:

- a strict request-header count limit;
- a strict header-line length limit;
- a bound on simultaneous in-progress requests;
- admission refusal before parsing when fixed headroom criteria are not met;
- an allocation-free failure path that closes the TCP connection instead of constructing an HTTP error response under pressure;
- replacement or elimination of the confirmed throwing per-header container operation;
- retention of the independent 8 KiB response gate.

Adjacent `_params` and response-header lists will be inventoried. They will be changed only if evidence or failure-path analysis shows the same condition reaches them.

Any proposed ESPAsyncWebServer fork, version update, or `platformio.ini` dependency change requires a separate user approval.

### Outcome C: No failure under the tested envelope

If neither corruption nor parser exhaustion reproduces:

- leave the diagnostic firmware on the USB board for the overnight soak;
- improve retained breadcrumbs if a blind interval remains;
- report the exact tested workload and boundaries;
- do not claim the incident fixed and do not invent a framework patch.

## 9. Verification requirements for an eventual fix

An eventual fix must satisfy all of the following before it is proposed for production:

1. Desktop unit tests pass.
2. ESP32 firmware builds with the exact intended dependency set.
3. A controlled-allocation regression demonstrates pre-fix failure and post-fix graceful handling.
4. Normal browser requests succeed up to the documented header-count and line-length limits.
5. Requests beyond a limit or below admission headroom close gracefully without panic.
6. Repeated refused requests do not leak heap or request objects.
7. The exact one-user Update + Logs sequence passes.
8. The bounded two-client workload passes.
9. The realistic overnight soak passes.
10. Every reset, if any, is decoded against its byte-matching preserved ELF.
11. Production deployment remains blocked pending explicit approval.

## 10. Adjacent finding

`VersionInfo::size_bytes` lacks a default initializer and appears after reboot as `0xA5A5A5A5` in `/update/check_result`. This is a deterministic initialization defect, not proof of heap corruption.

It will remain separate from the crash diagnostic implementation unless it interferes with test interpretation. Any later fix should initialize both firmware and filesystem sizes to zero and add a fresh-state API test.

## 11. Non-goals and safety properties

- No secret-bearing HTTP data is retained or logged.
- No unbounded concurrency, request size, header count, or test duration is permitted.
- Pressure allocations always have an automatic release path.
- Production is never a test target.
- Diagnostics cannot silently alter normal release behavior because they compile out without the diagnostic defines.
- No success claim is based solely on fewer crashes; the evidence gate determines the root-cause class first.
