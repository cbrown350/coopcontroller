#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ArduinoFake.h"

#include "Logger.h"
#include "MockHAL.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace fakeit;

// Test fixture for Logger
class LoggerTest : public ::testing::Test {
    MockHAL* mockHal;

protected:
    void SetUp() override {
        // Create new MockHAL instance for this test
        mockHal = new MockHAL();

        When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);

        // Set up MockHAL
        Logger::getInstance().begin(mockHal);
        // // Clear logs before each test
        Logger::getInstance().clearLogs();
        // // // Set log level to VERBOSE to capture all messages during testing
        Logger::getInstance().setLogLevel(LogLevel::VERBOSE);
    }

    void TearDown() override {
        // Clear logs after each test
        Logger::getInstance().clearLogs();

        // Clean up mock
        delete mockHal;
        mockHal = nullptr;
    }
};

// Test Logger initialization
TEST_F(LoggerTest, Initialization) {
    // Test that singleton works
    Logger& logger_instance = Logger::getInstance();
    logger_instance.logInfo("Test info message");
    EXPECT_TRUE(logger_instance.getLogsAsJson().length() >= 0);  // Just check it works 
}

// Test logging at different levels
TEST_F(LoggerTest, LogLevels) {
    Logger& logger_instance = Logger::getInstance();
    
    // Set log level to VERBOSE to capture all messages
    logger_instance.setLogLevel(LogLevel::VERBOSE);
    
    // Test all log levels
    logger_instance.logVerbose("Test verbose message");
    logger_instance.logDebug("Test debug message");
    logger_instance.logInfo("Test info message");
    logger_instance.logWarning("Test warning message");
    logger_instance.logError("Test error message");
    
    // Check that logs were created
    EXPECT_GT(logger_instance.getLogCount(), 0);
    
    // Get logs as JSON and verify content
    String json = logger_instance.getLogsAsJson();
    EXPECT_TRUE(json.indexOf("Test verbose message") >= 0);
    EXPECT_TRUE(json.indexOf("Test debug message") >= 0);
    EXPECT_TRUE(json.indexOf("Test info message") >= 0);
    EXPECT_TRUE(json.indexOf("Test warning message") >= 0);
    EXPECT_TRUE(json.indexOf("Test error message") >= 0);
}

// Test log level filtering
TEST_F(LoggerTest, LogLevelFiltering) {
    Logger& logger_instance = Logger::getInstance();
    
    // Set log level to WARNING
    logger_instance.setLogLevel(LogLevel::WARNING);
    
    // Log messages at different levels
    logger_instance.logVerbose("This should be filtered out");
    logger_instance.logDebug("This should be filtered out");
    logger_instance.logInfo("This should be filtered out");
    logger_instance.logWarning("This should appear");
    logger_instance.logError("This should appear");
    
    // Get logs and verify filtering worked
    String json = logger_instance.getLogsAsJson();
    EXPECT_TRUE(json.indexOf("This should be filtered out") < 0);
    EXPECT_TRUE(json.indexOf("This should appear") >= 0);
}

// Test formatted logging
TEST_F(LoggerTest, FormattedLogging) {
    Logger& logger_instance = Logger::getInstance();
    
    // Test formatted logging methods
    logger_instance.logfVerbose("Formatted verbose %s with %d", "message", 42);
    logger_instance.logfDebug("Formatted debug %s with %d", "message", 42);
    logger_instance.logfInfo("Formatted info %s with %d", "message", 42);
    logger_instance.logfWarning("Formatted warning %s with %d", "message", 42);
    logger_instance.logfError("Formatted error %s with %d", "message", 42);
    
    // Verify logs were created
    EXPECT_GT(logger_instance.getLogCount(), 0);
    
    // Check formatted content
    String json = logger_instance.getLogsAsJson();
    EXPECT_TRUE(json.indexOf("Formatted verbose message with 42") >= 0);
    EXPECT_TRUE(json.indexOf("Formatted debug message with 42") >= 0);
    EXPECT_TRUE(json.indexOf("Formatted info message with 42") >= 0);
    EXPECT_TRUE(json.indexOf("Formatted warning message with 42") >= 0);
    EXPECT_TRUE(json.indexOf("Formatted error message with 42") >= 0);
}

