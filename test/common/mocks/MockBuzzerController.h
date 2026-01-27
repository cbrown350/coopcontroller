#ifndef MOCK_BUZZERCONTROLLER_H
#define MOCK_BUZZERCONTROLLER_H

#include "BuzzerController.h"

// Mock BuzzerController that tracks alert calls without making sounds
class MockBuzzerController : public BuzzerController {
public:
    MockBuzzerController() = default;
    virtual ~MockBuzzerController() = default;

    void triggerAlert(AlertType type) {
        lastTriggeredAlert = type;
        hasAlert = true;
        alertTriggered = true;
    }

    void clearAlert(AlertType type) {
        lastClearedAlert = type;
        if (type == lastTriggeredAlert) {
            hasAlert = false;
        }
    }

    // Test helper methods
    AlertType getLastTriggeredAlert() const {
        return lastTriggeredAlert;
    }

    AlertType getLastClearedAlert() const {
        return lastClearedAlert;
    }

    bool wasAlertTriggered() const {
        return alertTriggered;
    }

    void reset() {
        lastTriggeredAlert = static_cast<AlertType>(255); // Invalid value to indicate "no alert"
        lastClearedAlert = static_cast<AlertType>(255);
        hasAlert = false;
        alertTriggered = false;
    }

private:
    AlertType lastTriggeredAlert = static_cast<AlertType>(255); // Invalid value
    AlertType lastClearedAlert = static_cast<AlertType>(255);
    bool hasAlert = false;
    bool alertTriggered = false;
};

#endif // MOCK_BUZZERCONTROLLER_H
