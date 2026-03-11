#include "SunriseSunset.h"
#include <Arduino.h>
#include "Logger.h"
#include <time.h>
#include <string>
#include <SolarCalculator.h>

SunriseSunsetCalculator::SunriseSunsetCalculator() {
  latitude_ = 40.7128;  // Default: New York City
  longitude_ = -74.0060;
  utcOffset_ = -5; // Default: EST
  lastCalculation_ = 0;
  sunriseMinutes_ = 360; // Default: 6:00 AM
  sunsetMinutes_ = 1080;  // Default: 6:00 PM
}

void SunriseSunsetCalculator::begin(IHAL* hal, double lat, double lon, int utcOffset) {
  hal_ = hal;
  latitude_ = lat;
  longitude_ = lon;
  utcOffset_ = utcOffset;
  lastCalculation_ = 0; // Force calculation on first update
  logger.logfInfo("SunriseSunsetCalculator initialized: lat=%.4f, lon=%.4f, UTC offset=%d", lat, lon, utcOffset);
}

void SunriseSunsetCalculator::setCoordinates(double lat, double lon, int utcOffset) {
  latitude_ = lat;
  longitude_ = lon;
  utcOffset_ = utcOffset;
  lastCalculation_ = 0; // Force recalculation on next update()
  logger.logfInfo("SunriseSunsetCalculator coordinates updated: lat=%.4f, lon=%.4f, UTC offset=%d", lat, lon, utcOffset);
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
  if (!hal_->getLocalTime(&timeinfo, 1000)) {
    logger.logWarning("Failed to get local time for sunrise/sunset calculation");
    return;
  }

  // Calculate sunrise/sunset for today using SolarCalculator library functions
  double transit;
  double sunrise;
  double sunset;

  // Use the library function to calculate sunrise/sunset (returns UTC time)
  calcSunriseSunset(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   latitude_, longitude_, transit, sunrise, sunset);

  // Determine actual UTC offset (DST-aware if configTzTime was used)
  // Compare localtime vs gmtime to detect the real offset including DST
  int effectiveOffset = utcOffset_; // Fallback to configured static offset
#ifdef ESP32
  // On ESP32 with configTzTime(), localtime returns DST-aware local time.
  // Compare localtime vs gmtime to derive the actual offset including DST.
  time_t nowUtc = time(nullptr);
  if (nowUtc > 86400) { // Only if we have a valid time (past epoch day 1)
    struct tm localTm{}, utcTm{};
    localtime_r(&nowUtc, &localTm);
    gmtime_r(&nowUtc, &utcTm);
    int localMinTotal = localTm.tm_yday * 1440 + localTm.tm_hour * 60 + localTm.tm_min;
    int utcMinTotal = utcTm.tm_yday * 1440 + utcTm.tm_hour * 60 + utcTm.tm_min;
    int diffMinutes = localMinTotal - utcMinTotal;
    // Handle year boundary (Dec 31 vs Jan 1)
    if (diffMinutes > 720) diffMinutes -= 1440 * 365;
    if (diffMinutes < -720) diffMinutes += 1440 * 365;
    effectiveOffset = diffMinutes / 60;
  }
#endif

  // Convert from UTC to local time by adding the UTC offset
  double sunriseLocal = sunrise + effectiveOffset;
  double sunsetLocal = sunset + effectiveOffset;
  
  // Handle day wraparound (if time goes negative or exceeds 24 hours)
  if (sunriseLocal < 0) sunriseLocal += 24;
  if (sunriseLocal >= 24) sunriseLocal -= 24;
  if (sunsetLocal < 0) sunsetLocal += 24;
  if (sunsetLocal >= 24) sunsetLocal -= 24;
  
  // Convert from hours to minutes since midnight
  auto sunriseHour = (int)sunriseLocal;
  auto sunriseMinute = (int)((sunriseLocal - sunriseHour) * 60);
  auto sunsetHour = (int)sunsetLocal;
  auto sunsetMinute = (int)((sunsetLocal - sunsetHour) * 60);
  
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
  
  logger.logfInfo("Sunrise/sunset calculated for %d-%d-%d: Sunrise %s (%d:%02d local), Sunset %s (%d:%02d local) [offset: %d, DST: %s]",
    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
    getSunriseTime().c_str(), sunriseHour, sunriseMinute,
    getSunsetTime().c_str(), sunsetHour, sunsetMinute, effectiveOffset,
    (effectiveOffset != utcOffset_) ? "yes" : "no");
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
  
  std::string hourStr = (hour < 10) ? "0" + std::to_string(hour) : std::to_string(hour);
  std::string minStr = (minute < 10) ? "0" + std::to_string(minute) : std::to_string(minute);
  return String((hourStr + ":" + minStr + " " + period.c_str()).c_str());
}

String SunriseSunsetCalculator::getSunsetTime() const {
  int hour = sunsetMinutes_ / 60;
  int minute = sunsetMinutes_ % 60;
  
  // Convert to 12-hour format
  String period = (hour >= 12) ? "PM" : "AM";
  hour = (hour > 12) ? hour - 12 : hour;
  hour = (hour == 0) ? 12 : hour; // Convert 0 to 12
  
  std::string hourStr = (hour < 10) ? "0" + std::to_string(hour) : std::to_string(hour);
  std::string minStr = (minute < 10) ? "0" + std::to_string(minute) : std::to_string(minute);
  return String((hourStr + ":" + minStr + " " + period.c_str()).c_str());
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
  return (now - lastCalculation_) >= 86400/2; // 24 * 60 * 60 - twice a day
}