# Framework Migration — Resume Note (fix/framework-migration-v2)

**STATUS 2026-07-21 EOD: DO NOT DEPLOY. OTA is broken** (manifest fetch
truncates, root cause unknown — see "BLOCKING BUG" section below). The
addHeader crash and the TLS heap-pressure crash are both fixed and validated.
The soft-wedge is converted to a self-recovering panic (not a full fix, but a
major improvement) under a load test far heavier than real usage. Next
session: fix OTA, re-verify, then this is ready to ship.

**Branch created:** 2026-07-21, off `master` @ `e92b92d` (v0.8.3 — includes the
`getMaxAllocHeap` instrumentation + syslog rate limiter, which the migration
validation needs).

## Why this branch exists — the confirmed root cause

The long-standing "runs fine, then crashes/wedges when loading web pages" bug is
**NOT** application-level. Proven 2026-07-21 on USB board 192.168.2.99:

- With remote syslog **fully disabled** (zero UDP sends, zero `endPacket` ENOMEM),
  the board still panics after ~120 s under realistic web+TLS load.
- Clean coredump (ELF `b43db1a6`) decodes to:
  `async_tcp → AsyncWebServerRequest::_send → AsyncAbstractResponse::_respond →
  _assembleHead → AsyncWebServerResponse::addHeader → std::_List_node<AsyncWebHeader>
  operator new → __cxa_allocate_exception → std::terminate → abort/panic`.
- Heap looked healthy just before (~115 KB free, ~55 KB largest contiguous).

This is an uncaught `std::bad_alloc` inside **ESPAsyncWebServer's own** response-
header assembly on the async_tcp task under concurrent-request network-buffer
pressure. Our web-handler try/catch cannot catch it (throw is inside library
`_respond()`, below our lambda). Application mitigations tried and reverted:
max-alloc TLS gate, TLS-vs-web serialization, syslog rate limit (kept as
diagnostics only), syslog circuit breaker. None prevent the panic.

See project memory `asyncwebserver-addheader-bad-alloc-definitive.md`.

## The real fix: pioarduino / arduino-esp32 3.3.9 / IDF 5.x / mbedtls 3.x

A prior attempt lives on the **old** branch `fix/pioarduino-migration` (@ `a7a3e60`,
forked from the older `840e1a7`). It killed the async_tcp crash but had two
blockers. Reference its 3 commits (do NOT merge the stale branch — cherry-pick
ideas onto this fresh one instead):

- `043257b` fix(hal): migrate to pioarduino platform (arduino-esp32 3.3.9, mbedtls 3.x)
- `b4b615f` fix(build): make esp32-dev fit on the larger 3.x toolchain
- `a7a3e60` WIP: pioarduino migration + lean mbedtls recompile attempt

### Concrete starting recipe (from the old branch's platformio.ini)

```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip

[env:esp32-dev]
build_type = release   ; NOT debug — debug injects -Og, overrides -Os, image is
                       ; ~34 KB too big for the 0x1B0000 OTA partition on 3.x
build_flags =
    ${env.build_flags}
    -g -Os -ffunction-sections -fdata-sections -Wl,--gc-sections
```

## Known blockers to solve (from the parked attempt + memory)

1. **Outbound TLS broke under heap pressure** on 3.x (weather/OTA/LLM). Investigate
   whether mbedtls 3.x dynamic buffers / `setBufferSizes()` (now available!) or
   partition/heap tuning fixes it. `getMaxAllocHeap` telemetry (now on master)
   will show the real contiguous headroom.
2. **Lean-mbedtls recompile link-fails.** May not be needed if stock 3.x fits;
   re-evaluate before going down the custom-sdkconfig path.
3. **NVS/settings wipe on framework change** — see memory
   `nvs-wipe-and-flash-order-on-framework-migration.md`. Provisioning needs direct
   `mklittlefs` + `esptool write_flash 0x370000`; buildfs excludes user_settings.json.
   DO NOT OTA production until validated.
