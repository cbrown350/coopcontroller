#ifndef MOCK_SUNRISESUNSETCALCULATOR_H
#define MOCK_SUNRISESUNSETCALCULATOR_H

#include "SunriseSunset.h"

// Mock SunriseSunsetCalculator that uses default times (6 AM and 6 PM)
// and prevents actual time calculations that would require HAL
class MockSunriseSunsetCalculator : public SunriseSunsetCalculator {
public:
    MockSunriseSunsetCalculator() = default;

    // Prevent update methods from making HAL calls
    void update() {
        // Mock implementation - does nothing, uses default times
    }

    void forceUpdate() {
        // Mock implementation - does nothing, uses default times
    }

    // Note: getSunriseMinutes() and getSunsetMinutes() are not virtual,
    // so they will return the base class default values (360 = 6 AM, 1080 = 6 PM)
};

#endif // MOCK_SUNRISESUNSETCALCULATOR_H
