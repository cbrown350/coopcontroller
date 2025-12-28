#include <gtest/gtest.h>
#include "BuzzerController.h"

class BuzzerControllerTest : public ::testing::Test {
protected:
    BuzzerController buzzer;
};

TEST_F(BuzzerControllerTest, DisabledAlertDoesNotActivate) {
    buzzer.setEnabled(false);
    buzzer.triggerAlert(AlertType::SENSOR_ERROR);

    EXPECT_FALSE(buzzer.hasActiveAlert());
}

TEST_F(BuzzerControllerTest, LowerPriorityAlertIsBlocked) {
    buzzer.triggerAlert(AlertType::PUMP_ERROR);           // higher priority (lower enum value)
    buzzer.triggerAlert(AlertType::SYSTEM_ERROR);         // lower priority (higher enum value)

    EXPECT_TRUE(buzzer.hasActiveAlert());
    EXPECT_EQ(AlertType::PUMP_ERROR, buzzer.getCurrentAlertType());
}

TEST_F(BuzzerControllerTest, ClearAlertResetsState) {
    buzzer.triggerAlert(AlertType::SENSOR_ERROR);
    buzzer.clearAlert(AlertType::SENSOR_ERROR);

    EXPECT_FALSE(buzzer.hasActiveAlert());
}