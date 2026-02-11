#include "UpdateManager.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Update.h>

void UpdateManager::begin(IHAL* hal, const String& manifest_url) {
    hal_ = hal;

    // If manifest_url is provided, use it; otherwise construct from GITHUB_REPO
    if (manifest_url.length() > 0) {
        manifest_url_ = manifest_url;
    } else {
        String repo = githubRepo;
        if (repo.length() > 0) {
            manifest_url_ = "https://github.com/" + repo + "/releases/latest/download/version_manifest.json";
        }
    }

    status_ = UpdateStatus::IDLE;
    error_ = UpdateError::NONE;
    bytes_downloaded_ = 0;
    total_bytes_ = 0;
    progress_percent_ = 0;
    last_check_time_ = 0;
    current_operation_start_ = 0;
    last_error_message_ = "";

    device_version_ = firmwareVersion;

    logger.logDebug("UpdateManager initialized with version: " + device_version_);
    logger.logDebug("Manifest URL: " + manifest_url_);
}

bool UpdateManager::parseVersion(const String& version, int& major, int& minor, int& patch) const {
    // Parse semantic version: "1.0.5" or "1.0.0-beta"
    // Extract major.minor.patch (ignore pre-release and build metadata)

    int dotCount = 0;
    int firstDot = -1;
    int secondDot = -1;

    for (int i = 0; i < version.length(); i++) {
        if (version[i] == '.') {
            dotCount++;
            if (dotCount == 1) firstDot = i;
            else if (dotCount == 2) secondDot = i;
        } else if (version[i] == '-' || version[i] == '+') {
            // Found pre-release or build metadata
            if (dotCount < 2) return false; // Invalid format
            break;
        }
    }

    if (dotCount < 2) return false; // Need at least major.minor.patch

    major = version.substring(0, firstDot).toInt();
    minor = version.substring(firstDot + 1, secondDot).toInt();

    // Find end of patch (stop at '-', '+', or end of string)
    int patchEnd = secondDot + 1;
    while (patchEnd < version.length() && version[patchEnd] >= '0' && version[patchEnd] <= '9') {
        patchEnd++;
    }
    patch = version.substring(secondDot + 1, patchEnd).toInt();

    return true;
}

bool UpdateManager::isVersionNewer(const String& current, const String& available) const {
    int currMajor, currMinor, currPatch;
    int availMajor, availMinor, availPatch;

    if (!parseVersion(current, currMajor, currMinor, currPatch)) {
        logger.logError("Failed to parse current version: " + current);
        return false;
    }

    if (!parseVersion(available, availMajor, availMinor, availPatch)) {
        logger.logError("Failed to parse available version: " + available);
        return false;
    }

    if (availMajor != currMajor) return availMajor > currMajor;
    if (availMinor != currMinor) return availMinor > currMinor;
    return availPatch > currPatch;
}

void UpdateManager::setStatus(UpdateStatus newStatus, bool clearError) {
    status_ = newStatus;
    if (clearError) {
        error_ = UpdateError::NONE;
        last_error_message_ = "";
    }
}

void UpdateManager::setError(UpdateError error, const String& message) {
    error_ = error;
    last_error_message_ = message;
    status_ = UpdateStatus::ERROR;
    logger.logError("Update error: " + message);
}

void UpdateManager::checkForUpdates() {
    if (!hal_) {
        setError(UpdateError::NET_ERROR, "HAL not initialized");
        return;
    }

    setStatus(UpdateStatus::CHECKING, true);
    current_operation_start_ = hal_->millis();

    logger.logInfo("Checking for updates from: " + manifest_url_);

    // TODO: Implement HTTP GET request to fetch manifest
    // This will require HAL HTTP client methods or AsyncWebServer client
    // For now, set error to indicate not implemented
    setError(UpdateError::NET_ERROR, "Update checking not yet implemented");

    // Update last check time in settings
    last_check_time_ = hal_->millis();
}

