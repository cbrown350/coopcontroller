#ifndef MOCK_BUZZERCONTROLLER_H
#define MOCK_BUZZERCONTROLLER_H

#include "BuzzerController.h"

// Mock BuzzerController that tracks alert calls without making sounds
class MockBuzzerController : public BuzzerController {
public:
    MockBuzzerController() = default;

    void triggerAlert(AlertType type) {
        lastTriggeredAlert = type;
        hasAlert = true;
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

    void reset() {
        lastTriggeredAlert = AlertType::SYSTEM_ERROR;
        lastClearedAlert = AlertType::SYSTEM_ERROR;
        hasAlert = false;
    }

private:
    AlertType lastTriggeredAlert = AlertType::SYSTEM_ERROR;
    AlertType lastClearedAlert = AlertType::SYSTEM_ERROR;
    bool hasAlert = false;
};

#endif // MOCK_BUZZERCONTROLLER_H
