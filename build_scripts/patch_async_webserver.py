"""
Pre-build patch for ESPAsyncWebServer's TCP-accept allocation.

Root cause of the remaining async_tcp panic (decoded 2026-07-23, USB board,
ELF 9ee66c16): the server's onClient callback allocates the per-connection
AsyncWebServerRequest with a THROWING `new` (WebServer.cpp:50). Under heap
fragmentation that allocation throws std::bad_alloc, and because async_tcp has
no catch handler, __cxa_throw -> std::terminate -> abort reboots the board.
This site runs inside the AsyncTCP library below any application handler, so
no app-level heap gate can reach it (confirmed: three app-level gates at three
different sites each failed to stop this one).

Fix: replace the throwing new with new(std::nothrow) and explicitly close the
just-accepted socket when the allocation returns null. The existing code
already has a null-check branch (abort + delete c), but under -fexceptions it
is dead code because the throw never returns nullptr. nothrow makes that branch
live, so an OOM on connection accept drops that one request instead of killing
the whole async_tcp task.

Reproducibility: this patch is applied to the vendored library copy in the
PIO libdeps directory (gitignored, regenerated from lib_deps on install).
ApplyPatch is idempotent — it skips when the marker is already present, so
re-runs (and library reinstalls after the first patch) are safe. The marker
comment lets anyone verify the running firmware includes the fix.
"""
from pathlib import Path

Import("env")  # type: ignore # noqa: F821 - provided by PlatformIO SCons

MARKER = "// COOP_PATCHED_NOthrow_ACCEPT"


def find_webserver_cpp() -> Path | None:
    candidates = [
        Path(".pio/libdeps/esp32-dev/ESPAsyncWebServer/src/WebServer.cpp"),
        Path(".pio/libdeps") / "esp32-dev" / "ESPAsyncWebServer" / "src" / "WebServer.cpp",
    ]
    for c in candidates:
        if c.exists():
            return c
    # Fall back to a directory walk under .pio/libdeps.
    root = Path(".pio/libdeps")
    if root.exists():
        for p in root.rglob("ESPAsyncWebServer/src/WebServer.cpp"):
            return p
    return None


path = find_webserver_cpp()
if path is None:
    # Library not installed yet (first build before lib_deps resolved). The
    # patch will apply on the next build after PIO populates libdeps.
    print("[patch_async_webserver] WebServer.cpp not found yet; skipping (will retry next build).")
else:
    text = path.read_text()
    if MARKER in text:
        print(f"[patch_async_webserver] already patched: {path}")
    else:
        old = (
            '      AsyncWebServerRequest *r = new AsyncWebServerRequest((AsyncWebServer *)s, c);\n'
            '      if (r == NULL) {\n'
            '        c->abort();\n'
            '        delete c;\n'
            '      }'
        )
        new = (
            '      // ' + MARKER + ': use nothrow so an OOM here drops this one\n'
            '      // connection instead of throwing std::bad_alloc and aborting the\n'
            '      // async_tcp task (which reboots the device). The null branch below\n'
            '      // then closes the just-accepted socket.\n'
            '      AsyncWebServerRequest *r = new (std::nothrow) AsyncWebServerRequest((AsyncWebServer *)s, c);\n'
            '      if (r == NULL) {\n'
            '        c->abort();\n'
            '        delete c;\n'
            '      }'
        )
        if old not in text:
            print(f"[patch_async_webserver] WARNING: expected source block not found in {path}; library may have changed upstream. Review needed.")
        else:
            path.write_text(text.replace(old, new))
            print(f"[patch_async_webserver] patched: {path}")
