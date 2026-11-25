"""
Pre-build patch for ESPAsyncWebServer and AsyncTCP:
1. AsyncTCP.cpp: wrap handle_async_event() in try/catch(...) so no uncaught C++
   exception (e.g. std::bad_alloc under heap pressure) can ever reach
   std::terminate() and reboot the board on the async_tcp task.
2. WebServer.cpp: use new (std::nothrow) + try/catch on connection accept so OOM
   drops only that one connection instead of killing async_tcp.
3. WebRequest.cpp:
   - Filter stored HTTP headers in _parseReqHeader() to only headers actually
     used by ESPAsyncWebServer / ElegantOTA / CoopControllerWebServer, capped
     at 16 headers. Browsers send 15-20 headers per request (User-Agent,
     Sec-CH-UA*, Sec-Fetch-*, Accept-Language, Referer, etc.) that are never
     read; skipping them cuts per-request heap allocations by ~80%.
   - Wrap _onData, _onAck, _onPoll, _onTimeout, _onDisconnect in try/catch(...)
     so any OOM inside ESPAsyncWebServer aborts only that request and returns
     cleanly to AsyncClient (preventing pbuf leaks and async_tcp panics).
   - Keep optional COOP_CRASH_DIAGNOSTICS breadcrumb hooks when enabled.

Reproducibility: patches are applied to the vendored library copy under
.pio/libdeps/<PIOENV>/ (gitignored, regenerated on lib install).
Idempotent — safe to run on every build and upgrades older partial patches.
"""
from pathlib import Path

Import("env")  # type: ignore # noqa: F821 - provided by PlatformIO SCons

ACCEPT_EH_MARKER = "// COOP_PATCHED_ACCEPT_EH"
WEBREQ_EH_MARKER = "// COOP_PATCHED_WEBREQ_EH_AND_HEADER_FILTER"
ASYNCTCP_EH_MARKER = "// COOP_PATCHED_ASYNCTCP_EH"


def get_pioenv() -> str:
    try:
        pioenv = env.get("PIOENV")  # type: ignore # noqa: F821
        if pioenv:
            return str(pioenv)
    except Exception:
        pass
    return "esp32-dev"


def find_lib_file(lib_dir: str, rel_path: str) -> Path | None:
    """Find a file under .pio/libdeps/<PIOENV>/<lib_dir>/<rel_path>."""
    pioenv = get_pioenv()
    direct = Path(".pio/libdeps") / pioenv / lib_dir / rel_path
    if direct.exists():
        return direct
    root = Path(".pio/libdeps") / pioenv
    if root.exists():
        for p in root.rglob(f"{lib_dir}/{rel_path}"):
            return p
    return None


