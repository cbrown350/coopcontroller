#ifndef SUNRISE_SUNSET_H
#define SUNRISE_SUNSET_H

#include <Arduino.h>
#include <SolarCalculator.h>
#include "IHAL.h"

/**
 * @brief Sunrise and sunset time calculator
 *
 * Calculates sunrise and sunset times for a given geographic location
 * and timezone. Uses astronomical algorithms for accuracy.
 *
 * Features:
 * - Daily automatic recalculation (every 24 hours)
 * - Configurable location (latitude, longitude)
 * - Timezone offset support
 * - Returns times in minutes since midnight
 * - Formatted time strings available
 *
 * Usage:
 * - Initialize with location coordinates
 * - Call update() daily (checks 24hr interval internally)
 * - Use getSunriseMinutes()/getSunsetMinutes() for scheduling
 * - Call forceUpdate() after location changes
 *
 * Time Format:
 * - Internal: Minutes since midnight (0-1439)
 * - Display: "HH:MM AM/PM" format
 */
class SunriseSunsetCalculator {
private:
  IHAL* hal_ = nullptr;        ///< Hardware abstraction layer for time
  double latitude_;            ///< Geographic latitude (decimal degrees)
  double longitude_;           ///< Geographic longitude (decimal degrees)
  int utcOffset_;              ///< UTC timezone offset in hours
  time_t lastCalculation_;     ///< Timestamp of last calculation
  int sunriseMinutes_;         ///< Sunrise time in minutes since midnight
  int sunsetMinutes_;          ///< Sunset time in minutes since midnight

public:
  /**
   * @brief Default constructor
   *
   * Initializes calculator with default values.
   * Must call begin() before use.
   */
  SunriseSunsetCalculator();

  /**
   * @brief Initialize calculator with location
   *
   * Sets geographic coordinates and timezone offset.
   *
   * @param hal Pointer to hardware abstraction layer
   * @param lat Latitude in decimal degrees (positive = North)
   * @param lon Longitude in decimal degrees (positive = East)
   * @param utcOffset UTC offset in hours (e.g., -5 for EST)
   */
  void begin(IHAL* hal, double lat, double lon, int utcOffset = 0);

  /**
   * @brief Update sunrise/sunset calculation
   *
   * Recalculates times if 24 hours have passed since last calculation.
   * Safe to call frequently - checks internal timer.
   */
  void update();

  /**
   * @brief Force immediate recalculation
   *
   * Recalculates sunrise/sunset regardless of time since last calculation.
   * Use after coordinate changes or system initialization.
   */
  void forceUpdate();

  /**
   * @brief Update location coordinates
   *
   * Changes the geographic location and forces recalculation.
   *
   * @param lat New latitude in decimal degrees
   * @param lon New longitude in decimal degrees
   * @param utcOffset New UTC offset in hours
   */
  void setCoordinates(double lat, double lon, int utcOffset = 0);

  /**
   * @brief Get sunrise time in minutes
   *
   * @return Minutes since midnight (e.g., 360 = 6:00 AM)
   */
  int getSunriseMinutes() const;

  /**
   * @brief Get sunset time in minutes
   *
   * @return Minutes since midnight (e.g., 1140 = 7:00 PM)
   */
  int getSunsetMinutes() const;

  /**
   * @brief Get sunrise time as formatted string
   *
   * @return String in "HH:MM AM/PM" format
   */
  String getSunriseTime() const;

  /**
   * @brief Get sunset time as formatted string
   *
   * @return String in "HH:MM AM/PM" format
   */
  String getSunsetTime() const;

  /**
   * @brief Check if recalculation is needed
   *
   * @return true if 24 hours have passed since last calculation
   */
  bool shouldCalculate() const;
};

#endif // SUNRISE_SUNSET_H