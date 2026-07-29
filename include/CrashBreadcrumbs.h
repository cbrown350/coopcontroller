#ifndef __CRASH_BREADCRUMBS_H__
#define __CRASH_BREADCRUMBS_H__

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Crash Breadcrumbs — retained pre-crash diagnostic ring
//
// A fixed-size ring of numeric records in RTC no-init memory that survives
// panic/software resets. Records allocation and lifecycle boundaries around
// the ESPAsyncWebServer request-header parser, update/TLS paths, and a
// test-only heap-pressure controller.
//
// Design constraints:
// - NEVER allocates on the heap (no String, no malloc, no new).
// - NEVER stores header names/values, URLs, bodies, credentials, or tokens.
// - Numeric metadata only: event id, args, heap snapshot, core/task hints.
// - Compiles to inline no-ops unless COOP_CRASH_DIAGNOSTICS=1 is defined at
//   build time. When disabled, no .cpp linkage is required (needed for the
//   desktop unit-test environment which excludes src/ via build_src_filter).
//
// See docs/superpowers/specs/2026-07-29-async-request-header-crash-diagnostics-design.md
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Event identifiers recorded in the breadcrumb ring.
typedef enum {
    CRUMB_NONE = 0,
    CRUMB_BOOT = 1,

    // ESPAsyncWebServer request lifecycle
    CRUMB_WEB_ACCEPT = 10,
    CRUMB_WEB_REQUEST_ALLOCATED = 11,
    CRUMB_WEB_DATA = 12,
    CRUMB_WEB_HEADER_PARSE = 13,
    CRUMB_WEB_HEADER_INSERT_BEFORE = 14,
    CRUMB_WEB_HEADER_INSERT_AFTER = 15,
    CRUMB_WEB_HEADERS_COMPLETE = 16,
    CRUMB_WEB_REQUEST_DESTROYED = 17,

    // Update / TLS boundaries
    CRUMB_UPDATE_REQUESTED = 30,
    CRUMB_UPDATE_START = 31,
    CRUMB_UPDATE_HTTP_BEFORE = 32,
    CRUMB_UPDATE_HTTP_AFTER = 33,
    CRUMB_UPDATE_JSON_BEFORE = 34,
    CRUMB_UPDATE_JSON_AFTER = 35,
    CRUMB_UPDATE_END = 36,
    CRUMB_TLS_ALLOC_BEFORE = 40,
    CRUMB_TLS_ALLOC_SUCCESS = 41,
    CRUMB_TLS_ALLOC_REFUSED = 42,
    CRUMB_TLS_ALLOC_EXCEPTION = 43,

    // Heap-pressure controller
    CRUMB_PRESSURE_REQUESTED = 50,
    CRUMB_PRESSURE_ACTIVE = 51,
    CRUMB_PRESSURE_RELEASED = 52,

} CrashBreadcrumbEvent;

// Heap-pressure controller status.
typedef struct {
    bool active;
    uint32_t target_max_block;
    uint32_t deadline_ms;
    uint32_t allocated_blocks;
    uint32_t free_heap;
    uint32_t max_block;
} CrashPressureStatus;

// ============================================================================
// Public API
// ============================================================================

// When COOP_CRASH_DIAGNOSTICS is NOT defined, provide inline no-op stubs so
// desktop tests and release firmware need no separate .cpp linkage.
#if !defined(COOP_CRASH_DIAGNOSTICS)

static inline void crashBreadcrumbRecord(CrashBreadcrumbEvent event, uint32_t arg0, uint32_t arg1) {
    (void)event; (void)arg0; (void)arg1;
}

static inline void crashBreadcrumbRequestDelta(int delta, uint32_t arg0, uint32_t arg1) {
    (void)delta; (void)arg0; (void)arg1;
}

static inline void crashBreadcrumbReport(void (*logFunc)(const char *msg)) {
    (void)logFunc;
}

static inline void crashBreadcrumbPressureRequest(uint32_t targetMaxBlock, uint32_t durationMs) {
    (void)targetMaxBlock; (void)durationMs;
}

static inline void crashBreadcrumbPressureUpdate(void) {}

static inline CrashPressureStatus crashBreadcrumbPressureStatus(void) {
    CrashPressureStatus s = {false, 0, 0, 0, 0, 0};
    return s;
}

#else
// COOP_CRASH_DIAGNOSTICS defined — active instrumentation declared here,
// implemented in src/CrashBreadcrumbs.cpp (ESP32 only).

void crashBreadcrumbRecord(CrashBreadcrumbEvent event, uint32_t arg0, uint32_t arg1);
void crashBreadcrumbRequestDelta(int delta, uint32_t arg0, uint32_t arg1);
void crashBreadcrumbReport(void (*logFunc)(const char *msg));
void crashBreadcrumbPressureRequest(uint32_t targetMaxBlock, uint32_t durationMs);
void crashBreadcrumbPressureUpdate(void);
CrashPressureStatus crashBreadcrumbPressureStatus(void);

#endif // COOP_CRASH_DIAGNOSTICS

#ifdef __cplusplus
}
#endif

#endif // __CRASH_BREADCRUMBS_H__
