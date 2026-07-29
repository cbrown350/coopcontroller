"""
Pre-build patch for ESPAsyncWebServer's TCP-accept allocation and
diagnostic breadcrumb hooks in request-header parsing.

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

Diagnostic hooks (2026-07-29): when COOP_CRASH_DIAGNOSTICS=1 is defined at
build time, inject breadcrumb calls around _headers.emplace_back() in
WebRequest.cpp to capture the exact heap state at the allocation that panics
in production. The hooks compile to nothing in release builds.

Reproducibility: this patch is applied to the vendored library copy in the
PIO libdeps directory (gitignored, regenerated from lib_deps on install).
Idempotent — skips when markers are already present.
"""
from pathlib import Path

Import("env")  # type: ignore # noqa: F821 - provided by PlatformIO SCons

MARKER = "// COOP_PATCHED_NOthrow_ACCEPT"
DIAG_MARKER = "// COOP_DIAG_BREADCRUMBS"


def find_lib_file(name: str) -> Path | None:
    """Find a file under .pio/libdeps/esp32-dev/ESPAsyncWebServer/src/."""
    candidates = [
        Path(f".pio/libdeps/esp32-dev/ESPAsyncWebServer/src/{name}"),
        Path(".pio/libdeps") / "esp32-dev" / "ESPAsyncWebServer" / "src" / name,
    ]
    for c in candidates:
        if c.exists():
            return c
    root = Path(".pio/libdeps")
    if root.exists():
        for p in root.rglob(f"ESPAsyncWebServer/src/{name}"):
            return p
    return None


def patch_webserver_cpp() -> None:
    """Patch WebServer.cpp: nothrow accept + diagnostic request-count increment."""
    path = find_lib_file("WebServer.cpp")
    if path is None:
        print("[patch_async_webserver] WebServer.cpp not found yet; skipping (will retry next build).")
        return

    text = path.read_text()
    if MARKER in text:
        print(f"[patch_async_webserver] already patched: {path}")
        return

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
        '      } else {\n'
        '        #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '        crashBreadcrumbRequestDelta(1, 0, 0);\n'
        '        #endif\n'
        '      }'
    )
    if old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: expected accept block not found in {path}; "
            "library may have changed upstream."
        )
    path.write_text(text.replace(old, new))
    print(f"[patch_async_webserver] patched: {path}")


def patch_webrequest_cpp() -> None:
    """Patch WebRequest.cpp: diagnostic breadcrumb hooks around header insertion."""
    path = find_lib_file("WebRequest.cpp")
    if path is None:
        print("[patch_async_webserver] WebRequest.cpp not found yet; skipping (will retry next build).")
        return

    text = path.read_text()
    if DIAG_MARKER in text:
        print(f"[patch_async_webserver] WebRequest.cpp already diag-patched: {path}")
        return

    # 1. Add include after the last library include
    inc_old = '#include "./literals.h"\n'
    inc_new = (
        inc_old
        + '// ' + DIAG_MARKER + '\n'
        + '#ifdef COOP_CRASH_DIAGNOSTICS\n'
        + '#include "CrashBreadcrumbs.h"\n'
        + '#endif\n'
    )
    if inc_old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find include anchor in {path}"
        )
    text = text.replace(inc_old, inc_new, 1)

    # 2. Wrap _headers.emplace_back with before/after breadcrumb hooks
    emplace_old = '    _headers.emplace_back(std::move(header));\n'
    emplace_new = (
        '    #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '    crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_BEFORE, _headers.size(), _temp.length());\n'
        '    #endif\n'
        '    _headers.emplace_back(std::move(header));\n'
        '    #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '    crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_AFTER, _headers.size(), _temp.length());\n'
        '    #endif\n'
    )
    if emplace_old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find _headers.emplace_back in {path}"
        )
    text = text.replace(emplace_old, emplace_new, 1)

    # 3. Add breadcrumb at _parseReqHeader entry
    parse_old = 'bool AsyncWebServerRequest::_parseReqHeader() {\n'
    parse_new = (
        parse_old
        + '  #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '  crashBreadcrumbRecord(CRUMB_WEB_HEADER_PARSE, _headers.size(), _temp.length());\n'
        '  #endif\n'
    )
    if parse_old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find _parseReqHeader in {path}"
        )
    text = text.replace(parse_old, parse_new, 1)

    # 4. Add breadcrumb in destructor (decrement active request count)
    dtor_old = 'AsyncWebServerRequest::~AsyncWebServerRequest() {\n'
    dtor_new = (
        dtor_old
        + '  #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '  crashBreadcrumbRequestDelta(-1, 0, 0);\n'
        '  #endif\n'
    )
    if dtor_old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find destructor in {path}"
        )
    text = text.replace(dtor_old, dtor_new, 1)

    # 5. Add breadcrumb at _onData entry
    #    The _onData method signature starts with:
    #    void AsyncWebServerRequest::_onData(void *buf, size_t len) {
    ondata_old = 'void AsyncWebServerRequest::_onData(void *buf, size_t len) {\n'
    ondata_new = (
        ondata_old
        + '  #ifdef COOP_CRASH_DIAGNOSTICS\n'
        '  crashBreadcrumbRecord(CRUMB_WEB_DATA, (uint32_t)len, _parseState);\n'
        '  #endif\n'
    )
    if ondata_old not in text:
        # Non-fatal: the signature might differ slightly; skip this hook.
        print(f"[patch_async_webserver] WARNING: _onData signature not matched in {path}; skipping data-entry hook.")
    else:
        text = text.replace(ondata_old, ondata_new, 1)

    path.write_text(text)
    print(f"[patch_async_webserver] diag-patched: {path}")


patch_webserver_cpp()
patch_webrequest_cpp()