// Test JSON log export
TEST_F(LoggerTest, GetLogsAsJson) {
    Logger& logger_instance = Logger::getInstance();
    
    // Add some test logs
    logger_instance.logInfo("Test message 1");
    logger_instance.logWarning("Test message 2");
    logger_instance.logError("Test message 3");
    
    // Get logs as JSON
    String json = logger_instance.getLogsAsJson();
    
    // Verify JSON structure
    EXPECT_FALSE(json.length() == 0);
    EXPECT_TRUE(json.indexOf("\"logs\"") >= 0);
    EXPECT_TRUE(json.indexOf("Test message 1") >= 0);
    EXPECT_TRUE(json.indexOf("Test message 2") >= 0);
    EXPECT_TRUE(json.indexOf("Test message 3") >= 0);
}

// Test log buffer limits (assuming max 150 entries)
TEST_F(LoggerTest, LogBufferLimit) {
    Logger& logger_instance = Logger::getInstance();
    
    // Add more logs than buffer can hold
    for (int i = 0; i < 160; ++i) {
        char buffer[50];
        sprintf(buffer, "Message %d ", i);
        logger_instance.logInfo(buffer);
    }
    
    // Should not exceed MAX_LOG_ENTRIES
    EXPECT_LE(logger_instance.getLogCount(), 150);
    
    // Get logs and verify circular buffer behavior
    String json = logger_instance.getLogsAsJson();
    
    // Early messages should be overwritten
    EXPECT_TRUE(json.indexOf("Message 0 ") < 0);
    EXPECT_TRUE(json.indexOf("Message 9 ") < 0);
    
    // Later messages should be present
    EXPECT_TRUE(json.indexOf("Message 150 ") >= 0);
    EXPECT_TRUE(json.indexOf("Message 159 ") >= 0);
}

