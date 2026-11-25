#include "UpdateManager.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "config.h"
#include "CrashBreadcrumbs.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/sha256.h>

// Defined in HAL_ESP32.cpp. Empty on desktop test builds (no HAL_ESP32), so the
// linkage only resolves on the ESP32 target. On desktop, UpdateManager links
// against a mock HAL that never calls httpGet, so the symbol is absent — guard
// use with #ifdef ESP_PLATFORM.
#ifdef ESP_PLATFORM
extern String g_http_get_diag;
#endif

void UpdateManager::begin(IHAL* hal, const String& manifest_url) {
    hal_ = hal;

    setManifestUrl(manifest_url);

    status_ = UpdateStatus::IDLE;
    error_ = UpdateError::NONE;
    bytes_downloaded_ = 0;
    total_bytes_ = 0;
    progress_percent_ = 0;
    last_check_time_ = 0;
    current_operation_start_ = 0;
    last_error_message_ = "";

    check_requested_ = false;
    install_requested_ = false;
    install_skip_filesystem_ = false;
    install_force_ = false;

    sha_ctx_ = nullptr;
    sha_ctx_initialized_ = false;

    device_version_ = firmwareVersion;

    logger.logDebug("UpdateManager initialized with version: " + device_version_);
    logger.logDebug("Manifest URL: " + manifest_url_);
}

void UpdateManager::setManifestUrl(const String& manifest_url) {
    // Use the provided URL when set; otherwise build the default from the
    // compiled-in repo.
    //
    // Default is the tag-independent release-asset manifest:
    //   https://github.com/<repo>/releases/latest/download/version_manifest.json
    // which 302-redirects (2 hops, via github.com -> release-assets.
    // githubusercontent.com / Azure blob) to the small (~900 byte) project
    // version_manifest.json published by the release workflow.
    //
    // Why not api.github.com/repos/<repo>/releases/latest (the previous
    // default): on the pioarduino 3.x / IDF 5.5.4 / mbedtls 3.x stack that
    // body (~14 KB) stalls mid-transfer — the board reads ~7-9 KB then
    // available() reports 0 permanently and the manifest parse fails. The
    // stall is specific to api.github.com's serving behavior with this lwIP
    // stack; raw.githubusercontent.com and release-assets.githubusercontent.com
    // (Fastly / Azure) do not exhibit it. The release-asset manifest is served
    // from the latter, so it reads reliably. The GitHub API URL still works
    // if a user configures it explicitly (manifest_url setting).
    if (manifest_url.length() > 0) {
        manifest_url_ = manifest_url;
    } else {
        String repo = githubRepo;
        manifest_url_ = repo.length() > 0
            ? "https://github.com/" + repo + "/releases/latest/download/version_manifest.json"
            : "";
    }
}

bool UpdateManager::parseVersion(const String& version, int& major, int& minor, int& patch) const {
    // Parse semantic version: "1.0.5", "v1.0.5", or "1.0.0-beta"
    // Extract major.minor.patch (ignore pre-release and build metadata)
    String v = version;
    v.trim();
    if (v.startsWith("v") || v.startsWith("V")) {
        v = v.substring(1);
    }

    unsigned int dotCount = 0;
    int firstDot = -1;
    int secondDot = -1;
    unsigned int coreEnd = v.length();

    for (unsigned int i = 0; i < v.length(); i++) {
        char c = v[i];
        if (c == '.') {
            dotCount++;
            if (dotCount == 1) firstDot = i;
            else if (dotCount == 2) secondDot = i;
            else return false;
        } else if (c == '-' || c == '+') {
            if (dotCount < 2) return false;
            coreEnd = i;
            break;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }

    if (dotCount != 2 || firstDot <= 0 || secondDot <= firstDot + 1 ||
        static_cast<unsigned int>(secondDot + 1) >= coreEnd) {
        return false;
    }

    major = v.substring(0, firstDot).toInt();
    minor = v.substring(firstDot + 1, secondDot).toInt();
    patch = v.substring(secondDot + 1, coreEnd).toInt();

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
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = newStatus;
    if (clearError) {
        error_ = UpdateError::NONE;
        last_error_message_ = "";
    }
}

void UpdateManager::setError(UpdateError error, const String& message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = error;
        last_error_message_ = message;
        status_ = UpdateStatus::ERROR;
    }
    logger.logError("Update error: " + message);
}