def patch_asynctcp_cpp() -> None:
    """Patch AsyncTCP.cpp: catch any C++ exception escaping handle_async_event."""
    path = find_lib_file("AsyncTCP", "src/AsyncTCP.cpp")
    if path is None:
        print("[patch_async_webserver] AsyncTCP.cpp not found yet; skipping.")
        return

    text = path.read_text()
    if ASYNCTCP_EH_MARKER in text:
        print(f"[patch_async_webserver] already patched: {path}")
        return

    old = (
        "void AsyncTCP_detail::handle_async_event(lwip_tcp_event_packet_t *e) {\n"
        "  if (e->client == NULL) {\n"
        "    // do nothing when arg is NULL\n"
        "    // ets_printf(\"event arg == NULL: 0x%08x\\n\", e->recv.pcb);\n"
        "  } else if (e->event == LWIP_TCP_RECV) {\n"
        "    // ets_printf(\"-R: 0x%08x\\n\", e->recv.pcb);\n"
        "    e->client->_recv(e->recv.pcb, e->recv.pb, e->recv.err);\n"
        "    e->recv.pb = nullptr;  // given to client\n"
        "  } else if (e->event == LWIP_TCP_FIN) {\n"
        "    // ets_printf(\"-F: 0x%08x\\n\", e->fin.pcb);\n"
        "    e->client->_fin(e->fin.pcb, e->fin.err);\n"
        "  } else if (e->event == LWIP_TCP_SENT) {\n"
        "    // ets_printf(\"-S: 0x%08x\\n\", e->sent.pcb);\n"
        "    e->client->_sent(e->sent.pcb, e->sent.len);\n"
        "  } else if (e->event == LWIP_TCP_POLL) {\n"
        "    // ets_printf(\"-P: 0x%08x\\n\", e->poll.pcb);\n"
        "    e->client->_poll(e->poll.pcb);\n"
        "  } else if (e->event == LWIP_TCP_ERROR) {\n"
        "    // ets_printf(\"-E: 0x%08x %d\\n\", e->client, e->error.err);\n"
        "    e->client->_error(e->error.err);\n"
        "  } else if (e->event == LWIP_TCP_CONNECTED) {\n"
        "    // ets_printf(\"C: 0x%08x 0x%08x %d\\n\", e->client, e->connected.pcb, e->connected.err);\n"
        "    e->client->_connected(e->connected.pcb, e->connected.err);\n"
        "  } else if (e->event == LWIP_TCP_ACCEPT) {\n"
        "    // ets_printf(\"A: 0x%08x 0x%08x\\n\", e->client, e->accept.client);\n"
        "    e->accept.server->_accepted(e->client);\n"
        "  } else if (e->event == LWIP_TCP_DNS) {\n"
        "    // ets_printf(\"D: 0x%08x %s = %s\\n\", e->client, e->dns.name, ipaddr_ntoa(&e->dns.addr));\n"
        "    e->client->_dns_found(&e->dns.addr);\n"
        "  }\n"
        "  _free_event(e);\n"
        "}"
    )

    new = (
        "void AsyncTCP_detail::handle_async_event(lwip_tcp_event_packet_t *e) {\n"
        "  " + ASYNCTCP_EH_MARKER + "\n"
        "  try {\n"
        "    if (e->client == NULL) {\n"
        "      // do nothing when arg is NULL\n"
        "    } else if (e->event == LWIP_TCP_RECV) {\n"
        "      e->client->_recv(e->recv.pcb, e->recv.pb, e->recv.err);\n"
        "      e->recv.pb = nullptr;  // given to client\n"
        "    } else if (e->event == LWIP_TCP_FIN) {\n"
        "      e->client->_fin(e->fin.pcb, e->fin.err);\n"
        "    } else if (e->event == LWIP_TCP_SENT) {\n"
        "      e->client->_sent(e->sent.pcb, e->sent.len);\n"
        "    } else if (e->event == LWIP_TCP_POLL) {\n"
        "      e->client->_poll(e->poll.pcb);\n"
        "    } else if (e->event == LWIP_TCP_ERROR) {\n"
        "      e->client->_error(e->error.err);\n"
        "    } else if (e->event == LWIP_TCP_CONNECTED) {\n"
        "      e->client->_connected(e->connected.pcb, e->connected.err);\n"
        "    } else if (e->event == LWIP_TCP_ACCEPT) {\n"
        "      e->accept.server->_accepted(e->client);\n"
        "    } else if (e->event == LWIP_TCP_DNS) {\n"
        "      e->client->_dns_found(&e->dns.addr);\n"
        "    }\n"
        "  } catch (...) {\n"
        "    if (e->client != NULL && e->event != LWIP_TCP_ACCEPT && e->event != LWIP_TCP_ERROR) {\n"
        "      e->client->abort();\n"
        "    }\n"
        "  }\n"
        "  _free_event(e);\n"
        "}"
    )

    if old not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: expected handle_async_event block not found in {path}"
        )
    path.write_text(text.replace(old, new, 1))
    print(f"[patch_async_webserver] patched: {path}")


def patch_webserver_cpp() -> None:
    """Patch WebServer.cpp: nothrow + try/catch on TCP accept."""
    path = find_lib_file("ESPAsyncWebServer", "src/WebServer.cpp")
    if path is None:
        print("[patch_async_webserver] WebServer.cpp not found yet; skipping.")
        return

    text = path.read_text()
    if ACCEPT_EH_MARKER in text:
        print(f"[patch_async_webserver] already patched: {path}")
        return

    new_block = (
        "    [](void *s, AsyncClient *c) {\n"
        "      " + ACCEPT_EH_MARKER + "\n"
        "      if (c == NULL) {\n"
        "        return;\n"
        "      }\n"
        "      c->setRxTimeout(3);\n"
        "      AsyncWebServerRequest *r = nullptr;\n"
        "      try {\n"
        "        r = new (std::nothrow) AsyncWebServerRequest((AsyncWebServer *)s, c);\n"
        "      } catch (...) {\n"
        "        r = nullptr;\n"
        "      }\n"
        "      if (r == NULL) {\n"
        "        c->abort();\n"
        "        delete c;\n"
        "      } else {\n"
        "        #ifdef COOP_CRASH_DIAGNOSTICS\n"
        "        crashBreadcrumbRequestDelta(1, 0, 0);\n"
        "        #endif\n"
        "      }\n"
        "    },"
    )

    # Support upgrading from either stock upstream OR our earlier partial patch
    start_anchor = "    [](void *s, AsyncClient *c) {\n"
    end_anchor = "\n    },\n    this"
    start_idx = text.find(start_anchor)
    end_idx = text.find(end_anchor, start_idx) if start_idx != -1 else -1
    if start_idx == -1 or end_idx == -1:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: expected onClient lambda not found in {path}"
        )

    # Replace up through "    },"
    text = text[:start_idx] + new_block + text[end_idx + len("\n    },"):]
    path.write_text(text)
    print(f"[patch_async_webserver] patched: {path}")


