#ifndef SUNRISE_SUNSET_H
#define SUNRISE_SUNSET_H

#include <Arduino.h>
#include <SolarCalculator.h>

class SunriseSunsetCalculator {
private:
  double latitude_;
  double longitude_;
  int utcOffset_;
  time_t lastCalculation_;
  int sunriseMinutes_; // Minutes since midnight
  int sunsetMinutes_;  // Minutes since midnight

public:
  SunriseSunsetCalculator();
  void begin(double lat, double lon, int utcOffset);
  void update(); // Call daily to recalculate
  void forceUpdate(); // Force immediate recalculation (for initialization or settings changes)
  int getSunriseMinutes() const;
  int getSunsetMinutes() const;
  String getSunriseTime() const; // Format: "HH:MM AM/PM"
  String getSunsetTime() const;
  bool shouldCalculate() const; // Check if 24hrs passed
};

#endif // SUNRISE_SUNSET_H