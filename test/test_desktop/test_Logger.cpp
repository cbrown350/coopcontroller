#include <gtest/gtest.h>
#include "test_common.h"
#include "WiFi.h"
#include "LittleFS.h"
#include "ESPmDNS.h"

// Define Global Mocks
MockESP ESP;
WiFiClass WiFi;
FS LittleFS;
MDNSResponder MDNS;

// Include the library to test
#include "Logger.h"

using namespace fakeit;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoFakeReset();
    }
    
    void TearDown() override {
    }
};

TEST_F(LoggerTest, Initialization) {
    // Act
    Logger& testLogger = Logger::getInstance();  // Use singleton
    testLogger.logInfo("Test message");

    // Assert - Just check that no exception is thrown
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}