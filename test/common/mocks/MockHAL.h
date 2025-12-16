#ifndef __MOCKHAL_H__
#define __MOCKHAL_H__

#include "IHAL.h"

#include <stdint.h>
#include <cstdarg>
#include <cstdio>

class MockHAL : public IHAL
{
public:
    bool wifiConnected = true;

    void SerialPrintf(const char* format, ...) override { // NOSONAR
        va_list args;
        va_start(args, format);
        vprintf(format, args); // NOSONAR
        va_end(args);
    }

    void SerialPrintln(const char* message) override {
        printf("%s\n", message);
    }

    void SerialPrint(const char* message) override {
        printf("%s", message);
    }

    bool begin() override {
        printf("MockHAL initialized\n");
        return true;
    }

    unsigned long getTime() override {
        return 123456789; // Return a fixed timestamp for testing
    }

    uint32_t getFreeHeap() override {
        return 102400; // Return a fixed heap size for testing
    }

    bool WiFiIsConnected() override {
        return wifiConnected;
    }
};

#endif // __MOCKHAL_H__