void UpdateManager::installUpdate(bool skip_filesystem) {
    if (!hal_) {
        setError(UpdateError::NET_ERROR, "HAL not initialized");
        return;
    }

    if (status_ != UpdateStatus::AVAILABLE) {
        setError(UpdateError::MANIFEST_PARSE, "No update available to install");
        return;
    }

    setStatus(UpdateStatus::DOWNLOADING, true);
    current_operation_start_ = hal_->millis();

    logger.logInfo("Starting firmware update to version: " + manifest_.latest_version);

    // TODO: Implement download and installation
    // Use ESP.Update or similar for firmware installation
    setError(UpdateError::NET_ERROR, "Update installation not yet implemented");
}

UpdateStatusSnapshot UpdateManager::getStatus() const {
    UpdateStatusSnapshot snapshot;
    snapshot.status = status_;
    snapshot.error = error_;
    snapshot.progress_percent = progress_percent_;
    snapshot.bytes_downloaded = bytes_downloaded_;
    snapshot.total_bytes = total_bytes_;
    snapshot.error_message = last_error_message_;
    snapshot.last_check_time = last_check_time_;
    snapshot.next_check_time = 0; // TODO: Calculate based on settings

    return snapshot;
}

String UpdateManager::getDeviceVersion() const {
    return device_version_;
}

String UpdateManager::getLatestVersion() const {
    return manifest_.latest_version;
}

bool UpdateManager::isUpdateAvailable() const {
    if (manifest_.latest_version.length() == 0) {
        return false;
    }
    return isVersionNewer(device_version_, manifest_.latest_version);
}

void UpdateManager::toJson(JsonObject& json) const {
    json["status"] = (int)status_;
    json["error"] = (int)error_;
    json["error_message"] = last_error_message_;
    json["progress_percent"] = progress_percent_;
    json["bytes_downloaded"] = bytes_downloaded_;
    json["total_bytes"] = total_bytes_;
}

JsonDocument UpdateManager::getCheckResponseJson() const {
    JsonDocument doc;

    doc["manifest_url"] = manifest_url_;
    doc["last_check_time"] = last_check_time_;
    doc["current_version"] = device_version_;
    doc["available_version"] = manifest_.latest_version;
    doc["update_available"] = isUpdateAvailable();

    JsonObject firmware = doc["firmware"].to<JsonObject>();
    firmware["version"] = manifest_.firmware.version;
    firmware["url"] = manifest_.firmware.url;
    firmware["size_bytes"] = manifest_.firmware.size_bytes;

    JsonObject filesystem = doc["filesystem"].to<JsonObject>();
    filesystem["version"] = manifest_.filesystem.version;
    filesystem["url"] = manifest_.filesystem.url;
    filesystem["size_bytes"] = manifest_.filesystem.size_bytes;

    return doc;
}

JsonDocument UpdateManager::getStatusResponseJson() const {
    JsonDocument doc;

    String statusStr;
    switch (status_) {
        case UpdateStatus::IDLE: statusStr = "idle"; break;
        case UpdateStatus::CHECKING: statusStr = "checking"; break;
        case UpdateStatus::AVAILABLE: statusStr = "available"; break;
        case UpdateStatus::CURRENT: statusStr = "current"; break;
        case UpdateStatus::DOWNLOADING: statusStr = "downloading"; break;
        case UpdateStatus::VERIFYING: statusStr = "verifying"; break;
        case UpdateStatus::INSTALLING: statusStr = "installing"; break;
        case UpdateStatus::COMPLETE: statusStr = "complete"; break;
        case UpdateStatus::ERROR: statusStr = "error"; break;
        default: statusStr = "unknown"; break;
    }

    doc["status"] = statusStr;
    doc["progress"] = progress_percent_;
    doc["last_check"] = last_check_time_;
    doc["error"] = last_error_message_;

    return doc;
}

void UpdateManager::abort() {
    if (status_ == UpdateStatus::DOWNLOADING || status_ == UpdateStatus::INSTALLING) {
        setStatus(UpdateStatus::IDLE, true);
        logger.logWarning("Update operation aborted");
    }
}

void UpdateManager::reset() {
    status_ = UpdateStatus::IDLE;
    error_ = UpdateError::NONE;
    last_error_message_ = "";
    progress_percent_ = 0;
    bytes_downloaded_ = 0;
    total_bytes_ = 0;
}
