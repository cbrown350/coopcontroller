# OTA Update System

**Status:** In Implementation (Feb 2026)

## Overview

The OTA (Over-The-Air) Update System enables CoopController devices to automatically check for, download, and install firmware and filesystem updates from GitHub releases. This eliminates the need for manual USB uploads and enables remote management at scale.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     CoopController Device (ESP32)               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────┐         ┌─────────────────┐              │
│  │  UpdateManager   │◄────────┤  SettingsManager│              │
│  │  (New Component) │         └─────────────────┘              │
│  └────────┬─────────┘                                           │
│           │                                                     │
│     ┌─────▼──────────────┐                                     │
│     │ Check for Updates  │                                     │
│     │ (Daily or Manual)  │                                     │
│     └──────┬─────────────┘                                     │
│            │                                                    │
└────────────┼────────────────────────────────────────────────────┘
             │
             │ HTTPS (Over WiFi)
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│                     GitHub Releases (cbrown350/coopcontroller)  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  /releases/download/v1.0.0/                                    │
│  ├── firmware.bin          (firmware binary)                   │
│  ├── littlefs.bin          (filesystem binary)                 │
│  └── version_manifest.json (version metadata)                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Component Details

### UpdateManager (lib/UpdateManager/)

**Responsibility:** Check for updates, download artifacts, verify checksums, trigger installation

**Key Methods:**
- `checkForUpdates()` - Query manifest, compare versions, return bool if update available
- `downloadUpdate(firmware_url, filesystem_url)` - Download with SHA256 verification
- `installUpdate()` - Apply firmware/filesystem updates (leverages existing mechanisms)
- `getLatestManifest()` - Fetch and parse version_manifest.json
- `getDeviceVersion()` - Return current firmware version string

**Settings Integration:**
- `auto_update_enabled` (bool, default: false) - Enable/disable automatic daily checks
- `update_check_interval_hours` (int, default: 24) - Check frequency
- `manifest_url` (string) - URL to version_manifest.json (set via build flag)

**HAL Integration:**
- Uses HAL for HTTP requests (network efficiency)
- Uses HAL for filesystem access
- Fully mockable for unit testing with MockHAL

### Version Manifest Format

**File:** version_manifest.json (published to GitHub releases)

```json
{
  "latest_version": "1.0.0",
  "firmware": {
    "version": "1.0.0",
    "url": "https://github.com/cbrown350/coopcontroller/releases/download/v1.0.0/firmware.bin",
    "size_bytes": 1303537,
    "sha256": "abc123def456..."
  },
  "filesystem": {
    "version": "1.0.0",
    "url": "https://github.com/cbrown350/coopcontroller/releases/download/v1.0.0/littlefs.bin",
    "size_bytes": 262144,
    "sha256": "ghi789jkl012..."
  },
  "release_date": "2026-02-10T12:00:00Z",
  "changelog": "- Added OTA update system\n- Bug fixes\n- Performance improvements"
}
```

## REST API Endpoints

### GET /update/check (Public)
Check for available updates without installing.

**Response:**
```json
{
  "update_available": true,
  "current_version": "0.9.0",
  "available_version": "1.0.0",
  "manifest": {
    "firmware": {
      "url": "https://github.com/.../firmware.bin",
      "size_bytes": 1303537
    },
    "filesystem": {
      "url": "https://github.com/.../littlefs.bin",
      "size_bytes": 262144
    }
  }
}
```

### POST /update/install (Protected)
Initiate update installation. Requires authentication if API auth is enabled.

**Parameters:**
- `skip_filesystem` (optional bool) - Skip filesystem update, only update firmware

**Response:**
```json
{
  "status": "installing",
  "progress": 0,
  "current_version": "0.9.0",
  "target_version": "1.0.0"
}
```

### GET /update/status (Public)
Get current update status and progress.

**Response:**
```json
{
  "status": "idle",
  "progress": 100,
  "last_check": "2026-02-10T12:00:00Z",
  "next_check": "2026-02-11T12:00:00Z"
}
```

## Web UI Settings

**Location:** Settings page → "System Updates" section

Controls:
- **Enable Auto-Updates** - Toggle to enable/disable automatic daily checks
- **Check Interval** - Number selector (1-168 hours)
- **Manual Check Button** - Trigger immediate update check
- **Update Available Badge** - Shows when update is ready (with version number)
- **Install Update Button** - (Appears when update available)
- **Last Check** - Display timestamp of last manifest check

## GitHub Actions Workflow

**Trigger:** Pushes to master branch

