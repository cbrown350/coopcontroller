// Mock ElegantOTA.h for desktop unit testing
// This file provides mock implementations for ElegantOTA functionality

#ifndef ELEGANT_OTA_MOCK_H
#define ELEGANT_OTA_MOCK_H

#include <functional>

// Mock ElegantOTA class for desktop testing
class ElegantOTAClass {
public:
    // Mock setAuth - does nothing in desktop testing
    void setAuth(const char* username, const char* password) {
        // Stub - does nothing in desktop environment
        (void)username;
        (void)password;
    }

    // Mock onProgress - accepts callback but does nothing
    void onProgress(std::function<void(unsigned int, unsigned int)> callback) {
        (void)callback;
    }

    // Mock onStart - accepts callback but does nothing
    void onStart(std::function<void()> callback) {
        (void)callback;
    }

    // Mock onEnd - accepts callback but does nothing
    void onEnd(std::function<void(bool)> callback) {
        (void)callback;
    }

    // Mock loop - does nothing in desktop testing
    void loop() {
        // Stub - does nothing in desktop environment
    }
};

// Create a singleton instance to match the library's usage pattern
extern ElegantOTAClass ElegantOTA;

#endif // ELEGANT_OTA_MOCK_H
