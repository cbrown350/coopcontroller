#include "CrashBreadcrumbs.h"

// ============================================================================
// Active ESP32 instrumentation — only compiled when COOP_CRASH_DIAGNOSTICS=1.
// When not defined, inline no-op stubs in the header handle everything, and
// this .cpp compiles to nothing.
// ============================================================================

#if defined(COOP_CRASH_DIAGNOSTICS) && defined(ESP_PLATFORM)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ============================================================================
// Constants
// ============================================================================

static constexpr uint32_t CB_MAGIC = 0xCB4EAD01;
static constexpr uint32_t CB_VERSION = 1;
static constexpr int CB_RING_SIZE = 64;
static constexpr int CB_MAX_PRESSURE_BLOCKS = 96;
static constexpr uint32_t CB_PRESSURE_CHUNK = 4096;
static constexpr uint32_t CB_PRESSURE_MIN_TARGET = 8192;
static constexpr uint32_t CB_PRESSURE_MAX_TARGET = 65536;
static constexpr uint32_t CB_PRESSURE_MIN_DURATION = 1000;
static constexpr uint32_t CB_PRESSURE_MAX_DURATION = 60000;
static constexpr uint32_t CB_PRESSURE_MIN_FREE_FLOOR = 50000;

// ============================================================================
// Retained ring in RTC no-init memory (survives panic/software reset)
// ============================================================================

struct __attribute__((packed)) BreadcrumbRecord {
    uint32_t seq;          // commit sequence (0 = uncommitted/incomplete)
    uint32_t event;        // CrashBreadcrumbEvent
    uint32_t arg0;
    uint32_t arg1;
    uint32_t free_heap;    // ESP.getFreeHeap() at record time
    uint32_t max_block;    // heap_caps_get_largest_free_block(DEFAULT|INTERNAL)
    uint32_t min_heap;     // ESP.getMinFreeHeap()
    int32_t  core;         // xPortGetCoreID()
    int32_t  stk_hwm;      // uxTaskGetStackHighWaterMark(NULL) in words
    uint32_t time_ms;      // esp_timer_get_time() / 1000
    uint32_t active_reqs;  // g_active_request_count
    uint8_t  heap_ok;      // 0=fail 1=ok 255=not checked
};

struct __attribute__((packed)) BreadcrumbRingHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t next_seq;      // monotonic allocator (never wraps to 0)
    int32_t  write_index;   // ring write position
    uint32_t active_requests;
};

// Place in RTC no-init region so it survives all resets except power-loss.
RTC_NOINIT_ATTR static BreadcrumbRingHeader cb_header;
RTC_NOINIT_ATTR static BreadcrumbRecord cb_ring[CB_RING_SIZE];

// Active inbound request count (volatile — written from async_tcp, read from loop)
static volatile int32_t g_active_request_count = 0;

// ============================================================================
// Heap-pressure controller state (main-loop only)
// ============================================================================

static uint8_t* g_pressure_blocks[CB_MAX_PRESSURE_BLOCKS] = {};
static int g_pressure_count = 0;
static bool g_pressure_active = false;
static uint32_t g_pressure_target = 0;
static uint32_t g_pressure_deadline = 0;

// ============================================================================
// Internal helpers
// ============================================================================

static uint32_t cb_get_max_block() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

static void cb_validate_ring() {
    if (cb_header.magic != CB_MAGIC || cb_header.version != CB_VERSION) {
        cb_header.magic = CB_MAGIC;
        cb_header.version = CB_VERSION;
        cb_header.next_seq = 1;
        cb_header.write_index = 0;
        cb_header.active_requests = 0;
        for (int i = 0; i < CB_RING_SIZE; i++) {
            cb_ring[i].seq = 0;
        }
    }
}

// ============================================================================
// Public API implementation
// ============================================================================

extern "C" void crashBreadcrumbRecord(CrashBreadcrumbEvent event, uint32_t arg0, uint32_t arg1) {
    cb_validate_ring();

    uint32_t idx = cb_header.write_index;
    BreadcrumbRecord &r = cb_ring[idx];

    r.seq = 0; // mark incomplete
    r.event = (uint32_t)event;
    r.arg0 = arg0;
    r.arg1 = arg1;
    r.free_heap = ESP.getFreeHeap();
    r.max_block = cb_get_max_block();
    r.min_heap = ESP.getMinFreeHeap();
    r.core = xPortGetCoreID();
    r.stk_hwm = (int32_t)uxTaskGetStackHighWaterMark(nullptr);
    r.time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    r.active_reqs = (uint32_t)g_active_request_count;

#if defined(COOP_HEAP_INTEGRITY_DIAGNOSTICS)
    r.heap_ok = heap_caps_check_integrity(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL, false) ? 1 : 0;
#else
    r.heap_ok = 255; // not checked
#endif

    r.seq = __atomic_fetch_add(&cb_header.next_seq, 1, __ATOMIC_RELEASE);

    cb_header.write_index = (int)((idx + 1) % CB_RING_SIZE);
}