void UpdateManager::checkForUpdates() {
    if (!hal_) {
        setError(UpdateError::NET_ERROR, "HAL not initialized");
        return;
    }

    String manifestUrl;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifestUrl = manifest_url_;
    }

    if (manifestUrl.length() == 0) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_check_time_ = hal_->millis();
        }
        setError(UpdateError::NET_ERROR, "No manifest URL configured");
        return;
    }

    setStatus(UpdateStatus::CHECKING, true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_operation_start_ = hal_->millis();
        last_check_ok_ = false;
    }

    crashBreadcrumbRecord(CRUMB_UPDATE_START, hal_->getFreeHeap(), hal_->getMaxAllocHeap());

    logger.logInfo("Checking for updates from: " + manifestUrl);

    // Fetch manifest JSON (no mutex held across network I/O)
    crashBreadcrumbRecord(CRUMB_UPDATE_HTTP_BEFORE, hal_->getFreeHeap(), hal_->getMaxAllocHeap());
    String manifestJson = hal_->httpGet(manifestUrl, 15000);
    crashBreadcrumbRecord(CRUMB_UPDATE_HTTP_AFTER, (uint32_t)manifestJson.length(), hal_->getFreeHeap());
    if (manifestJson.length() == 0) {
        String halErr = hal_->getLastHttpError();
        String msg = "Failed to fetch manifest from: " + manifestUrl;
        if (halErr.length() > 0) {
            msg += " (" + halErr + ")";
        }
#ifdef ESP_PLATFORM
        if (g_http_get_diag.length() > 0) {
            msg += " | httpGet: " + g_http_get_diag;
        }
#endif
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_check_time_ = hal_->millis();
        }
        setError(UpdateError::NET_ERROR, msg);
        return;
    }

    // Parse manifest JSON
    crashBreadcrumbRecord(CRUMB_UPDATE_JSON_BEFORE, (uint32_t)manifestJson.length(), hal_->getFreeHeap());
    JsonDocument doc;
    DeserializationError jsonError = deserializeJson(doc, manifestJson);
    crashBreadcrumbRecord(CRUMB_UPDATE_JSON_AFTER, hal_->getFreeHeap(), hal_->getMaxAllocHeap());
    if (jsonError) {
        String preview = manifestJson.substring(0, 120);
        preview.replace("\r", "\\r");
        preview.replace("\n", "\\n");
        String tail = manifestJson.length() > 120
            ? manifestJson.substring(manifestJson.length() - 80)
            : "";
        tail.replace("\r", "\\r");
        tail.replace("\n", "\\n");
        String msg = "Parse " + String(jsonError.c_str()) +
                     " | body_len=" + String(manifestJson.length()) +
                     " | head=" + preview + " | tail=" + tail;
#ifdef ESP_PLATFORM
        if (g_http_get_diag.length() > 0) {
            msg += " | httpGet: " + g_http_get_diag;
        }
#endif
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_check_time_ = hal_->millis();
        }
        setError(UpdateError::MANIFEST_PARSE, msg);
        return;
    }

    UpdateManifest parsed;
    bool githubRelease = doc["latest_version"].isNull() && !doc["tag_name"].isNull();

    if (githubRelease) {
        String tag = doc["tag_name"].as<String>();
        if (tag.startsWith("v") || tag.startsWith("V")) tag = tag.substring(1);
        parsed.latest_version = tag;
        parsed.release_date = doc["published_at"].as<String>();

        JsonArray assets = doc["assets"].as<JsonArray>();
        for (JsonObject asset : assets) {
            String name = asset["name"].as<String>();
            VersionInfo* target = nullptr;
            if (name == "firmware.bin") {
                target = &parsed.firmware;
            } else if (name == "littlefs.bin") {
                target = &parsed.filesystem;
            }

            if (target != nullptr) {
                target->version = parsed.latest_version;
                target->url = asset["browser_download_url"].as<String>();
                target->size_bytes = asset["size"].as<uint32_t>();

                String digest = asset["digest"].as<String>();
                target->sha256 = digest.startsWith("sha256:")
                    ? digest.substring(7)
                    : "";
            }
        }
    } else {
        String latestVer = !doc["latest_version"].isNull() ? doc["latest_version"].as<String>() : String("");
        if (latestVer.startsWith("v") || latestVer.startsWith("V")) latestVer = latestVer.substring(1);
        parsed.latest_version = latestVer;
        parsed.release_date = !doc["release_date"].isNull() ? doc["release_date"].as<String>() : String("");

        if (doc["firmware"].is<JsonObject>()) {
            parsed.firmware.version = doc["firmware"]["version"].as<String>();
            parsed.firmware.url = doc["firmware"]["url"].as<String>();
            parsed.firmware.size_bytes = doc["firmware"]["size_bytes"].as<uint32_t>();
            parsed.firmware.sha256 = doc["firmware"]["sha256"].as<String>();
        }

        if (doc["filesystem"].is<JsonObject>()) {
            parsed.filesystem.version = doc["filesystem"]["version"].as<String>();
            parsed.filesystem.url = doc["filesystem"]["url"].as<String>();
            parsed.filesystem.size_bytes = doc["filesystem"]["size_bytes"].as<uint32_t>();
            parsed.filesystem.sha256 = doc["filesystem"]["sha256"].as<String>();
        }
    }

    crashBreadcrumbRecord(CRUMB_UPDATE_END, hal_->getFreeHeap(), hal_->getMaxAllocHeap());

    if (parsed.latest_version.length() == 0) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_check_time_ = hal_->millis();
        }
        setError(UpdateError::MANIFEST_PARSE, "Manifest missing latest_version field");
        return;
    }

    String devVer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifest_ = parsed;
        last_check_time_ = hal_->millis();
        last_check_ok_ = true;
        devVer = device_version_;
    }

    if (isVersionNewer(devVer, parsed.latest_version)) {
        setStatus(UpdateStatus::AVAILABLE, true);
        logger.logInfo("Update available: " + devVer + " -> " + parsed.latest_version);
    } else {
        setStatus(UpdateStatus::CURRENT, true);
        logger.logInfo("Firmware is up to date: " + devVer);
    }
}

