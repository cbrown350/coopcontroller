#ifndef MOCK_BUZZERCONTROLLER_H
#define MOCK_BUZZERCONTROLLER_H

#include "BuzzerController.h"

/**
 * @brief Mock implementation of BuzzerController for testing
 *
 * Provides a test double for BuzzerController that tracks method calls
 * without activating actual hardware. Used in unit tests to verify
 * controller behavior without sound output.
 *
 * Features:
 * - Tracks triggered alerts
 * - Tracks cleared alerts
 * - Provides test helper methods
 * - Reset functionality for test isolation
 */
class MockBuzzerController : public BuzzerController {
public:
    MockBuzzerController() = default;
    virtual ~MockBuzzerController() = default;

    /**
     * @brief Mock trigger alert - records alert type
     *
     * @param type AlertType that was triggered
     */
    void triggerAlert(AlertType type) {
        lastTriggeredAlert = type;
        hasAlert = true;
        alertTriggered = true;
    }

    /**
     * @brief Mock clear alert - records cleared alert type
     *
     * @param type AlertType that was cleared
     */
    void clearAlert(AlertType type) {
        lastClearedAlert = type;
        if (type == lastTriggeredAlert) {
            hasAlert = false;
        }
    }

    // ========================================================================
    // TEST HELPER METHODS
    // ========================================================================

    /**
     * @brief Get last triggered alert type
     *
     * @return Last AlertType that was triggered
     */
    AlertType getLastTriggeredAlert() const {
        return lastTriggeredAlert;
    }

    /**
     * @brief Get last cleared alert type
     *
     * @return Last AlertType that was cleared
     */
    AlertType getLastClearedAlert() const {
        return lastClearedAlert;
    }

    /**
     * @brief Check if any alert was triggered
     *
     * @return true if triggerAlert() was called
     */
    bool wasAlertTriggered() const {
        return alertTriggered;
    }

    /**
     * @brief Reset mock state
     *
     * Clears all tracking variables for test isolation.
     */
    void reset() {
        lastTriggeredAlert = static_cast<AlertType>(255); // Invalid value to indicate "no alert"
        lastClearedAlert = static_cast<AlertType>(255);
        hasAlert = false;
        alertTriggered = false;
    }

private:
    AlertType lastTriggeredAlert = static_cast<AlertType>(255); ///< Last alert triggered
    AlertType lastClearedAlert = static_cast<AlertType>(255);    ///< Last alert cleared
    bool hasAlert = false;                                       ///< Currently has active alert
    bool alertTriggered = false;                                  ///< Alert was triggered flag
};

#endif // MOCK_BUZZERCONTROLLER_H
