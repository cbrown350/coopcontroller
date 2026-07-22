# Framework Migration — Resume Note (fix/framework-migration-v2)

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
- `crash_watch.py <ip> <dur>` — reboot/wedge tally with reset reasons.
- `serial_cap.py <port> <baud> <log>` — reconnecting serial capture (use
  `~/.platformio/penv/bin/python`, it has pyserial 3.5).
- Decode coredumps: `xtensa-esp32-elf-addr2line -pfiaC -e
  .pio/build/esp32-dev/firmware.elf <frames>`.
