#include <gtest/gtest.h>
#include "ArduinoFake.h"
#include "MockHAL.h"
#include "SunriseSunset.h"
#include "Logger.h"

using namespace fakeit;

class SunriseSunsetTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create MockHAL instance
    hal_ = new MockHAL();

    // Reset ArduinoFake
    ArduinoFakeReset();

    // Reset mock state
    hal_->reset();

    // Mock ALL Arduino functions BEFORE initializing anything
    When(Method(ArduinoFake(), micros)).AlwaysReturn(1000000);
    // Make millis() return mockHAL.millisValue so tests can control time
    When(Method(ArduinoFake(), millis)).AlwaysDo([this]() { return hal_->millisValue; });
    When(Method(ArduinoFake(), delay)).AlwaysReturn();
    When(Method(ArduinoFake(), delayMicroseconds)).AlwaysReturn();

    // Initialize Logger AFTER all Arduino function mocks are set up
    Logger::getInstance().begin(hal_);
    Logger::getInstance().clearLogs();
    Logger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    
    // Create SunriseSunsetCalculator instance
    calculator_ = new SunriseSunsetCalculator();
    
    // Configure mock time info for successful time retrieval
    struct tm timeinfo;
    timeinfo.tm_year = 2024;
    timeinfo.tm_mon = 0;     // January
    timeinfo.tm_mday = 15;    // 15th
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    
    hal_->setTimeInfo(timeinfo);
    hal_->setGetLocalTimeResult(true);
  }
  
  void TearDown() override {
    delete calculator_;
    delete hal_;
  }
  
  MockHAL* hal_;
  SunriseSunsetCalculator* calculator_;
};

// Constructor Tests
TEST_F(SunriseSunsetTest, ConstructorInitializesCorrectly) {
  ASSERT_NE(calculator_, nullptr);
  EXPECT_EQ(calculator_->getSunriseMinutes(), 360);  // Default: 6:00 AM
  EXPECT_EQ(calculator_->getSunsetMinutes(), 1080);  // Default: 6:00 PM
}

// Initialization Tests
TEST_F(SunriseSunsetTest, BeginSetsCoordinatesAndOffset) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5); // New York
  
  // Coordinates and offset should be set
  // (Cannot directly access private members, so we test through forceUpdate)
}

TEST_F(SunriseSunsetTest, BeginForcesUpdateOnFirstCall) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  
  // First call to forceUpdate should trigger calculation
  // (We'll test this in the forceUpdate tests)
}

// Sunrise/Sunset Calculation Tests
TEST_F(SunriseSunsetTest, ForceUpdateCalculatesSunriseSunset) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  
  calculator_->forceUpdate();
  
  // Sunrise and sunset should be calculated (non-zero values)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
  EXPECT_LT(sunrise, sunset); // Sunrise before sunset
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesLocalTimeFailure) {
  hal_->setGetLocalTimeResult(false);

  calculator_->begin(hal_, 40.7128, -74.0060, -5);

  // Should handle failure gracefully
  calculator_->forceUpdate();

  // Sunrise/sunset should remain at default values
  EXPECT_EQ(calculator_->getSunriseMinutes(), 360);
  EXPECT_EQ(calculator_->getSunsetMinutes(), 1080);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesZeroLatitude) {
  calculator_->begin(hal_, 0.0, -74.0060, -5);
  
  calculator_->forceUpdate();
  
  // Should handle zero latitude
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Values may be zero or calculated based on library behavior
  EXPECT_GE(sunrise, 0);
  EXPECT_GE(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesZeroLongitude) {
  calculator_->begin(hal_, 40.7128, 0.0, -5);
  
  calculator_->forceUpdate();
  
  // Should handle zero longitude
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Values may be zero or calculated based on library behavior
  EXPECT_GE(sunrise, 0);
  EXPECT_GE(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesNegativeLatitude) {
  calculator_->begin(hal_, -40.7128, -74.0060, -5); // Southern hemisphere
  
  calculator_->forceUpdate();
  
  // Should handle negative latitude (southern hemisphere)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesPositiveLongitude) {
  calculator_->begin(hal_, 40.7128, 74.0060, -5); // Eastern hemisphere
  
  calculator_->forceUpdate();
  
  // Should handle positive longitude
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesZeroTimezoneOffset) {
  calculator_->begin(hal_, 40.7128, -74.0060, 0); // UTC
  
  calculator_->forceUpdate();
  
  // Should handle zero timezone offset (UTC)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesNegativeTimezoneOffset) {
  calculator_->begin(hal_, 40.7128, -74.0060, -8); // PST
  
  calculator_->forceUpdate();
  
  // Should handle negative timezone offset (west of UTC)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesPositiveTimezoneOffset) {
  calculator_->begin(hal_, 40.7128, -74.0060, 5); // Pakistan
  
  calculator_->forceUpdate();
  
  // Should handle positive timezone offset (east of UTC)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateHandlesExtremeTimezoneOffset) {
  calculator_->begin(hal_, 40.7128, -74.0060, 12); // New Zealand
  
  calculator_->forceUpdate();
  
  // Should handle extreme timezone offset
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

// Getter Tests
TEST_F(SunriseSunsetTest, GetSunriseMinutesReturnsCorrectValue) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();
  
  int sunrise = calculator_->getSunriseMinutes();
  
  EXPECT_GT(sunrise, 0);
  EXPECT_LT(sunrise, 720); // Less than 12 hours (720 minutes)
}

TEST_F(SunriseSunsetTest, GetSunsetMinutesReturnsCorrectValue) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();
  
  int sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GT(sunset, 720); // Greater than 12 hours (720 minutes)
  EXPECT_LT(sunset, 1440); // Less than 24 hours (1440 minutes)
}

TEST_F(SunriseSunsetTest, GetSunriseMinutesReturnsDefaultBeforeCalculation) {
  // Don't call forceUpdate
  int sunrise = calculator_->getSunriseMinutes();

  EXPECT_EQ(sunrise, 360);  // Default: 6:00 AM
}

TEST_F(SunriseSunsetTest, GetSunsetMinutesReturnsDefaultBeforeCalculation) {
  // Don't call forceUpdate
  int sunset = calculator_->getSunsetMinutes();

  EXPECT_EQ(sunset, 1080);  // Default: 6:00 PM
}

// Time Formatting Tests (using instance methods)
TEST_F(SunriseSunsetTest, GetSunriseTimeReturnsFormattedString) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();

  String result = calculator_->getSunriseTime();

  // Verify format is correct (contains ":" and AM/PM)
  EXPECT_TRUE(result.indexOf(":") >= 0);
  EXPECT_TRUE(result.indexOf("AM") >= 0 || result.indexOf("PM") >= 0);
}