**Process:**
1. Determine new semantic version (based on commit messages or manual tag)
2. Build firmware: `pio run -e esp32-release`
3. Build web UI: `cd web && npm run build`
4. Run tests: `pio test` (ensure no regressions)
5. Extract firmware.bin and littlefs.bin
6. Calculate SHA256 checksums
7. Generate version_manifest.json
8. Create GitHub release with tag (v1.0.0, etc.)
9. Upload firmware.bin, littlefs.bin, version_manifest.json as release assets

**Environment Variables:**
- `FIRMWARE_VERSION` - Semantic version (must be set, passed to build)
- `GITHUB_TOKEN` - Automatic (used for creating releases)

## Version Comparison Logic

Implements semantic versioning (MAJOR.MINOR.PATCH):
- `1.0.0` < `1.0.1` (patch update)
- `1.0.1` < `1.1.0` (minor update)
- `1.1.0` < `2.0.0` (major update)

Algorithm:
```cpp
bool isUpdateAvailable(const String& currentVersion, const String& availableVersion) {
  // Parse version strings: "1.0.0" -> [1, 0, 0]
  // Compare: if available > current, return true
}
```

## PlatformIO Configuration

**platformio.ini additions:**
```ini
build_flags =
    -D GITHUB_REPO=cbrown350/coopcontroller
    -D VERSION_MANIFEST_URL=https://api.github.com/repos/cbrown350/coopcontroller/releases/latest
```

**Environment Variable:**
```bash
export FIRMWARE_VERSION=1.0.0  # Must be set before build
pio run -e esp32-release
```

## Implementation Phases

### Phase 1: Infrastructure (Current)
- ✅ GitHub Actions workflow with semantic versioning
- ✅ Version manifest generation script
- ✅ PlatformIO configuration updates
- ✅ `/version` endpoint enhancements

### Phase 2: UpdateManager Component
- UpdateManager class implementation
- HAL integration for HTTP/filesystem
- Version comparison logic
- Checksum verification (SHA256)
- Unit tests with MockHAL

### Phase 3: Web UI Integration
- Settings page controls
- Update availability badge
- Manual/automatic update triggering
- Progress display

### Phase 4: Edge Cases & Optimization
- Resume interrupted downloads
- Rollback on failed installation
- Storage space verification
- Network error recovery

## Error Handling

| Scenario | Behavior |
|----------|----------|
| Network unavailable | Defer check until WiFi connects, show warning |
| Download fails | Retry up to 3 times with exponential backoff |
| Checksum mismatch | Reject download, log error, do not install |
| Insufficient storage | Warn user, do not attempt update |
| Installation fails | Keep current version, alert user |
| Manifest unreachable | Log error, continue normal operation |

## Testing Strategy

**Unit Tests (MockHAL):**
- Version comparison logic (all semver combinations)
- Manifest parsing
- Checksum verification
- Error handling and retries

**Integration Tests:**
- Full update check → download → install cycle
- Network failure recovery
- Settings persistence

**Manual Testing:**
- OTA on real ESP32 device
- Various network conditions (slow, unreliable)
- Storage space edge cases
- Version rollback scenarios

## Security Considerations

1. **HTTPS Only** - All downloads use HTTPS (GitHub enforces this)
2. **Checksum Verification** - SHA256 verification before installation
3. **GitHub Source Trust** - Release artifacts must come from official GitHub releases
4. **No Auto-Install** - Default disabled; requires explicit user enable in settings
5. **Confirmation Required** - Manual update checks show manifest before downloading

## Future Enhancements

- **Automatic Rollback** - Revert to previous version if boot fails
- **Partial Updates** - Update only firmware or filesystem independently
- **Delta Updates** - Download only changed binary segments (reduces bandwidth)
- **Signed Releases** - GPG signing of releases for additional security
- **Update Scheduling** - Schedule updates for specific times (e.g., midnight)
- **Notification** - Email/Telegram notifications when updates available

## Troubleshooting

### Update Check Never Completes
- Check WiFi connectivity (`/system_status` endpoint)
- Verify manifest URL is reachable
- Check device logs for network errors

### "Update Available" But Install Fails
- Check device storage space
- Verify checksum calculation
- Check device logs for installation errors

### Device Reboots During Update
- Watchdog timeout too short - check configuration
- Insufficient RAM during download - reduce buffer size
- SD card/LittleFS corruption - may need factory reset

### Version Shows Outdated After Update
- Check device time (NTP sync issue)
- Verify device restarted properly
- Check build timestamp in logs

## References

- [GitHub Releases API](https://docs.github.com/en/rest/releases)
- [Semantic Versioning](https://semver.org/)
- [SHA256 Checksums](https://en.wikipedia.org/wiki/SHA-2)
- [ESP32 OTA Updates](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