extern "C" void crashBreadcrumbRequestDelta(int delta, uint32_t arg0, uint32_t arg1) {
    if (delta > 0) {
        g_active_request_count++;
    } else if (delta < 0 && g_active_request_count > 0) {
        g_active_request_count--;
    }
    cb_validate_ring();
    cb_header.active_requests = (uint32_t)g_active_request_count;
    crashBreadcrumbRecord(delta > 0 ? CRUMB_WEB_ACCEPT : CRUMB_WEB_REQUEST_DESTROYED,
                          (uint32_t)g_active_request_count, arg1);
}

extern "C" void crashBreadcrumbReport(void (*logFunc)(const char *)) {
    if (logFunc == nullptr) return;

    cb_validate_ring();

    char line[240];

    snprintf(line, sizeof(line),
             "DIAG: breadcrumb ring v%u, %d slots, next_seq=%u",
             cb_header.version, CB_RING_SIZE, cb_header.next_seq);
    logFunc(line);

    for (int i = 0; i < CB_RING_SIZE; i++) {
        int idx = (cb_header.write_index + i) % CB_RING_SIZE;
        BreadcrumbRecord &r = cb_ring[idx];
        if (r.seq == 0) continue;

        snprintf(line, sizeof(line),
                 "DIAG: [#%lu] evt=%lu a0=%lu a1=%lu heap=%lu maxblk=%lu min=%lu "
                 "core=%d stkhw=%ld t=%lu reqs=%lu heapok=%u",
                 (unsigned long)r.seq, (unsigned long)r.event,
                 (unsigned long)r.arg0, (unsigned long)r.arg1,
                 (unsigned long)r.free_heap, (unsigned long)r.max_block,
                 (unsigned long)r.min_heap, (int)r.core, (long)r.stk_hwm,
                 (unsigned long)r.time_ms, (unsigned long)r.active_reqs,
                 (unsigned)r.heap_ok);
        logFunc(line);
    }

    snprintf(line, sizeof(line), "DIAG: breadcrumb report complete (%lu records shown)",
             (unsigned long)cb_header.next_seq);
    logFunc(line);

    cb_header.magic = 0; // force re-init on next boot
}

extern "C" void crashBreadcrumbPressureRequest(uint32_t targetMaxBlock, uint32_t durationMs) {
    if (targetMaxBlock < CB_PRESSURE_MIN_TARGET) targetMaxBlock = CB_PRESSURE_MIN_TARGET;
    if (targetMaxBlock > CB_PRESSURE_MAX_TARGET) targetMaxBlock = CB_PRESSURE_MAX_TARGET;
    if (durationMs < CB_PRESSURE_MIN_DURATION) durationMs = CB_PRESSURE_MIN_DURATION;
    if (durationMs > CB_PRESSURE_MAX_DURATION) durationMs = CB_PRESSURE_MAX_DURATION;

    g_pressure_target = targetMaxBlock;
    g_pressure_deadline = millis() + durationMs;
    g_pressure_active = true;

    crashBreadcrumbRecord(CRUMB_PRESSURE_REQUESTED, targetMaxBlock, durationMs);
}

extern "C" void crashBreadcrumbPressureUpdate(void) {
    if (!g_pressure_active) return;

    uint32_t now = millis();

    if (now >= g_pressure_deadline) {
        goto release;
    }

    while (cb_get_max_block() > g_pressure_target && g_pressure_count < CB_MAX_PRESSURE_BLOCKS) {
        if (ESP.getFreeHeap() < CB_PRESSURE_MIN_FREE_FLOOR) break;
        uint8_t *block = (uint8_t *)malloc(CB_PRESSURE_CHUNK);
        if (block == nullptr) break;
        memset(block, 0xA5, CB_PRESSURE_CHUNK);
        g_pressure_blocks[g_pressure_count++] = block;
    }

    return;

release:
    for (int i = 0; i < g_pressure_count; i++) {
        free(g_pressure_blocks[i]);
        g_pressure_blocks[i] = nullptr;
    }
    g_pressure_count = 0;
    g_pressure_active = false;
    crashBreadcrumbRecord(CRUMB_PRESSURE_RELEASED, ESP.getFreeHeap(), cb_get_max_block());
}

extern "C" CrashPressureStatus crashBreadcrumbPressureStatus(void) {
    CrashPressureStatus s = {};
    s.active = g_pressure_active;
    s.target_max_block = g_pressure_target;
    s.deadline_ms = g_pressure_deadline;
    s.allocated_blocks = (uint32_t)g_pressure_count;
    s.free_heap = ESP.getFreeHeap();
    s.max_block = cb_get_max_block();
    return s;
}

#endif // COOP_CRASH_DIAGNOSTICS && ESP_PLATFORM
