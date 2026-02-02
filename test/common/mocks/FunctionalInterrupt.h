// Mock FunctionalInterrupt.h for desktop unit testing
// This file provides mock implementations for ESP32 interrupt functionality

#ifndef FUNCTIONAL_INTERRUPT_MOCK_H
#define FUNCTIONAL_INTERRUPT_MOCK_H

#include <functional>
#include <stdint.h>

namespace std {
    // Mock attachInterrupt - does nothing in desktop testing
    // In a real ESP32 environment, this would attach an ISR to a pin
    // For testing purposes, we just provide a stub that does nothing
    // Accepts std::function to support std::bind usage in production code
    inline void attachInterrupt(uint8_t pin, std::function<void()> isr, int mode) {
        // Stub - does nothing in desktop environment
    }
    
    // Mock detachInterrupt - does nothing in desktop testing
    inline void detachInterrupt(uint8_t pin) {
        // Stub - does nothing in desktop environment
    }
}

// Define interrupt mode constants (from ESP32 Arduino esp32-hal-gpio.h)
#ifndef RISING
#define RISING 1
#endif
#ifndef FALLING
#define FALLING 2
#endif
#ifndef CHANGE
#define CHANGE 3
#endif

// Only define LOW/HIGH if not already defined by ArduinoFake
#ifndef LOW
#define LOW 0
#endif

#ifndef HIGH
#define HIGH 1
#endif

#endif // FUNCTIONAL_INTERRUPT_MOCK_H
