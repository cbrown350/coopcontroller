#include "SunriseSunset.h"
#include "Logger.h"
#include <time.h>
#include <SolarCalculator.h>

SunriseSunsetCalculator::SunriseSunsetCalculator() {
  latitude_ = 40.7128;  // Default: New York City
  longitude_ = -74.0060;
  utcOffset_ = -5; // Default: EST
  lastCalculation_ = 0;
  sunriseMinutes_ = 360; // Default: 6:00 AM
  sunsetMinutes_ = 1080;  // Default: 6:00 PM
}

void SunriseSunsetCalculator::begin(double lat, double lon, int utcOffset) {
  latitude_ = lat;
  longitude_ = lon;
  utcOffset_ = utcOffset;
  lastCalculation_ = 0; // Force calculation on first update
  logger.logf("SunriseSunsetCalculator initialized: lat=%.4f, lon=%.4f, UTC offset=%d", lat, lon, utcOffset);
}

void SunriseSunsetCalculator::update() {
  if (!shouldCalculate()) {
    return;
  }
  
  forceUpdate();
}

void SunriseSunsetCalculator::forceUpdate() {
  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    logger.logWarning("Failed to get local time for sunrise/sunset calculation");
    return;
  }
  
  // Calculate sunrise/sunset for today using SolarCalculator library functions
  double transit, sunrise, sunset;
  
  // Use the library function to calculate sunrise/sunset (returns UTC time)
  calcSunriseSunset(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   latitude_, longitude_, transit, sunrise, sunset);
  
  // Convert from UTC to local time by adding the UTC offset
  // Note: utcOffset_ is negative for western hemisphere (e.g., -7 for Mountain Time)
  double sunriseLocal = sunrise + utcOffset_;
  double sunsetLocal = sunset + utcOffset_;
  
  // Handle day wraparound (if time goes negative or exceeds 24 hours)
  if (sunriseLocal < 0) sunriseLocal += 24;
  if (sunriseLocal >= 24) sunriseLocal -= 24;
  if (sunsetLocal < 0) sunsetLocal += 24;
  if (sunsetLocal >= 24) sunsetLocal -= 24;
  
  // Convert from hours to minutes since midnight
  int sunriseHour = (int)sunriseLocal;
  int sunriseMinute = (int)((sunriseLocal - sunriseHour) * 60);
  int sunsetHour = (int)sunsetLocal;
  int sunsetMinute = (int)((sunsetLocal - sunsetHour) * 60);
  
  if (sunriseHour >= 0 && sunriseMinute >= 0) {
    sunriseMinutes_ = sunriseHour * 60 + sunriseMinute;
  } else {
    logger.logWarning("Invalid sunrise time calculated");
    sunriseMinutes_ = 360; // Default to 6:00 AM
  }
  
  if (sunsetHour >= 0 && sunsetMinute >= 0) {
    sunsetMinutes_ = sunsetHour * 60 + sunsetMinute;
  } else {
    logger.logWarning("Invalid sunset time calculated");
    sunsetMinutes_ = 1080; // Default to 6:00 PM
  }
  
  lastCalculation_ = time(nullptr);
  
  logger.logf("Sunrise/sunset calculated for %04d-%02d-%02d: Sunrise %s (%02d:%02d local), Sunset %s (%02d:%02d local) [UTC offset: %d]",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                getSunriseTime().c_str(), sunriseHour, sunriseMinute,
                getSunsetTime().c_str(), sunsetHour, sunsetMinute,
                utcOffset_);
}

int SunriseSunsetCalculator::getSunriseMinutes() const {
  return sunriseMinutes_;
}

int SunriseSunsetCalculator::getSunsetMinutes() const {
  return sunsetMinutes_;
}

String SunriseSunsetCalculator::getSunriseTime() const {
  int hour = sunriseMinutes_ / 60;
  int minute = sunriseMinutes_ % 60;
  
  // Convert to 12-hour format
  String period = (hour >= 12) ? "PM" : "AM";
  hour = (hour > 12) ? hour - 12 : hour;
  hour = (hour == 0) ? 12 : hour; // Convert 0 to 12
  
  char buffer[16];
  sprintf(buffer, "%02d:%02d %s", hour, minute, period.c_str());
  return String(buffer);
}

String SunriseSunsetCalculator::getSunsetTime() const {
  int hour = sunsetMinutes_ / 60;
  int minute = sunsetMinutes_ % 60;
  
  // Convert to 12-hour format
  String period = (hour >= 12) ? "PM" : "AM";
  hour = (hour > 12) ? hour - 12 : hour;
  hour = (hour == 0) ? 12 : hour; // Convert 0 to 12
  
  char buffer[16];
  sprintf(buffer, "%02d:%02d %s", hour, minute, period.c_str());
  return String(buffer);
}

bool SunriseSunsetCalculator::shouldCalculate() const {
  if (lastCalculation_ == 0) {
    return true; // First calculation
  }
  
  time_t now = time(nullptr);
  if (now == 0) {
    return false; // Invalid time
  }
  
  // Check if 24 hours have passed
  return (now - lastCalculation_) >= 86400; // 24 * 60 * 60
}