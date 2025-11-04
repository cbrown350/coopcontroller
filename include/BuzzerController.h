#ifndef __BUZZER_CONTROLLER_H__
#define __BUZZER_CONTROLLER_H__

#include <Arduino.h>

class BuzzerController {
public:
    BuzzerController(int pin = BUZZER_B_PIN);
    void begin();
    void buzz(int frequency, int duration);
    void stop();

private:
    int _pin;
};

#endif // __BUZZER_CONTROLLER_H__