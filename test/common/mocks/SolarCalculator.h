#ifndef SOLAR_CALCULATOR_MOCK_H
#define SOLAR_CALCULATOR_MOCK_H

// Mock SolarCalculator library for desktop unit testing
// Based on jpb10/SolarCalculator 2.0.2

#include <cmath>

// namespace solarcalculator {

// Standard altitudes for sunrise/sunset calculations
constexpr double SUNRISESET_STD_ALTITUDE = -0.8333;
constexpr double CIVIL_DAWN_DUSK_STD_ALTITUDE = -6.0;
constexpr double NAUTICAL_DAWN_DUSK_STD_ALTITUDE = -12.0;
constexpr double ASTRONOMICAL_DAWN_DUSK_STD_ALTITUDE = -18.0;

// Julian Day structure
struct JulianDay {
    double JD;  // Julian day at 0h UT (JD ending in .5)
    double m;    // Fractional day, 0h to 24h (decimal number between 0 and 1)
    
    explicit JulianDay(unsigned long utc) : JD(utc / 86400.0 + 2440587.5), m(0.0) {}
    JulianDay(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);
};

// Utility functions
double wrapTo360(double angle);
double wrapTo180(double angle);

// Julian Day calculation - takes year, month, day, hour, minute, second
inline double calcJulianDay(int year, int month, int day, int hour, int minute, int second) {
    // Simplified Julian Day calculation for testing
    // Formula: JD = INT(365.25*(Y+4716.6)) + INT(30.6001*(M+0.5)) + D + B - 1524.5
    // where Y=year, M=month, D=day, B=0 for Jan/Feb, B=-0.5 for Jan/Feb of leap year
    
    // For simplicity in tests, just return a reasonable value
    // Real implementation would use proper astronomical algorithms
    return 2451545.0 + (year - 2000) * 365.25; // Approximate Julian Day for year 2000
}

// Julian centuries
double calcJulianCent(JulianDay jd);

// Solar coordinates
double calcGeomMeanLongSun(double T);
double calcGeomMeanAnomalySun(double T);
double calcSunEqOfCenter(double T);
double calcSunRadVector(double T);
double calcMeanObliquityOfEcliptic(double T);
void calcSolarCoordinates(double T, double& ra, double& dec);

// Sidereal time at Greenwich
double calcGrMeanSiderealTime(JulianDay jd);

// Sun's position in sky
void equatorial2horizontal(double H, double dec, double lat, double& az, double& el);
double calcHourAngleRiseSet(double dec, double lat, double h0);
double calcRefraction(double el);

// Equation of time, in minutes of time
void calcEquationOfTime(JulianDay jd, double& E);

// Sun's geocentric (as seen from the center of Earth) equatorial coordinates, in degrees and AUs
void calcEquatorialCoordinates(JulianDay jd, double& rt_ascension, double& declination, double& radius_vector);

// Sun's topocentric (as seen from the observer's place on Earth's surface) horizontal coordinates, in degrees
void calcHorizontalCoordinates(JulianDay jd, double latitude, double longitude, double& azimuth, double& elevation);

// Find times of sunrise, transit, and sunset, in hours
void calcSunriseSunset(JulianDay jd, double latitude, double longitude,
                        double& transit, double& sunrise, double& sunset,
                        double altitude = SUNRISESET_STD_ALTITUDE, int iterations = 1);

// Wrapper functions - All calculations assume time inputs in Coordinated Universal Time (UTC)
void calcEquationOfTime(unsigned long utc, double& E);
void calcEquationOfTime(int year, int month, int day, int hour, int minute, int second, double& E);

void calcEquatorialCoordinates(unsigned long utc, double& rt_ascension, double& declination, double& radius_vector);
void calcEquatorialCoordinates(int year, int month, int day, int hour, int minute, int second,
                                double& rt_ascension, double& declination, double& radius_vector);

void calcHorizontalCoordinates(unsigned long utc, double latitude, double longitude,
                                double& azimuth, double& elevation);
void calcHorizontalCoordinates(int year, int month, int day, int hour, int minute, int second,
                                double latitude, double longitude, double& azimuth, double& elevation);

void calcSunriseSunset(unsigned long utc, double latitude, double longitude,
                        double& transit, double& sunrise, double& sunset,
                        double altitude = SUNRISESET_STD_ALTITUDE, int iterations = 1);
void calcSunriseSunset(int year, int month, int day, double latitude, double longitude,
                        double& transit, double& sunrise, double& sunset,
                        double altitude = SUNRISESET_STD_ALTITUDE, int iterations = 1);

void calcCivilDawnDusk(unsigned long utc, double latitude, double longitude,
                        double& transit, double& dawn, double& dusk);
void calcCivilDawnDusk(int year, int month, int day, double latitude, double longitude,
                        double& transit, double& dawn, double& dusk);

void calcNauticalDawnDusk(unsigned long utc, double latitude, double longitude,
                           double& transit, double& dawn, double& dusk);
void calcNauticalDawnDusk(int year, int month, int day, double latitude, double longitude,
                           double& transit, double& dawn, double& dusk);

void calcAstronomicalDawnDusk(unsigned long utc, double latitude, double longitude,
                               double& transit, double& dawn, double& dusk);
void calcAstronomicalDawnDusk(int year, int month, int day, double latitude, double longitude,
                               double& transit, double& dawn, double& dusk);

// } // namespace solarcalculator

#endif // SOLAR_CALCULATOR_MOCK_H
