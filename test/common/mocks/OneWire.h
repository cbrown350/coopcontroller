#ifndef ONEWIRE_MOCK_H
#define ONEWIRE_MOCK_H

#include <cstdint>

class OneWire {
public:
    OneWire(uint8_t pin) {
        (void)pin;
    }
    
    ~OneWire() {
    }
    
    // Master reset - returns bool (true if device present)
    bool reset(void) {
        return true;  // Mock: device present
    }
    
    // Search methods
    void reset_search(void) {
    }
    
    uint8_t search(uint8_t* addr) {
        (void)addr;
        return 0;  // No devices found in mock
    }
    
    void target_search(uint8_t family_code) {
        (void)family_code;
    }
    
    void depower(void) {
    }
    
    // Communication methods
    uint8_t write(uint8_t v) {
        (void)v;
        return 0;  // Return number of bytes written
    }
    
    void write_bit(bool v) {
        (void)v;
    }
    
    uint8_t read(void) {
        return 0;  // Mock: return 0
    }
    
    uint8_t read_bit(void) {
        return 0;
    }
    
    void select(uint8_t* addr) {
        (void)addr;
    }
    
    void skip(void) {
    }
};

#endif // ONEWIRE_MOCK_H
