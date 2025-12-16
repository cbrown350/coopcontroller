#ifndef __SIMPLESYSLOG_H__
#define __SIMPLESYSLOG_H__

#include <stdio.h>
#include <stdarg.h>

// Real SimpleSyslog class for testing (no-op implementation)
class SimpleSyslog {
public:
    // Updated constructor to match Logger's usage (5 args)
    SimpleSyslog(const char* hostname, const char* app, const char* server, uint16_t port = 514, uint16_t max_packet_size = 128) {} // NOSONAR
    void printf(int facility, int severity, const char* format, ...) const { // NOSONAR
        ::printf("MockSyslog[%d,%d]: ", facility, severity);
        va_list args;
        va_start(args, format);
        ::vprintf(format, args); // NOSONAR
        va_end(args);
    }
};

#define FAC_USER 1 // NOSONAR
#define PRI_DEBUG 7 // NOSONAR

#endif // __SIMPLESYSLOG_H__