void UpdateManager::installUpdate(bool skip_filesystem, bool force) {
    if (!hal_) {
        setError(UpdateError::NET_ERROR, "HAL not initialized");
        return;
    }

    // Whether a usable manifest is currently loaded. Gate on the actual data
    // (firmware URL + size), NOT on status_: a transient re-check whose network
    // fetch failed sets status_ to ERROR via setError() while leaving the
    // previously-parsed manifest_ intact. Gating force-install on status_ then
    // wrongly reported "No manifest available" even though a good manifest was
    // loaded and shown in the UI. (The intermittent GitHub redirect fetch made
    // such a failed re-check common.)
    bool manifestLoaded = manifest_.latest_version.length() > 0 &&
                          manifest_.firmware.url.length() > 0 &&
                          manifest_.firmware.size_bytes > 0;

    if (force) {
        // Force mode: allow install whenever a manifest has been fetched, even
        // if the current version already matches (reinstall / recover bad flash).
        if (!manifestLoaded) {
            setError(UpdateError::MANIFEST_PARSE, "No manifest available - check for updates first");
            return;
        }
    } else {
        // Non-force: only install when the manifest is a genuinely newer version.
        if (!manifestLoaded || !isVersionNewer(device_version_, manifest_.latest_version)) {
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

    abort_requested_.store(false, std::memory_order_release);

    // --- Install firmware ---
    if (manifest_.firmware.url.length() > 0 && manifest_.firmware.size_bytes > 0) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            phase_ = "firmware";
            bytes_downloaded_ = 0;
            total_bytes_ = manifest_.firmware.size_bytes;
            progress_percent_ = 0;
        }
        setStatus(UpdateStatus::DOWNLOADING, true);

        logger.logInfo("Downloading firmware: " + String((unsigned long)manifest_.firmware.size_bytes) + " bytes");

        // Initialize SHA256 context for incremental hashing
        sha_ctx_ = new mbedtls_sha256_context();
        mbedtls_sha256_init((mbedtls_sha256_context*)sha_ctx_);
        mbedtls_sha256_starts((mbedtls_sha256_context*)sha_ctx_, false);  // false = SHA256 (not SHA224)
        sha_ctx_initialized_ = true;

        // Begin OTA firmware partition
        if (!hal_->otaBegin(manifest_.firmware.size_bytes, 0)) {
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            setError(UpdateError::INSTALL_FAILED, "Failed to begin firmware OTA: " + hal_->otaGetError());
            return;
        }

        // Stream download, writing each chunk to OTA partition and updating SHA256
        bool downloadSuccess = hal_->httpGetStream(manifest_.firmware.url,
            [this](const uint8_t* data, size_t len, uint32_t downloaded, uint32_t total) -> bool {
                if (abort_requested_.load(std::memory_order_acquire)) {
                    return false;
                }
                // Update SHA256 hash with incoming data
                if (sha_ctx_initialized_) {
                    mbedtls_sha256_update((mbedtls_sha256_context*)sha_ctx_, data, len);
                }

                // Write to OTA partition
                size_t written = hal_->otaWrite(data, len);
                if (written != len) {
                    return false; // Write failed, abort download
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    bytes_downloaded_ = downloaded;
                    total_bytes_ = total;
                    progress_percent_ = total > 0 ? (uint8_t)(((uint64_t)downloaded * 100ULL) / total) : 0;
                }
                return true;
            }, 120000); // 2 minute timeout for firmware download

        if (!downloadSuccess) {
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            hal_->otaAbort();
            if (abort_requested_.load(std::memory_order_acquire)) {
                setStatus(UpdateStatus::IDLE, true);
                return;
            }
            String msg = "Firmware download failed or incomplete";
#ifdef ESP_PLATFORM
            if (g_http_get_diag.length() > 0) {
                msg += " | httpGetStream: " + g_http_get_diag;
            }
#endif
            setError(UpdateError::DOWNLOAD_FAILED, msg);
            return;
        }

        logger.logInfo("Firmware download complete: " + String((unsigned long)bytes_downloaded_) + " bytes, verifying checksum...");

        // Verify SHA256 checksum if provided
        setStatus(UpdateStatus::VERIFYING);
        if (manifest_.firmware.sha256.length() > 0) {
            unsigned char hash[32];
            mbedtls_sha256_finish((mbedtls_sha256_context*)sha_ctx_, hash);
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;

            // Convert to hex string
            char hashHex[65];
            for (int i = 0; i < 32; i++) {
                sprintf(hashHex + (i * 2), "%02x", hash[i]);
            }
            hashHex[64] = '\0';

            // Compare with manifest
            if (!manifest_.firmware.sha256.equalsIgnoreCase(hashHex)) {
                hal_->otaAbort();
                setError(UpdateError::CHECKSUM_MISMATCH,
                    "Firmware SHA256 mismatch: expected " + manifest_.firmware.sha256 +
                    ", got " + String(hashHex));
                return;
            }

            logger.logInfo("Firmware SHA256 verified: " + String(hashHex));
        } else {
            // No checksum provided, cleanup context
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            logger.logWarning("No firmware SHA256 checksum provided - skipping verification");
        }

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

        // Initialize SHA256 context for incremental hashing
        sha_ctx_ = new mbedtls_sha256_context();
        mbedtls_sha256_init((mbedtls_sha256_context*)sha_ctx_);
        mbedtls_sha256_starts((mbedtls_sha256_context*)sha_ctx_, false);
        sha_ctx_initialized_ = true;

        // Stop web server before filesystem OTA to prevent async handlers
        // on core 0 from accessing LittleFS while it's being overwritten
        hal_->webServerEnd();
        delay(100);  // Allow in-flight requests to complete

        // End current filesystem before flashing
        hal_->fsEnd();

        if (!hal_->otaBegin(manifest_.filesystem.size_bytes, 1)) {
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            hal_->fsBegin();
            setError(UpdateError::INSTALL_FAILED, "Failed to begin filesystem OTA: " + hal_->otaGetError());
            return;
        }

        bool fsDownloadSuccess = hal_->httpGetStream(manifest_.filesystem.url,
            [this](const uint8_t* data, size_t len, uint32_t downloaded, uint32_t total) -> bool {
                if (abort_requested_.load(std::memory_order_acquire)) {
                    return false;
                }
                // Update SHA256 hash with incoming data
                if (sha_ctx_initialized_) {
                    mbedtls_sha256_update((mbedtls_sha256_context*)sha_ctx_, data, len);
                }

                // Write to OTA partition
                size_t written = hal_->otaWrite(data, len);
                if (written != len) {
                    logger.logError("Filesystem OTA write failed: wrote " + String((unsigned long)written) + "/" + String((unsigned long)len) + " bytes");
                    return false;
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    bytes_downloaded_ = downloaded;
                    total_bytes_ = total;
                    progress_percent_ = total > 0 ? (uint8_t)(((uint64_t)downloaded * 100ULL) / total) : 0;
                }
                return true;
            }, 120000);

        if (!fsDownloadSuccess) {
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            hal_->otaAbort();
            hal_->fsBegin();
            if (abort_requested_.load(std::memory_order_acquire)) {
                setStatus(UpdateStatus::IDLE, true);
                return;
            }
            setError(UpdateError::DOWNLOAD_FAILED, "Filesystem download failed or incomplete");
            return;
        }

        logger.logInfo("Filesystem download complete: " + String((unsigned long)bytes_downloaded_) + " bytes, verifying checksum...");

        // Verify SHA256 checksum if provided
        setStatus(UpdateStatus::VERIFYING);
        if (manifest_.filesystem.sha256.length() > 0) {
            unsigned char hash[32];
            mbedtls_sha256_finish((mbedtls_sha256_context*)sha_ctx_, hash);
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;

            // Convert to hex string
            char hashHex[65];
            for (int i = 0; i < 32; i++) {
                sprintf(hashHex + (i * 2), "%02x", hash[i]);
            }
            hashHex[64] = '\0';

            // Compare with manifest
            if (!manifest_.filesystem.sha256.equalsIgnoreCase(hashHex)) {
                hal_->otaAbort();
                hal_->fsBegin();
                setError(UpdateError::CHECKSUM_MISMATCH,
                    "Filesystem SHA256 mismatch: expected " + manifest_.filesystem.sha256 +
                    ", got " + String(hashHex));
                return;
            }

            logger.logInfo("Filesystem SHA256 verified: " + String(hashHex));
        } else {
            // No checksum provided, cleanup context
            mbedtls_sha256_free((mbedtls_sha256_context*)sha_ctx_);
            delete (mbedtls_sha256_context*)sha_ctx_;
            sha_ctx_ = nullptr;
            sha_ctx_initialized_ = false;
            logger.logWarning("No filesystem SHA256 checksum provided - skipping verification");
        }

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

void UpdateManager::requestCheck() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        check_requested_ = true;  // coalesces repeated clicks while pending
        status_ = UpdateStatus::CHECKING; // lets UI poll immediately
        last_check_ok_ = false;
    }
    logger.logInfo("Update check requested (deferred to main loop)");
    crashBreadcrumbRecord(CRUMB_UPDATE_REQUESTED, 0, 0);
}

