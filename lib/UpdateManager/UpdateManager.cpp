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

    install_requested_ = false;
    install_skip_filesystem_ = false;
    install_force_ = false;

    device_version_ = firmwareVersion;

    logger.logDebug("UpdateManager initialized with version: " + device_version_);
    logger.logDebug("Manifest URL: " + manifest_url_);
}

bool UpdateManager::parseVersion(const String& version, int& major, int& minor, int& patch) const {
    // Parse semantic version: "1.0.5" or "1.0.0-beta"
    // Extract major.minor.patch (ignore pre-release and build metadata)

    unsigned int dotCount = 0;
    int firstDot = -1;
    int secondDot = -1;

    for (unsigned int i = 0; i < version.length(); i++) {
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
    unsigned int patchEnd = secondDot + 1;
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

    if (manifest_url_.length() == 0) {
        setError(UpdateError::NET_ERROR, "No manifest URL configured");
        return;
    }

    setStatus(UpdateStatus::CHECKING, true);
    current_operation_start_ = hal_->millis();

    logger.logInfo("Checking for updates from: " + manifest_url_);

    // Fetch manifest JSON
    String manifestJson = hal_->httpGet(manifest_url_, 15000);
    if (manifestJson.length() == 0) {
        setError(UpdateError::NET_ERROR, "Failed to fetch manifest from: " + manifest_url_);
        last_check_time_ = hal_->millis();
        return;
    }

    // Parse manifest JSON
    JsonDocument doc;
    DeserializationError jsonError = deserializeJson(doc, manifestJson);
    if (jsonError) {
        setError(UpdateError::MANIFEST_PARSE, "Failed to parse manifest JSON: " + String(jsonError.c_str()));
        last_check_time_ = hal_->millis();
        return;
    }

    // Extract manifest data
    manifest_.latest_version = doc["latest_version"].as<String>();
    manifest_.release_date = doc["release_date"].as<String>();

    if (doc["firmware"].is<JsonObject>()) {
        manifest_.firmware.version = doc["firmware"]["version"].as<String>();
        manifest_.firmware.url = doc["firmware"]["url"].as<String>();
        manifest_.firmware.size_bytes = doc["firmware"]["size_bytes"].as<uint32_t>();
        manifest_.firmware.sha256 = doc["firmware"]["sha256"].as<String>();
    }

    if (doc["filesystem"].is<JsonObject>()) {
        manifest_.filesystem.version = doc["filesystem"]["version"].as<String>();
        manifest_.filesystem.url = doc["filesystem"]["url"].as<String>();
        manifest_.filesystem.size_bytes = doc["filesystem"]["size_bytes"].as<uint32_t>();
        manifest_.filesystem.sha256 = doc["filesystem"]["sha256"].as<String>();
    }

    last_check_time_ = hal_->millis();

    // Compare versions
    if (manifest_.latest_version.length() == 0) {
        setError(UpdateError::MANIFEST_PARSE, "Manifest missing latest_version field");
        return;
    }

    if (isVersionNewer(device_version_, manifest_.latest_version)) {
        setStatus(UpdateStatus::AVAILABLE, true);
        logger.logInfo("Update available: " + device_version_ + " -> " + manifest_.latest_version);
    } else {
        setStatus(UpdateStatus::CURRENT, true);
        logger.logInfo("Firmware is up to date: " + device_version_);
    }
}

void UpdateManager::installUpdate(bool skip_filesystem, bool force) {
    if (!hal_) {
        setError(UpdateError::NET_ERROR, "HAL not initialized");
        return;
    }

    if (force) {
        // Force mode: allow install if manifest has been fetched (AVAILABLE or CURRENT)
        if (status_ != UpdateStatus::AVAILABLE && status_ != UpdateStatus::CURRENT) {
            setError(UpdateError::MANIFEST_PARSE, "No manifest available - check for updates first");
            return;
        }
    } else {
        if (status_ != UpdateStatus::AVAILABLE) {
            setError(UpdateError::MANIFEST_PARSE, "No update available to install");
            return;
        }
    }

    // Backup settings to NVS before filesystem flash to preserve user configuration
    if (!skip_filesystem) {
        logger.logInfo("Backing up settings to NVS before filesystem update...");
        if (!settingsManager.backupToNVS()) {
            logger.logWarning("Failed to backup settings to NVS - settings may be lost after filesystem update");
        }
    }

    current_operation_start_ = hal_->millis();
    logger.logInfo("Starting firmware update to version: " + manifest_.latest_version);

    // --- Install firmware ---
    if (manifest_.firmware.url.length() > 0 && manifest_.firmware.size_bytes > 0) {
        phase_ = "firmware";
        setStatus(UpdateStatus::DOWNLOADING, true);
        bytes_downloaded_ = 0;
        total_bytes_ = manifest_.firmware.size_bytes;
        progress_percent_ = 0;

        logger.logInfo("Downloading firmware: " + String((unsigned long)manifest_.firmware.size_bytes) + " bytes");

        // Begin OTA firmware partition
        if (!hal_->otaBegin(manifest_.firmware.size_bytes, 0)) {
            setError(UpdateError::INSTALL_FAILED, "Failed to begin firmware OTA: " + hal_->otaGetError());
            return;
        }

        // Stream download, writing each chunk to OTA partition
        bool downloadSuccess = hal_->httpGetStream(manifest_.firmware.url,
            [this](const uint8_t* data, size_t len, uint32_t downloaded, uint32_t total) -> bool {
                size_t written = hal_->otaWrite(data, len);
                if (written != len) {
                    return false; // Write failed, abort download
                }
                bytes_downloaded_ = downloaded;
                total_bytes_ = total;
                progress_percent_ = total > 0 ? (uint8_t)((downloaded * 100) / total) : 0;
                return true;
            }, 120000); // 2 minute timeout for firmware download

        if (!downloadSuccess) {
            hal_->otaAbort();
            setError(UpdateError::DOWNLOAD_FAILED, "Firmware download failed or incomplete");
            return;
        }

        logger.logInfo("Firmware download complete: " + String((unsigned long)bytes_downloaded_) + " bytes, finalizing...");

        // Finalize firmware OTA
        setStatus(UpdateStatus::INSTALLING);
        if (!hal_->otaEnd(true)) {
            hal_->otaAbort();
            setError(UpdateError::INSTALL_FAILED, "Firmware install failed: " + hal_->otaGetError());
            return;
        }

        logger.logInfo("Firmware update installed successfully");
    }

    // --- Install filesystem (if not skipping) ---
    if (!skip_filesystem && manifest_.filesystem.url.length() > 0 && manifest_.filesystem.size_bytes > 0) {
        phase_ = "filesystem";
        bytes_downloaded_ = 0;
        total_bytes_ = manifest_.filesystem.size_bytes;
        progress_percent_ = 0;
        setStatus(UpdateStatus::DOWNLOADING, true);

        logger.logInfo("Downloading filesystem: " + String((unsigned long)manifest_.filesystem.size_bytes) + " bytes");

        // End current filesystem before flashing
        hal_->fsEnd();

        if (!hal_->otaBegin(manifest_.filesystem.size_bytes, 1)) {
            hal_->fsBegin();
            setError(UpdateError::INSTALL_FAILED, "Failed to begin filesystem OTA: " + hal_->otaGetError());
            return;
        }

        bool fsDownloadSuccess = hal_->httpGetStream(manifest_.filesystem.url,
            [this](const uint8_t* data, size_t len, uint32_t downloaded, uint32_t total) -> bool {
                size_t written = hal_->otaWrite(data, len);
                if (written != len) {
                    logger.logError("Filesystem OTA write failed: wrote " + String((unsigned long)written) + "/" + String((unsigned long)len) + " bytes");
                    return false;
                }
                bytes_downloaded_ = downloaded;
                total_bytes_ = total;
                progress_percent_ = total > 0 ? (uint8_t)((downloaded * 100) / total) : 0;
                return true;
            }, 120000);

        if (!fsDownloadSuccess) {
            hal_->otaAbort();
            hal_->fsBegin();
            setError(UpdateError::DOWNLOAD_FAILED, "Filesystem download failed or incomplete");
            return;
        }

        logger.logInfo("Filesystem download complete: " + String((unsigned long)bytes_downloaded_) + " bytes, finalizing...");

        setStatus(UpdateStatus::INSTALLING);
        if (!hal_->otaEnd(true)) {
            hal_->otaAbort();
            hal_->fsBegin();
            setError(UpdateError::INSTALL_FAILED, "Filesystem install failed: " + hal_->otaGetError());
            return;
        }

        logger.logInfo("Filesystem update installed successfully");
    }

    phase_ = "complete";
    setStatus(UpdateStatus::COMPLETE, true);
    progress_percent_ = 100;
    logger.logWarning("Update complete! Restarting device in 5 seconds...");
    delay(5000);  // Allow time for status poll to see COMPLETE
    hal_->restart();
}

void UpdateManager::requestInstall(bool skip_filesystem, bool force) {
    install_requested_ = true;
    install_skip_filesystem_ = skip_filesystem;
    install_force_ = force;
    logger.logInfo("Update install requested (deferred to main loop)");
}

void UpdateManager::update() {
    if (!hal_) return;
    if (status_ == UpdateStatus::DOWNLOADING || status_ == UpdateStatus::INSTALLING ||
        status_ == UpdateStatus::VERIFYING || status_ == UpdateStatus::CHECKING) {
        return; // Already busy
    }

    // Handle deferred install request from web handler
    if (install_requested_) {
        install_requested_ = false;
        installUpdate(install_skip_filesystem_, install_force_);
        return;
    }

    if (!settingsManager.getAutoUpdateEnabled()) return;

    unsigned long checkIntervalMs = (unsigned long)settingsManager.getUpdateCheckIntervalHours() * 3600000UL;
    unsigned long now = hal_->millis();

    if (last_check_time_ == 0 || (now - last_check_time_) >= checkIntervalMs) {
        checkForUpdates();
    }
}

UpdateStatusSnapshot UpdateManager::getStatus() const {
    UpdateStatusSnapshot snapshot;
    snapshot.status = status_;
    snapshot.error = error_;
    snapshot.progress_percent = progress_percent_;
    snapshot.bytes_downloaded = bytes_downloaded_;
    snapshot.total_bytes = total_bytes_;
    snapshot.error_message = last_error_message_;
    snapshot.phase = phase_;
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

    doc["release_date"] = manifest_.release_date;
    doc["github_repo"] = String(githubRepo);

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
    doc["phase"] = phase_;
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
    phase_ = "";
    progress_percent_ = 0;
    bytes_downloaded_ = 0;
    total_bytes_ = 0;
}
