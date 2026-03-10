#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>

#include "HAL_ESP32.h"

HAL_ESP32* hal;

void setUp(void) {
    hal = new HAL_ESP32();
    hal->begin();
}

void tearDown(void) {
    delete hal;
    hal = nullptr;
}

// Verify httpPost method exists and compiles with correct signature
void test_httpPost_signature(void) {
    // Just verify the method pointer exists (compile-time check)
    String (IHAL::*fn)(const String&, const String&, unsigned long) = &IHAL::httpPost;
    TEST_ASSERT_NOT_NULL(fn);
}

// Verify httpGet method exists and compiles with correct signature
void test_httpGet_signature(void) {
    String (IHAL::*fn)(const String&, unsigned long) = &IHAL::httpGet;
    TEST_ASSERT_NOT_NULL(fn);
}

// Test httpPost returns empty when WiFi is not connected
// WiFi must be initialized (WiFi.mode) to avoid lwIP crash, but not connected
void test_httpPost_no_wifi_returns_empty(void) {
    // Initialize WiFi stack without connecting (prevents lwIP assertion)
    WiFi.mode(WIFI_STA);
    delay(100);

    String result = hal->httpPost("https://192.0.2.1/test", "{}", 2000);
    // Should fail gracefully - empty string or error
    TEST_ASSERT_TRUE(result.length() == 0 || result.indexOf("error") >= 0);

    WiFi.mode(WIFI_OFF);
}

// Test httpGet returns empty when WiFi is not connected
void test_httpGet_no_wifi_returns_empty(void) {
    WiFi.mode(WIFI_STA);
    delay(100);

    String result = hal->httpGet("https://192.0.2.1/test", 2000);
    TEST_ASSERT_TRUE(result.length() == 0 || result.indexOf("error") >= 0);

    WiFi.mode(WIFI_OFF);
}

// Test system functions work
void test_system_functions(void) {
    TEST_ASSERT_GREATER_THAN(0, hal->getFreeHeap());
    TEST_ASSERT_GREATER_THAN(0, hal->getHeapSize());
    TEST_ASSERT_NOT_NULL(hal->getChipModel());
}

void setup() {
    delay(2000);
    UNITY_BEGIN();

    RUN_TEST(test_httpPost_signature);
    RUN_TEST(test_httpGet_signature);
    RUN_TEST(test_system_functions);
    RUN_TEST(test_httpPost_no_wifi_returns_empty);
    RUN_TEST(test_httpGet_no_wifi_returns_empty);

    UNITY_END();
}

void loop() {
    // Unused
}