void UpdateManager::requestInstall(bool skip_filesystem, bool force) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        install_skip_filesystem_ = skip_filesystem;
        install_force_ = force;
        install_requested_ = true;
    }
    logger.logInfo("Update install requested (deferred to main loop)");
}

void UpdateManager::update() {
    if (!hal_) return;

    bool doCheck = false;
    bool doInstall = false;
    bool skipFs = false;
    bool forceInstall = false;
    bool busy = false;
    unsigned long lastCheck = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (check_requested_) {
            check_requested_ = false;
            doCheck = true;
        } else if (status_ == UpdateStatus::DOWNLOADING || status_ == UpdateStatus::INSTALLING ||
                   status_ == UpdateStatus::VERIFYING || status_ == UpdateStatus::CHECKING) {
            busy = true;
        } else if (install_requested_) {
            install_requested_ = false;
            skipFs = install_skip_filesystem_;
            forceInstall = install_force_;
            doInstall = true;
        }
        lastCheck = last_check_time_;
    }

    if (doCheck) {
        checkForUpdates();
        return;
    }
    if (busy) return;
    if (doInstall) {
        installUpdate(skipFs, forceInstall);
        return;
    }

    if (!settingsManager.getAutoUpdateEnabled()) return;

    unsigned long checkIntervalMs = (unsigned long)settingsManager.getUpdateCheckIntervalHours() * 3600000UL;
    unsigned long now = hal_->millis();

    if (lastCheck == 0 || (now - lastCheck) >= checkIntervalMs) {
        checkForUpdates();
    }
}

