#ifndef __UPDATE_MANAGER_H__
#define __UPDATE_MANAGER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IHAL.h"

/**
 * @brief Update status enumeration
 *
 * Represents the current state of the update system.
 */
enum class UpdateStatus {
    IDLE,              ///< No update in progress
    CHECKING,          ///< Checking for available updates
    AVAILABLE,         ///< Update available but not downloading
    CURRENT,           ///< Already up to date
    DOWNLOADING,       ///< Downloading update
    VERIFYING,         ///< Verifying checksum
    INSTALLING,        ///< Installing update
    COMPLETE,          ///< Update completed successfully
    ERROR              ///< Error occurred
};

/**
 * @brief Update error codes
 *
 * Specific error reasons for update failures.
 */
enum class UpdateError {
    NONE,              ///< No error
    NET_ERROR,         ///< Network/WiFi error
    MANIFEST_PARSE,    ///< Failed to parse manifest JSON
    DOWNLOAD_FAILED,   ///< Download failed or interrupted
    CHECKSUM_MISMATCH, ///< Downloaded file checksum doesn't match
    INSUFFICIENT_SPACE,///< Not enough space for update
    INSTALL_FAILED,    ///< Update installation failed
    VERSION_PARSE      ///< Failed to parse version string
};

/**
 * @brief Version information structure
 *
 * Holds information about a specific version.
 */
struct VersionInfo {
    String version;        ///< Version string (e.g., "1.0.0")
    String url;            ///< Download URL
    uint32_t size_bytes;   ///< File size in bytes
    String sha256;         ///< SHA256 checksum
};

/**
 * @brief Update manifest structure
 *
 * Information about available firmware and filesystem updates.
 */
struct UpdateManifest {
    String latest_version;
    VersionInfo firmware;
    VersionInfo filesystem;
    String release_date;
    String changelog;
};

/**
 * @brief Update status snapshot
 *
 * Complete status information for API responses.
 */
struct UpdateStatusSnapshot {
    UpdateStatus status;
    UpdateError error;
    uint8_t progress_percent;
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    String error_message;
    String phase;              ///< Current phase: "firmware", "filesystem", or ""
    unsigned long last_check_time;
    unsigned long next_check_time;
};

/**
 * @brief Over-The-Air firmware update manager
 *
 * Manages firmware and filesystem updates from GitHub releases.
 * Handles manifest checking, downloading, verification, and installation.
 *
 * Features:
 * - Semantic version comparison (1.0.0, 1.0.1, 1.1.0, 2.0.0)
 * - Manifest parsing from version_manifest.json
 * - SHA256 checksum verification
 * - Configurable update intervals
 * - Automatic or manual update triggering
 * - Progress reporting via REST API
 * - Full HAL abstraction for testability
 */
class UpdateManager {
private:
    IHAL* hal_;
    UpdateStatus status_;
    UpdateError error_;
    UpdateManifest manifest_;
    String device_version_;
    String manifest_url_;

    // Progress tracking
    uint32_t bytes_downloaded_;
    uint32_t total_bytes_;
    uint8_t progress_percent_;

    // Timing
    unsigned long last_check_time_;
    unsigned long current_operation_start_;

    // Error tracking
    String last_error_message_;

    // Phase tracking
    String phase_;

    // Deferred install request (set by web handler, executed by main loop)
    bool install_requested_;
    bool install_skip_filesystem_;
    bool install_force_;

    /**
     * @brief Compare two semantic versions
     *
     * @param current Current version string (e.g., "1.0.0")
     * @param available Available version string
     * @return true if available > current, false otherwise
     */
    bool isVersionNewer(const String& current, const String& available) const;

    /**
     * @brief Parse semantic version into components
     *
     * @param version Version string (e.g., "1.0.0")
     * @param major Output: major version number
     * @param minor Output: minor version number
     * @param patch Output: patch version number
     * @return true if parse successful, false if invalid format
     */
    bool parseVersion(const String& version, int& major, int& minor, int& patch) const;

    /**
     * @brief Set status and optionally clear error
     *
     * @param newStatus New status to set
     * @param clearError Whether to clear error state
     */
    void setStatus(UpdateStatus newStatus, bool clearError = false);

    /**
     * @brief Set error state
     *
     * @param error Error code
     * @param message Error message for logging
     */
    void setError(UpdateError error, const String& message = "");

public:
    /**
     * @brief Default constructor
     *
     * Initializes update manager in idle state.
     * Must call begin() before use.
     */
    UpdateManager() = default;

    /**
     * @brief Initialize update manager
     *
     * @param hal Pointer to hardware abstraction layer
     * @param manifest_url URL to version_manifest.json
     */
    void begin(IHAL* hal, const String& manifest_url);

    /**
     * @brief Check for available updates
     *
     * Fetches and parses manifest, compares versions.
     * Updates last_check_time in SettingsManager.
     */
    void checkForUpdates();

    /**
     * @brief Download and install update
     *
     * Downloads firmware and/or filesystem, verifies checksums,
     * and installs via ESP.Update mechanism.
     *
     * @param skip_filesystem If true, only update firmware
     */
    void installUpdate(bool skip_filesystem = false, bool force = false);

    /**
     * @brief Request deferred install (called from web handler)
     *
     * Sets a flag so installUpdate() runs from the main loop via update(),
     * allowing the web server to respond to status poll requests during download.
     */
    void requestInstall(bool skip_filesystem = false, bool force = false);

    /**
     * @brief Periodic update check and deferred install for main loop
     *
     * Call from main loop. Checks for updates automatically based on
     * auto_update_enabled setting and check interval.
     * Also executes deferred install requests.
     */
    void update();

    /**
     * @brief Get current update status
     *
     * @return Current UpdateStatusSnapshot
     */
    UpdateStatusSnapshot getStatus() const;

    /**
     * @brief Get current device firmware version
     *
     * Reads FIRMWARE_VERSION build flag.
     *
     * @return Version string (e.g., "1.0.0")
     */
    String getDeviceVersion() const;

    /**
     * @brief Get latest available version
     *
     * @return Version string from manifest, or empty if not checked
     */
    String getLatestVersion() const;

    /**
     * @brief Check if update is available
     *
     * @return true if newer version available, false otherwise
     */
    bool isUpdateAvailable() const;

    /**
     * @brief Get manifest information
     *
     * @return Current UpdateManifest
     */
    UpdateManifest getManifest() const { return manifest_; }

    /**
     * @brief Get status as JSON
     *
     * For REST API response serialization.
     *
     * @param json JsonObject to populate
     */
    void toJson(JsonObject& json) const;

    /**
     * @brief Get check response as JSON
     *
     * For /update/check endpoint.
     *
     * @return JsonDocument with update check results
     */
    JsonDocument getCheckResponseJson() const;

    /**
     * @brief Get status response as JSON
     *
     * For /update/status endpoint.
     *
     * @return JsonDocument with current status
     */
    JsonDocument getStatusResponseJson() const;

    /**
     * @brief Abort current operation
     *
     * Cancels any in-progress download or installation.
     */
    void abort();

    /**
     * @brief Reset status to idle
     *
     * Clears error state and returns to idle.
     */
    void reset();

    /**
     * @brief Set device version for testing
     * @param version Version string to use instead of firmwareVersion
     */
    void setDeviceVersionForTesting(const String& version) { device_version_ = version; }
};

#endif // __UPDATE_MANAGER_H__
