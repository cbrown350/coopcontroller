#include <Arduino.h>
#include <unity.h>

#include "Logger.h"


void test_same_instance(void) {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    
    // Both references should point to the same instance
    TEST_ASSERT_EQUAL(&logger1, &logger2);
}