UpdateStatusSnapshot UpdateManager::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    UpdateStatusSnapshot snapshot;
    snapshot.status = status_;
    snapshot.error = error_;
    snapshot.progress_percent = progress_percent_;
    snapshot.bytes_downloaded = bytes_downloaded_;
    snapshot.total_bytes = total_bytes_;
    snapshot.error_message = last_error_message_;
    snapshot.phase = phase_;
    snapshot.last_check_time = last_check_time_;
    snapshot.next_check_time = 0;

    return snapshot;
}

String UpdateManager::getDeviceVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return device_version_;
}

String UpdateManager::getLatestVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return manifest_.latest_version;
}

bool UpdateManager::isUpdateAvailable() const {
    String devVer, latestVer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        devVer = device_version_;
        latestVer = manifest_.latest_version;
    }
    if (latestVer.length() == 0) {
        return false;
    }
    return isVersionNewer(devVer, latestVer);
}

void UpdateManager::toJson(JsonObject& json) const {
    std::lock_guard<std::mutex> lock(mutex_);
    json["status"] = (int)status_;
    json["error"] = (int)error_;
    json["error_message"] = last_error_message_;
    json["progress_percent"] = progress_percent_;
    json["bytes_downloaded"] = bytes_downloaded_;
    json["total_bytes"] = total_bytes_;
}