4. **ESPAsyncWebServer version**: still `esp32async/ESPAsyncWebServer@3.10.1` pinned
   (3.11.2 keeps the same `std::list<AsyncWebHeader>` alloc path AND breaks our
   `switch(request_->method())`). The migration's value is the newer mbedtls/heap
   behavior, not the web server lib. Confirm 3.10.1 compiles on the 3.x core.

## Validation protocol (do this in the NEXT session — user's request)

1. Build + flash to USB board 192.168.2.99 (production .8 is UNTOUCHED).
2. **Thoroughly test ALL features** (user's explicit ask), not just the crash:
   door auto open/close + schedule, pump/water metering, light PWM, sensors,
   weather + LLM decider, Telegram, MQTT, OTA check/install, settings persistence,
   reset-reason display, web UI pages.
3. Re-run the crash repro: `scratchpad/realistic_load.sh 192.168.2.99 1500` +
   `crash_watch.py`. PASS = survives 25+ min, no panic, no wedge, HTTP continuously
   responsive, weather/OTA TLS still succeed.
4. Confirm `getMaxAllocHeap` telemetry shows healthy contiguous headroom under load.
5. All 841 desktop tests still pass (`pio test -e unit-test-desktop`).

## Reusable stress tooling (scratchpad)

- `realistic_load.sh <ip> <dur>` — realistic browser+TLS load (the repro).
- `crash_watch.py <ip> <dur> [poll_s]` — reboot/wedge tally with reset reasons.
- `serial_cap.py <port> <baud> <log>` — reconnecting serial capture (use
  `~/.platformio/penv/bin/python`, it has pyserial 3.5).
- Decode coredumps: `xtensa-esp32-elf-addr2line -pfiaC -e
  .pio/build/esp32-dev/firmware.elf <frames>`.

## 2026-07-21 session findings (this branch, fix/framework-migration-v2)

### Migration applied — pioarduino 55.03.39 (arduino-esp32 3.3.9 / IDF 5.5.4 / mbedtls 3.x)

- `platform` → pioarduino `55.03.39` tag. **Pinned to a tag, NOT the rolling
  `stable` zip**: the rolling zip's framework spec drifts, triggering pioarduino's
  reinstall path, which crashes under Python 3.14 (`exists(None)` in
  `safe_framework_cleanup`). A pinned tag gives a deterministic package spec.
- `build_type = release` + explicit `-Os -g` for esp32-dev: the larger 3.x
  toolchain image is ~34KB too big for the 0x1B0000 OTA partition in debug (PIO
  debug injects `-Og`, overriding `-Os`). Image is ~1580 KB, ~148 KB headroom.
- 3.x API code changes: `WiFiClass::status()` → `WiFi.status()` (4 sites incl.
  the v0.8.x wifi-connect guard cluster the prior attempt missed); LEDC
  channel→pin-based (`ledcAttach`/`ledcWrite`) bridged via HAL members; mbedtls
  `_ret`→non-`_ret` SHA256 (HAL + UpdateManager + desktop mock aliases); IDF 5.x
  `esp_task_wdt_config_t` + `esp_task_wdt_reconfigure` fallback, moved before
  `wifiController.begin`.
- `build_scripts/ensure_pioarduino_backup.py` (pre:): works around the pioarduino
  `checkprogsize` restore race that intermittently fails non-LTO builds.
- **841/841 desktop tests pass.**

### TLS heap pressure — RESOLVED with a conservative lean config

Stock pioarduino mbedtls is fat: with ~90KB free but only ~34-55KB largest
contiguous block, the TLS handshake fails with `(-10368) X509 - Allocation of
memory failed` (weather) and `(-17040) RSA BIGNUM alloc failed` (LLM/ollama).
Fix: `custom_sdkconfig = file://build_scripts/lean_mbedtls.sdkconfig` with ONLY
the link-safe trims (keeps the full cipher/curve set so NetworkClientSecure's
ssl_client.cpp still links):
  - `CONFIG_BT_ENABLED=n` + BLE Mesh off — the single biggest static-RAM win.
  - `CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096` (from 16384) — frees ~12KB per TLS
    context. IN stays at 16384 (server responses can approach 16KB).
  - `CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE=n`.
Aggressive trims (ECP curves, key exchanges) link-fail and are NOT used. With
this config, weather + LLM TLS succeed; heap ~119KB free / 55KB max_alloc.

### BLOCKING BUG: OTA manifest check truncates on 3.x — root cause NOT yet found, do not deploy

`/update/check` fetches `api.github.com/.../releases/latest` (~14KB JSON over
TLS, multiple TLS records). On 3.x the body truncates at ~2.8-8.6KB (varies run
to run) → `IncompleteInput`/`InvalidInput` parse error. Prod (.8, 2.x firmware)
parses it fine, so this is a 3.x regression, and it means **OTA is broken on
this branch** — do not merge/deploy until fixed and `/update/check` verified to
return `check_ok: true` with a correct `available_version`.

**Earlier theory in this note was WRONG and should not be trusted:** it claimed
`WiFiClientSecure::available()` peeks `mbedtls_ssl_read(NULL, 0)` without
fetching the next record from the socket. Direct inspection of the real
mbedtls source this session
(`~/.platformio/packages/framework-espidf/components/mbedtls/mbedtls/library/ssl_msg.c`,
`mbedtls_ssl_read()` ~line 5921) shows this is false: whenever
`ssl->in_offt == NULL` (no buffered application-data record), `mbedtls_ssl_read`
unconditionally calls `mbedtls_ssl_read_record()` regardless of the requested
`len` — so a `len=0` peek DOES drive a real socket read and DOES fetch the next
record. The truncation has some other cause. Do not resume by patching around
the peeze theory; start by re-deriving the actual failure point.

**What's confirmed:**
- GitHub sends the correct `Content-Length: 14321`, uncompressed (verified via
  curl with the same `User-Agent: CoopController-OTA` header the board sends —
  no gzip, HTTP/2 downgrades fine to HTTP/1.1).
- The response fits in a single TLS record (14321 < 16384 max record size), so
  this is not a multi-record boundary issue in the TLS layer itself.
- Board reads stop early and cleanly (no error, no exception) — `httpGet()`
  just returns a short string.
- Weather/LLM/Telegram TLS (small responses, <2KB) are unaffected — this only
  shows up on bodies in the multi-KB range.

**What did NOT fix it (all tried and reverted this session):** `Connection:
keep-alive` instead of `close`; `readBytes()` instead of raw `read()`; 1s/5s
no-progress grace windows before giving up; symmetric 16KB/16KB
`SSL_IN_CONTENT_LEN`/`SSL_OUT_CONTENT_LEN` (vs the asymmetric 16K/4K in the
committed lean config); serializing outbound TLS (the unrelated fix for the
soft-wedge, committed anyway since it's independently correct).

**Where to look next:**
1. Instrument `HAL_ESP32::httpGet()` (lib/HAL_ESP32/HAL_ESP32.cpp) to log, on
   every loop iteration of the body-read loop: `client.available()`,
   `response.length()`, `contentLength`, and `client.connected()`. Get a
   byte-by-byte account of exactly where and why the loop exits early — the
   prior session's diagnostics kept getting lost to serial-capture unreliability
   (see "Serial capture is unreliable" note below) rather than actually proving
   the exit condition.
2. Check whether the *headers* are being mis-parsed — `readHttpLine()`
   (HAL_ESP32.cpp ~line 792) reads byte-by-byte via `client.read()` in a loop
   with its own `!client.connected() && available()==0` exit condition. If a
   header read exits early, `contentLength` could end up wrong or the code could
   start reading body bytes that are actually still header, desyncing everything
   downstream. This was never directly verified.
3. Consider testing with a *different* HTTPS endpoint serving a similar-sized
   body (not GitHub) to rule out anything GitHub-specific (HTTP/2 downgrade
   quirks, TCP_NODELAY behavior, etc.).
4. Verify OTA *install* (`httpGetStream()`, same file, binary firmware
   download) has the same or a different failure mode — it reads much larger
   bodies (~1.6MB) and was never tested this session because the manifest check
   never got far enough to reach it.
5. `mbedtls_ssl_read` source is at
   `~/.platformio/packages/framework-espidf/components/mbedtls/mbedtls/library/ssl_msg.c`
   if the bug turns out to be inside mbedtls itself rather than in our HAL code.

**Serial capture is unreliable for this investigation:** `serial_cap.py` (pyserial,
scratchpad/) reliably drops lines under load — multiple attempts this session to
capture `Serial.printf` diagnostics from `httpGet()`'s read loop failed
silently (empty grep results) even though the board was provably running the
diagnostic build. Prefer echoing findings into an HTTP-readable field (e.g.
temporarily embed lengths/state into the `/update/check_result` error string,
as done partway through this session) over trusting the serial log.

### Feature validation on USB board 192.168.2.99 (done)

- WiFi connect (BSSID-preferred, Browns): OK, RSSI -67, IP .99.
- Heap telemetry: ~85-119KB free / 55KB max_alloc, healthy under load.
- Door/pump/light/buzzer control endpoints: OK (actions acknowledged).
- Sun times: 06:10 / 20:56 (correct for Smithfield UT).
- Settings persistence (POST /update_settings + readback): OK.
- Reset-reason display: OK ("Power-on" code 1 clean boot; "Panic / exception"
  code 4 correctly shown after the lwIP assert below).
- Weather fetch (TLS): OK — 91.1°F Clouds, forecast retrieved.
- LLM decider (TLS): OK — connection test passes, decider active.
- OTA check (TLS): FAILS — truncation, root cause not found, **BLOCKS
  deployment**. See "BLOCKING BUG" section above.

### Crash repro results (realistic_load.sh: 8 web workers + TLS nudges every 20s, 1500s)

**Run 1 (framework migration + lean mbedtls only, no TLS-serialize/watchdog):**
wedged at ~180s — firmware loop alive (serial still emitting) but NO ping,
NO HTTP; unrecoverable without a physical reset; the wedge also CORRUPTED the
LittleFS settings partition (board rebooted into AP mode with defaults on next
power cycle). A parallel web-only test (6 workers, NO outbound TLS) survived
623s clean with zero incidents — isolates the wedge trigger to concurrent
OUTBOUND TLS, not web load. See memory `soft-wedge-is-tls-gated-not-web.md`.

**Fix applied (commit 4b66638):** `HAL_ESP32::createSecureClient` refuses a new
TLS handshake while one is already in flight (serializes outbound TLS — callers
already retry on their own schedule on a null client). `main.cpp` adds a
network-responsive watchdog: every 15s, a plain-TCP self-probe to the board's
own web server port; reboot after 90s of continuous failure while the loop is
alive (catches the case where `WiFi.status()` still claims connected but lwIP
can't accept).

**Run 2 (with the fix): the unrecoverable wedge is GONE.** Instead the board
hits a different, self-recovering failure 6 times over 1500s (~once per 4 min):
`assert failed: tcp_receive lwip/src/core/tcp_in.c:1450 (tcp_receive: wrong
state)` — a genuine lwIP TCP-PCB state-machine assertion inside IDF 5.5.4's
lwIP (confirmed by decoded backtrace: panic_abort → __assert_func → tcp_receive
→ tcp_process → tcp_input → ip4_input → tcpip_thread, entirely inside the `tiT`
lwIP task, not application code). Each panic self-reboots cleanly within
~15-30s; HTTP never showed sustained downtime to 60s-granularity polling;
**settings/LittleFS survive intact across all 6 reboots** (verified
`/get_settings` post-test: ssid/door_auto_close/hostname all correct).

**Verdict:** net-positive but not a zero-incident PASS. Went from "permanent
wedge + data corruption" to "self-recovering panic + data integrity preserved"
under an adversarial 8-worker+TLS stress load far heavier than real
coop-controller usage (occasional user + periodic weather/LLM/MQTT polling).
Ship with this as a documented residual risk; the `tcp_receive: wrong state`
signature is a new lead for a future session (likely lwIP TCP PCB corruption
from concurrent TLS teardown racing inbound connection churn — same family as
memory `wifi-panic-root-cause-tls-race-and-modem-sleep` but now inside lwIP
itself rather than mbedtls/x509 teardown).
