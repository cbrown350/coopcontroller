
#include <Arduino.h>
#include <unity.h>

#include "Logger.h"

#include "../common/mocks/MockHAL.h"

void test_same_instance(void);

MockHAL* hal;

void setUp(void) {
    // setup
    hal = new MockHAL();
    hal->wifiConnected = false;
    Logger::getInstance().begin(hal);
}

void tearDown(void) {
    // cleanup
    delete hal;
    hal = nullptr;
}

// For on‑device tests with Arduino
void setup() {
    delay(2000);              // allow serial monitor to start
    UNITY_BEGIN();

    RUN_TEST(test_same_instance);

    UNITY_END();
}

void loop()
{
	// Unused
}