JsonDocument UpdateManager::getCheckResponseJson() const {
    String manifestUrl, devVer, latestVer, lastErr;
    unsigned long lastCheck;
    bool checkOk;
    UpdateManifest m;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifestUrl = manifest_url_;
        lastCheck = last_check_time_;
        devVer = device_version_;
        latestVer = manifest_.latest_version;
        checkOk = last_check_ok_;
        lastErr = last_error_message_;
        m = manifest_;
    }

    JsonDocument doc;
    doc["manifest_url"] = manifestUrl;
    doc["last_check_time"] = lastCheck;
    doc["current_version"] = devVer;
    doc["available_version"] = latestVer;
    doc["update_available"] = (latestVer.length() > 0) ? isVersionNewer(devVer, latestVer) : false;

    doc["check_ok"] = checkOk;
    if (!checkOk && lastErr.length() > 0) {
        doc["error"] = lastErr;
    }

    JsonObject firmware = doc["firmware"].to<JsonObject>();
    firmware["version"] = m.firmware.version;
    firmware["url"] = m.firmware.url;
    firmware["size_bytes"] = m.firmware.size_bytes;

    JsonObject filesystem = doc["filesystem"].to<JsonObject>();
    filesystem["version"] = m.filesystem.version;
    filesystem["url"] = m.filesystem.url;
    filesystem["size_bytes"] = m.filesystem.size_bytes;

    doc["release_date"] = m.release_date;
    doc["github_repo"] = String(githubRepo);

    return doc;
}

JsonDocument UpdateManager::getStatusResponseJson() const {
    UpdateStatus st;
    uint8_t progress;
    String phase, lastErr;
    unsigned long lastCheck;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        st = status_;
        progress = progress_percent_;
        phase = phase_;
        lastCheck = last_check_time_;
        lastErr = last_error_message_;
    }

    JsonDocument doc;
    String statusStr;
    switch (st) {
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
    doc["progress"] = progress;
    doc["phase"] = phase;
    doc["last_check"] = lastCheck;
    doc["error"] = lastErr;

    return doc;
}

void UpdateManager::abort() {
    bool active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ == UpdateStatus::DOWNLOADING || status_ == UpdateStatus::INSTALLING ||
            status_ == UpdateStatus::VERIFYING) {
            abort_requested_.store(true, std::memory_order_release);
            status_ = UpdateStatus::IDLE;
            error_ = UpdateError::NONE;
            last_error_message_ = "";
            active = true;
        }
    }
    if (active) {
        logger.logWarning("Update operation aborted");
    }
}

void UpdateManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    abort_requested_.store(false, std::memory_order_release);
    status_ = UpdateStatus::IDLE;
    error_ = UpdateError::NONE;
    last_error_message_ = "";
    phase_ = "";
    progress_percent_ = 0;
    bytes_downloaded_ = 0;
    total_bytes_ = 0;
}