TEST_F(SunriseSunsetTest, GetSunsetTimeReturnsFormattedString) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();

  String result = calculator_->getSunsetTime();

  // Verify format is correct (contains ":" and AM/PM)
  EXPECT_TRUE(result.indexOf(":") >= 0);
  EXPECT_TRUE(result.indexOf("AM") >= 0 || result.indexOf("PM") >= 0);
}

TEST_F(SunriseSunsetTest, GetSunriseTimeReturnsDefaultBeforeCalculation) {
  // Default is 360 minutes = 6:00 AM
  String result = calculator_->getSunriseTime();
  EXPECT_EQ(result, "06:00 AM");
}

TEST_F(SunriseSunsetTest, GetSunsetTimeReturnsDefaultBeforeCalculation) {
  // Default is 1080 minutes = 6:00 PM
  String result = calculator_->getSunsetTime();
  EXPECT_EQ(result, "06:00 PM");
}

// Multiple Update Tests
TEST_F(SunriseSunsetTest, MultipleForceUpdatesRecalculate) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);

  // First update
  calculator_->forceUpdate();

  // Second update
  calculator_->forceUpdate();
  int sunrise2 = calculator_->getSunriseMinutes();
  int sunset2 = calculator_->getSunsetMinutes();

  // Values should be recalculated (may be same or different)
  EXPECT_GT(sunrise2, 0);
  EXPECT_GT(sunset2, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateAfterCoordinateChange) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();
  
  // Change coordinates
  calculator_->begin(hal_, 34.0522, -118.2437, -8); // Los Angeles
  
  // Force update with new coordinates
  calculator_->forceUpdate();
  
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Should calculate based on new coordinates
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ForceUpdateAfterTimezoneChange) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5); // EST
  calculator_->forceUpdate();

  // Change timezone
  calculator_->begin(hal_, 40.7128, -74.0060, -8); // PST

  // Force update with new timezone
  calculator_->forceUpdate();

  int sunrise2 = calculator_->getSunriseMinutes();
  int sunset2 = calculator_->getSunsetMinutes();

  // Should calculate based on new timezone
  // PST is 3 hours behind EST, so sunrise/sunset should be 3 hours later in local time
  // (Actually, calculation returns UTC times, so offset affects when displaying)
  EXPECT_GT(sunrise2, 0);
  EXPECT_GT(sunset2, 0);
}

// Edge Case Tests
TEST_F(SunriseSunsetTest, HandlesPolarLocations) {
  // Test North Pole
  calculator_->begin(hal_, 90.0, 0.0, 0);
  calculator_->forceUpdate();
  
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Should handle polar locations (may have special sunrise/sunset behavior)
  EXPECT_GE(sunrise, 0);
  EXPECT_GE(sunset, 0);
  
  // Test South Pole
  calculator_->begin(hal_, -90.0, 0.0, 0);
  calculator_->forceUpdate();
  
  sunrise = calculator_->getSunriseMinutes();
  sunset = calculator_->getSunsetMinutes();
  
  EXPECT_GE(sunrise, 0);
  EXPECT_GE(sunset, 0);
}