def patch_webrequest_cpp() -> None:
    """Patch WebRequest.cpp: header filtering + try/catch around AsyncClient callbacks."""
    path = find_lib_file("ESPAsyncWebServer", "src/WebRequest.cpp")
    if path is None:
        print("[patch_async_webserver] WebRequest.cpp not found yet; skipping.")
        return

    text = path.read_text()
    if WEBREQ_EH_MARKER in text:
        print(f"[patch_async_webserver] already patched: {path}")
        return

    # 1. Inject header filter helper + optional CrashBreadcrumbs include after literals.h
    inc_anchor = '#include "./literals.h"\n'
    if inc_anchor not in text:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find include anchor in {path}"
        )

    helper_block = (
        inc_anchor
        + WEBREQ_EH_MARKER + "\n"
        + "#ifdef COOP_CRASH_DIAGNOSTICS\n"
        + "#include \"CrashBreadcrumbs.h\"\n"
        + "#endif\n"
        + "static bool coopShouldKeepHeader(const String &name) {\n"
        + "  if (name.length() == 0) return false;\n"
        + "  if (name.equalsIgnoreCase(\"Authorization\") ||\n"
        + "      name.equalsIgnoreCase(\"If-None-Match\") ||\n"
        + "      name.equalsIgnoreCase(\"If-Modified-Since\") ||\n"
        + "      name.equalsIgnoreCase(\"Accept-Encoding\") ||\n"
        + "      name.equalsIgnoreCase(\"Origin\") ||\n"
        + "      name.equalsIgnoreCase(\"Access-Control-Request-Method\") ||\n"
        + "      name.equalsIgnoreCase(\"Access-Control-Request-Headers\") ||\n"
        + "      name.equalsIgnoreCase(\"Range\") ||\n"
        + "      name.equalsIgnoreCase(\"Host\") ||\n"
        + "      name.equalsIgnoreCase(\"Content-Type\") ||\n"
        + "      name.equalsIgnoreCase(\"Content-Length\") ||\n"
        + "      name.equalsIgnoreCase(\"Cookie\")) {\n"
        + "    return true;\n"
        + "  }\n"
        + "  return (name.length() >= 2 && (name[0] == 'X' || name[0] == 'x') && name[1] == '-');\n"
        + "}\n"
    )

    # Remove old diag include block if upgrading from previous patch
    old_diag_inc = (
        '#include "./literals.h"\n'
        '// // COOP_DIAG_BREADCRUMBS\n'
        '#ifdef COOP_CRASH_DIAGNOSTICS\n'
        '#include "CrashBreadcrumbs.h"\n'
        '#endif\n'
    )
    if old_diag_inc in text:
        text = text.replace(old_diag_inc, helper_block, 1)
    else:
        text = text.replace(inc_anchor, helper_block, 1)

    # 2. Wrap AsyncClient callbacks registered in AsyncWebServerRequest constructor in try/catch
    callbacks = [
        (
            "static_cast<AsyncWebServerRequest *>(r)->_onAck(len, time);",
            "try { static_cast<AsyncWebServerRequest *>(r)->_onAck(len, time); } catch (...) { static_cast<AsyncWebServerRequest *>(r)->abort(); }",
        ),
        (
            "static_cast<AsyncWebServerRequest *>(r)->_onDisconnect();",
            "try { static_cast<AsyncWebServerRequest *>(r)->_onDisconnect(); } catch (...) {}",
        ),
        (
            "static_cast<AsyncWebServerRequest *>(r)->_onTimeout(time);",
            "try { static_cast<AsyncWebServerRequest *>(r)->_onTimeout(time); } catch (...) { static_cast<AsyncWebServerRequest *>(r)->abort(); }",
        ),
        (
            "static_cast<AsyncWebServerRequest *>(r)->_onData(buf, len);",
            "try { static_cast<AsyncWebServerRequest *>(r)->_onData(buf, len); } catch (...) { static_cast<AsyncWebServerRequest *>(r)->abort(); }",
        ),
        (
            "static_cast<AsyncWebServerRequest *>(r)->_onPoll();",
            "try { static_cast<AsyncWebServerRequest *>(r)->_onPoll(); } catch (...) { static_cast<AsyncWebServerRequest *>(r)->abort(); }",
        ),
    ]
    for old_cb, new_cb in callbacks:
        if old_cb not in text:
            raise RuntimeError(
                f"[patch_async_webserver] FATAL: callback '{old_cb}' not found in {path}"
            )
        text = text.replace(old_cb, new_cb, 1)

    # 3. Filter and cap _headers.emplace_back() in _parseReqHeader()
    old_emplace_diag = (
        "    #ifdef COOP_CRASH_DIAGNOSTICS\n"
        "    crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_BEFORE, _headers.size(), _temp.length());\n"
        "    #endif\n"
        "    _headers.emplace_back(std::move(header));\n"
        "    #ifdef COOP_CRASH_DIAGNOSTICS\n"
        "    crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_AFTER, _headers.size(), _temp.length());\n"
        "    #endif\n"
    )
    old_emplace_stock = "    _headers.emplace_back(std::move(header));\n"
    new_emplace = (
        "    if (_headers.size() < 16 && coopShouldKeepHeader(name)) {\n"
        "      #ifdef COOP_CRASH_DIAGNOSTICS\n"
        "      crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_BEFORE, _headers.size(), _temp.length());\n"
        "      #endif\n"
        "      _headers.emplace_back(std::move(header));\n"
        "      #ifdef COOP_CRASH_DIAGNOSTICS\n"
        "      crashBreadcrumbRecord(CRUMB_WEB_HEADER_INSERT_AFTER, _headers.size(), _temp.length());\n"
        "      #endif\n"
        "    }\n"
    )
    if old_emplace_diag in text:
        text = text.replace(old_emplace_diag, new_emplace, 1)
    elif old_emplace_stock in text:
        text = text.replace(old_emplace_stock, new_emplace, 1)
    else:
        raise RuntimeError(
            f"[patch_async_webserver] FATAL: could not find _headers.emplace_back in {path}"
        )

    # 4. Ensure diagnostic hooks exist on stock copies if not already present
    if "CRUMB_WEB_HEADER_PARSE" not in text:
        parse_old = "bool AsyncWebServerRequest::_parseReqHeader() {\n"
        parse_new = (
            parse_old
            + "  #ifdef COOP_CRASH_DIAGNOSTICS\n"
            + "  crashBreadcrumbRecord(CRUMB_WEB_HEADER_PARSE, _headers.size(), _temp.length());\n"
            + "  #endif\n"
        )
        if parse_old in text:
            text = text.replace(parse_old, parse_new, 1)

    if "crashBreadcrumbRequestDelta(-1" not in text:
        dtor_old = "AsyncWebServerRequest::~AsyncWebServerRequest() {\n"
        dtor_new = (
            dtor_old
            + "  #ifdef COOP_CRASH_DIAGNOSTICS\n"
            + "  crashBreadcrumbRequestDelta(-1, 0, 0);\n"
            + "  #endif\n"
        )
        if dtor_old in text:
            text = text.replace(dtor_old, dtor_new, 1)

    if "CRUMB_WEB_DATA" not in text:
        ondata_old = "void AsyncWebServerRequest::_onData(void *buf, size_t len) {\n"
        ondata_new = (
            ondata_old
            + "  #ifdef COOP_CRASH_DIAGNOSTICS\n"
            + "  crashBreadcrumbRecord(CRUMB_WEB_DATA, (uint32_t)len, _parseState);\n"
            + "  #endif\n"
        )
        if ondata_old in text:
            text = text.replace(ondata_old, ondata_new, 1)

    path.write_text(text)
    print(f"[patch_async_webserver] patched: {path}")


patch_asynctcp_cpp()
patch_webserver_cpp()
patch_webrequest_cpp()
