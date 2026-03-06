#include "CrashDiagnostics.h"

#include <Arduino.h>
#include <esp_core_dump.h>

const char* xtensaExceptionName(uint32_t cause) {
    switch (cause) {
        case 0:  return "IllegalInstruction";
        case 1:  return "Syscall";
        case 2:  return "InstructionFetchError";
        case 3:  return "LoadStoreError";
        case 4:  return "Level1Interrupt";
        case 5:  return "Alloca";
        case 6:  return "IntegerDivideByZero";
        case 8:  return "Privileged";
        case 9:  return "LoadStoreAlignment";
        case 12: return "InstrPIFDataError";
        case 13: return "LoadStorePIFDataError";
        case 14: return "InstrPIFAddrError";
        case 15: return "LoadStorePIFAddrError";
        case 16: return "InstTLBMiss";
        case 17: return "InstTLBMultiHit";
        case 18: return "InstFetchPrivilege";
        case 20: return "InstFetchProhibited";
        case 24: return "LoadStoreTLBMiss";
        case 25: return "LoadStoreTLBMultiHit";
        case 26: return "LoadStorePrivilege";
        case 28: return "LoadProhibited";
        case 29: return "StoreProhibited";
        default: return "Unknown";
    }
}

void crashDiagnosticsCheck(void (*logFunc)(const char* msg)) {
    char buf[256];

    // Read coredump summary from flash (saved by ESP-IDF panic handler)
    esp_core_dump_summary_t summary;
    esp_err_t err = esp_core_dump_get_summary(&summary);

    if (err == ESP_OK) {
        snprintf(buf, sizeof(buf), "CRASH: Task '%s' at PC=0x%08lx",
                 summary.exc_task, (unsigned long)summary.exc_pc);
        logFunc(buf);

        snprintf(buf, sizeof(buf), "CRASH: Cause=%lu (%s), vaddr=0x%08lx",
                 (unsigned long)summary.ex_info.exc_cause,
                 xtensaExceptionName(summary.ex_info.exc_cause),
                 (unsigned long)summary.ex_info.exc_vaddr);
        logFunc(buf);

        if (summary.exc_bt_info.depth > 0 && !summary.exc_bt_info.corrupted) {
            uint32_t depth = summary.exc_bt_info.depth;
            if (depth > 16) depth = 16;
            int pos = snprintf(buf, sizeof(buf), "CRASH: Backtrace:");
            for (uint32_t i = 0; i < depth && pos < (int)sizeof(buf) - 12; i++) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " 0x%08lx",
                               (unsigned long)summary.exc_bt_info.bt[i]);
            }
            logFunc(buf);
        } else if (summary.exc_bt_info.corrupted) {
            logFunc("CRASH: Backtrace corrupted");
        }

        snprintf(buf, sizeof(buf), "CRASH: ELF SHA256: %.8s...", summary.app_elf_sha256);
        logFunc(buf);

        esp_core_dump_image_erase();
        logFunc("CRASH: Coredump erased after reading");

    } else if (err == ESP_ERR_NOT_FOUND) {
        // No coredump - normal boot, nothing to report
    } else {
        snprintf(buf, sizeof(buf), "CRASH: Failed to read coredump: %s", esp_err_to_name(err));
        logFunc(buf);
    }
}