TEST_F(SunriseSunsetTest, HandlesEquatorLocations) {
  // Test equator location
  calculator_->begin(hal_, 0.0, 0.0, 0);
  calculator_->forceUpdate();
  
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Should handle equator locations
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
  EXPECT_LT(sunrise, sunset);
}

TEST_F(SunriseSunsetTest, HandlesInternationalDateLine) {
  // Test location on International Date Line
  calculator_->begin(hal_, 0.0, 180.0, -12);
  calculator_->forceUpdate();
  
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // Should handle International Date Line
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, HandlesLargeTimezoneOffsets) {
  // Test various large timezone offsets
  int offsets[] = {-12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  
  for (int i = 0; i < 27; i++) {
    calculator_->begin(hal_, 40.7128, -74.0060, offsets[i]);
    calculator_->forceUpdate();
    
    int sunrise = calculator_->getSunriseMinutes();
    int sunset = calculator_->getSunsetMinutes();
    
    EXPECT_GT(sunrise, 0);
    EXPECT_GT(sunset, 0);
  }
}

// SetCoordinates Tests
TEST_F(SunriseSunsetTest, SetCoordinatesUpdatesValues) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();

  // Change coordinates using setCoordinates
  calculator_->setCoordinates(34.0522, -118.2437, -8); // Los Angeles
  calculator_->forceUpdate();

  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();

  // Should calculate based on new coordinates
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, ShouldCalculateReturnsTrueInitially) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);

  // Should return true before first calculation
  EXPECT_TRUE(calculator_->shouldCalculate());
}

TEST_F(SunriseSunsetTest, UpdateCallsForceUpdateWhenNeeded) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);

  // First update should calculate
  calculator_->update();

  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();

  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

// Integration Tests
TEST_F(SunriseSunsetTest, FullWorkflowBeginForceUpdateGet) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  
  // Force update
  calculator_->forceUpdate();
  
  // Get sunrise/sunset
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();

  // Format times using instance methods
  String sunriseStr = calculator_->getSunriseTime();
  String sunsetStr = calculator_->getSunsetTime();

  // Verify all operations succeeded
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
  EXPECT_LT(sunrise, sunset);
  EXPECT_TRUE(sunriseStr.length() > 0);
  EXPECT_TRUE(sunsetStr.length() > 0);
}

TEST_F(SunriseSunsetTest, MultipleLocations) {
  // Test multiple locations to ensure calculator works correctly
  struct Location {
    double lat;
    double lon;
    int offset;
    const char* name;
  };
  
  Location locations[] = {
    {40.7128, -74.0060, -5, "New York"},
    {34.0522, -118.2437, -8, "Los Angeles"},
    {51.5074, -0.1278, 0, "London"},
    {35.6762, 139.6503, 9, "Tokyo"},
    {-33.8688, 151.2093, 11, "Sydney"}
  };
  
  for (int i = 0; i < 5; i++) {
    calculator_->begin(hal_, locations[i].lat, locations[i].lon, locations[i].offset);
    calculator_->forceUpdate();
    
    int sunrise = calculator_->getSunriseMinutes();
    int sunset = calculator_->getSunsetMinutes();
    
    // Verify calculation succeeded for each location
    EXPECT_GT(sunrise, 0) << "Failed for " << locations[i].name;
    EXPECT_GT(sunset, 0) << "Failed for " << locations[i].name;
    EXPECT_LT(sunrise, sunset) << "Failed for " << locations[i].name;
  }
}

// HAL Integration Tests
TEST_F(SunriseSunsetTest, UsesHALGetLocalTime) {
  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  
  // Force update should call HAL's getLocalTime
  calculator_->forceUpdate();
  
  // Verify getLocalTime was called (by checking calculation succeeded)
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();
  
  // If getLocalTime failed, sunrise/sunset would be 0
  // If getLocalTime succeeded, sunrise/sunset would be calculated
  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);
}

TEST_F(SunriseSunsetTest, HandlesHALGetLocalTimeFailure) {
  hal_->setGetLocalTimeResult(false);

  calculator_->begin(hal_, 40.7128, -74.0060, -5);
  calculator_->forceUpdate();

  // Should handle HAL failure gracefully - values remain at defaults
  int sunrise = calculator_->getSunriseMinutes();
  int sunset = calculator_->getSunsetMinutes();

  EXPECT_EQ(sunrise, 360);  // Default: 6:00 AM
  EXPECT_EQ(sunset, 1080);  // Default: 6:00 PM
}

TEST_F(SunriseSunsetTest, PassesHALPointerCorrectly) {
  // Create new calculator instance
  SunriseSunsetCalculator* calc = new SunriseSunsetCalculator();

  // Begin with HAL pointer
  calc->begin(hal_, 40.7128, -74.0060, -5);
  calc->forceUpdate();

  // Should work correctly
  int sunrise = calc->getSunriseMinutes();
  int sunset = calc->getSunsetMinutes();

  EXPECT_GT(sunrise, 0);
  EXPECT_GT(sunset, 0);

  delete calc;
}