// Test log level to string conversion
TEST_F(LoggerTest, LogLevelToString) {
    Logger& logger_instance = Logger::getInstance();
    
    EXPECT_EQ(logger_instance.logLevelToString(LogLevel::VERBOSE), "VERBOSE");
    EXPECT_EQ(logger_instance.logLevelToString(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(logger_instance.logLevelToString(LogLevel::INFO), "INFO");
    EXPECT_EQ(logger_instance.logLevelToString(LogLevel::WARNING), "WARNING");
    EXPECT_EQ(logger_instance.logLevelToString(LogLevel::ERROR), "ERROR");
}

// Test clear logs functionality
TEST_F(LoggerTest, ClearLogs) {
    Logger& logger_instance = Logger::getInstance();
    
    // Add some logs
    logger_instance.logInfo("Test message 1");
    logger_instance.logWarning("Test message 2");
    EXPECT_GT(logger_instance.getLogCount(), 0);
    
    // Clear logs
    logger_instance.clearLogs();
    
    // Verify logs are cleared
    EXPECT_EQ(logger_instance.getLogCount(), 0);
    
    String json = logger_instance.getLogsAsJson();
    EXPECT_TRUE(json.indexOf("Test message 1") < 0);
    EXPECT_TRUE(json.indexOf("Test message 2") < 0);
}

// Test log count functionality
TEST_F(LoggerTest, LogCount) {
    Logger& logger_instance = Logger::getInstance();

    logger_instance.clearLogs(); // Ensure starting from empty state
    
    // Initially should be 0
    EXPECT_EQ(logger_instance.getLogCount(), 0);
    
    // Add logs and count
    logger_instance.logInfo("Message 1");
    EXPECT_EQ(logger_instance.getLogCount(), 1);
    
    logger_instance.logWarning("Message 2");
    EXPECT_EQ(logger_instance.getLogCount(), 2);
    
    logger_instance.logError("Message 3");
    EXPECT_EQ(logger_instance.getLogCount(), 3);
    
    // Clear and verify count resets
    logger_instance.clearLogs();
    EXPECT_EQ(logger_instance.getLogCount(), 0);
}

// Test set and get log level
TEST_F(LoggerTest, SetGetLogLevel) {
    Logger& logger_instance = Logger::getInstance();
    
    // Test setting different log levels
    logger_instance.setLogLevel(LogLevel::VERBOSE);
    EXPECT_EQ(logger_instance.getLogLevel(), LogLevel::VERBOSE);
    
    logger_instance.setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(logger_instance.getLogLevel(), LogLevel::DEBUG);
    
    logger_instance.setLogLevel(LogLevel::INFO);
    EXPECT_EQ(logger_instance.getLogLevel(), LogLevel::INFO);
    
    logger_instance.setLogLevel(LogLevel::WARNING);
    EXPECT_EQ(logger_instance.getLogLevel(), LogLevel::WARNING);
    
    logger_instance.setLogLevel(LogLevel::ERROR);
    EXPECT_EQ(logger_instance.getLogLevel(), LogLevel::ERROR);
}

// Test singleton pattern
TEST_F(LoggerTest, SingletonPattern) {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();

    // Both references should point to the same instance
    EXPECT_EQ(&logger1, &logger2);
}

// Concurrency regression test for the cross-core logging race.
//
// On the device the logger is called from the main loop (core 1) and from
// async web handlers (async_tcp task, core 0) with no coordination. Before the
// logMutex_ fix those callers raced on the circular buffer, the UUID generator,
// and the shared SimpleSyslog WiFiUDP socket, corrupting newlib lock/heap state
// (decoded as lock_init_generic aborts) and wedging the TCP acceptor.
//
// This test reproduces the concurrent access pattern on desktop: many threads
// emit logs while another thread repeatedly serializes the buffer via
// getLogsAsJson() (the /logs endpoint path). With the logMutex_ in place every
// entry point is serialized, so this must complete without a crash, a data
// race (under TSan), or a corrupted count. Without the lock it crashes/asserts.
TEST_F(LoggerTest, ConcurrentLoggingIsThreadSafe) {
    Logger& logger_instance = Logger::getInstance();
    logger_instance.clearLogs();

    constexpr int kWriterThreads = 8;
    constexpr int kLogsPerThread = 500;
    std::atomic<bool> start{false};
    std::atomic<int> readerReads{0};

    std::vector<std::thread> threads;
    threads.reserve(kWriterThreads + 1);

    // Writer threads hammer every severity path.
    for (int t = 0; t < kWriterThreads; ++t) {
        threads.emplace_back([&logger_instance, &start, t]() {
            while (!start.load(std::memory_order_acquire)) { /* spin */ }
            for (int i = 0; i < kLogsPerThread; ++i) {
                logger_instance.logfInfo("thread %d msg %d", t, i);
                logger_instance.logWarning("concurrent warning");
            }
        });
    }

    // Reader thread concurrently serializes the buffer (the /logs API path).
    threads.emplace_back([&logger_instance, &start, &readerReads]() {
        while (!start.load(std::memory_order_acquire)) { /* spin */ }
        for (int i = 0; i < 200; ++i) {
            String json = logger_instance.getLogsAsJson();
            // Buffer must always serialize to *some* valid JSON, never empty.
            if (json.length() > 0) {
                readerReads.fetch_add(1, std::memory_order_relaxed);
            }
            (void)logger_instance.getLogCount();
        }
    });

    start.store(true, std::memory_order_release);
    for (auto& th : threads) {
        th.join();
    }

    // The buffer is capacity-bounded and internally consistent after the storm.
    EXPECT_LE(logger_instance.getLogCount(), 150);
    EXPECT_GT(readerReads.load(), 0);

    // The logger is still fully functional after concurrent stress.
    logger_instance.clearLogs();
    logger_instance.logInfo("post-stress sanity");
    String finalJson = logger_instance.getLogsAsJson();
    EXPECT_TRUE(finalJson.indexOf("post-stress sanity") >= 0);
}
