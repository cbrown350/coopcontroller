#include "BuzzerController.h"


BuzzerController::BuzzerController(int pin)
    : _pin(pin) {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH); //active low buzzer off
}

void BuzzerController::begin() {
}

void BuzzerController::buzz(int frequency, int duration) {
    tone(_pin, frequency, duration);
}

void BuzzerController::stop() {
    noTone(_pin);
}
