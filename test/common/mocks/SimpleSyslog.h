#ifndef __SIMPLESYSLOG_H__
#define __SIMPLESYSLOG_H__

#include <stdio.h>
#include <stdarg.h>

// Real SimpleSyslog class for testing (no-op implementation).
// Adds a static send counter so tests can assert how many UDP sends the Logger
// actually made (used to verify the syslog rate limiter). sendCount is bumped
// once per printf() — i.e. once per would-be UDP packet.
class SimpleSyslog {
public:
    // C++17 inline static: one shared definition across all test TUs that
    // include this header, no separate .cpp definition needed (avoids
    // undefined-symbol link errors in suites that don't use the counter).
    inline static int sendCount = 0;  ///< Total printf()/send calls across all instances
    static void resetSendCount() { sendCount = 0; }

    // Updated constructor to match Logger's usage (5 args)
    SimpleSyslog(const char* hostname, const char* app, const char* server, uint16_t port = 514, uint16_t max_packet_size = 128) {} // NOSONAR
    void printf(int facility, int severity, const char* format, ...) const { // NOSONAR
        sendCount++;
        va_list args;
        va_start(args, format);
        (void)facility; (void)severity; (void)format; // no stdout spam in tests
        va_end(args);
    }
};

#define FAC_USER 1 // NOSONAR
#define PRI_DEBUG 7 // NOSONAR

#endif // __SIMPLESYSLOG_H__