# GitHub Actions Release Workflow

This document describes the automated release workflow for the coop_controller project.

## Overview

The release workflow automatically builds firmware and web UI, runs tests, and creates GitHub releases with semantic versioning.

**Trigger:** Pushes to `master` branch (doc-only changes are filtered out)
**Output:** GitHub Release with firmware/filesystem binaries and version manifest

## Semantic Versioning

The workflow uses **conventional commits** to automatically determine version bumps:

| Commit Type | Bump Type | Example |
|---|---|---|
| `feat:` | MINOR | v1.0.0 → v1.1.0 |
| `fix:` / `perf:` | PATCH | v1.0.0 → v1.0.1 |
| `BREAKING CHANGE:` in body | MAJOR | v1.0.0 → v2.0.0 |
| Other (`build:`, `docs:`, etc.) | Skip | No release |

### How It Works

1. Workflow fetches the latest git tag (e.g., `v0.1.0`)
2. Parses all commits since that tag
3. Scans commit messages for conventional commit patterns
4. Determines highest priority bump (MAJOR > MINOR > PATCH)
5. Increments appropriate version component
6. Creates and tags the new release

**Example:**
```
Latest tag: v0.1.0

Commits since v0.1.0:
  - feat(door): add timeout configuration    ← MINOR bump
  - fix(api): response encoding bug          ← PATCH bump
  - BREAKING CHANGE: remove deprecated API  ← MAJOR bump (highest priority)

Result: v0.1.0 → v1.0.0 (MAJOR bump applied)
```

## Workflow Stages

### 1. Checkout & Version Detection
- Clones repository with full git history
- Parses commits since last tag
- Determines semantic version bump
- Exits if tag already exists (idempotency)

### 2. Build Firmware
- Sets up Python environment
- Installs PlatformIO Core
- Runs: `pio run -e esp32-release`
- Environment: `FIRMWARE_VERSION=<detected_version>`
- Outputs:
  - `.pio/build/esp32-release/firmware.bin` (application only)
  - `.pio/build/esp32-release/littlefs.bin` (filesystem only)
  - `.pio/build/esp32-release/firmware_merged.bin` (complete flash image)

### 3. Build Web UI
- Sets up Node.js LTS
- Runs: `cd web && npm ci && npm run build`
- Outputs: `web/dist/` (SolidJS + Tailwind CSS bundle)

### 4. Run Tests
- Executes: `pio test`
- All 488 unit tests must pass
- Release **fails** if tests don't pass (regression protection)

### 5. Calculate Checksums
- Computes SHA256 hashes of firmware and filesystem
- Exports for manifest and release notes

### 6. Generate Version Manifest
- Creates `version_manifest.json` for UpdateManager
- Includes version, checksums, download URLs, release timestamp
- Used by devices to check for OTA updates

### 7. Create GitHub Release
- Uses `softprops/action-gh-release@v1` (modern action)
- Creates annotated git tag
- Uploads artifacts as release assets
- Generates changelog from commit messages
- Assets:
  - `firmware.bin` - Application binary (upload at 0x10000)
  - `littlefs.bin` - Filesystem binary (upload at 0x3d0000)
  - `firmware_merged.bin` - Complete flash image (upload at 0x0)
  - `version_manifest.json` - OTA manifest for devices
  - `firmware.bin.sha256` - Checksum file

## Workflow File

**Location:** `.github/workflows/release.yml`

**Key Environment Variables:**
- `FIRMWARE_VERSION` - Set to detected semantic version during build
- `GITHUB_REPOSITORY` - Used by manifest script to build GitHub URLs

**Conditions:**
- Only triggers on pushes to `master` branch
- Skips if only documentation files changed:
  - `*.md` files
  - `docs/**` directory
  - `.gitignore`
  - `LICENSE`

## Helper Scripts

### parse-semver.sh
**Location:** `.github/scripts/parse-semver.sh`

Parses git history and determines semantic version bump based on conventional commits.

**Usage:**
```bash
source .github/scripts/parse-semver.sh
echo "Bump type: $BUMP_TYPE"
echo "New version: $NEW_VERSION"
```

**Output Variables:**
- `BUMP_TYPE` - MAJOR, MINOR, or PATCH
- `NEW_VERSION` - Full version tag (e.g., v1.0.0)
- `MAJOR`, `MINOR`, `PATCH` - Individual components

### generate-manifest.sh
**Location:** `.github/scripts/generate-manifest.sh`

Generates version_manifest.json from build artifacts.

**Usage:**
```bash
bash .github/scripts/generate-manifest.sh \
  "v1.0.0" \
  "https://github.com/.../releases/download/v1.0.0/firmware.bin" \
  "https://github.com/.../releases/download/v1.0.0/littlefs.bin" \
  "https://github.com/.../releases/download/v1.0.0/firmware_merged.bin" \
  "output_manifest.json"
```

### generate_version_manifest.py
**Location:** `build_scripts/generate_version_manifest.py`

Python script that generates the OTA manifest file with checksums.

**Usage:**
```bash
python3 build_scripts/generate_version_manifest.py \
  --version v1.0.0 \
  --firmware-path artifacts/firmware.bin \
  --filesystem-path artifacts/littlefs.bin
```

**Output:**
```json
{
  "latest_version": "1.0.0",
  "firmware": {
    "version": "1.0.0",
    "url": "https://github.com/cbrown350/coopcontroller/releases/download/v1.0.0/firmware.bin",
    "size_bytes": 1234567,
    "sha256": "abc123..."
  },
  "filesystem": {
    "version": "1.0.0",
    "url": "https://github.com/cbrown350/coopcontroller/releases/download/v1.0.0/littlefs.bin",
    "size_bytes": 67890,
    "sha256": "def456..."
  },
  "release_date": "2024-02-10T12:34:56.789Z",
  "github_repo": "cbrown350/coopcontroller"
}
```

## Manual Testing

### Test Semantic Versioning Parser

```bash
# Simulate the version detection locally
cd /path/to/coop_controller
source .github/scripts/parse-semver.sh
echo "Next version: $NEW_VERSION ($BUMP_TYPE bump)"
```

### Test Manifest Generation

```bash
# Generate a manifest with test binaries
python3 build_scripts/generate_version_manifest.py \
  --version v1.0.0 \
  --firmware-path .pio/build/esp32-release/firmware.bin \
  --filesystem-path .pio/build/esp32-release/littlefs.bin \
  --output test-manifest.json

cat test-manifest.json
```

### Manual Release (if needed)

If automatic release fails or you need to create a release manually:

```bash
# Create the tag
git tag -a v1.0.0 -m "Release v1.0.0"

# Push to trigger workflow (or create release manually on GitHub)
git push origin v1.0.0

# Or create release directly via GitHub CLI
gh release create v1.0.0 \
  .pio/build/esp32-release/firmware.bin \
  .pio/build/esp32-release/littlefs.bin \
  version_manifest.json \
  --title "Release v1.0.0"
```

## Troubleshooting

### Release didn't trigger
- **Check:** Push was to `master` branch, not a different branch
- **Check:** Changes are not doc-only (edit a source file)
- **Check:** Git history is available (workflow has `fetch-depth: 0`)

### Wrong version detected
- **Check:** Latest git tag is correct: `git describe --tags --abbrev=0`
- **Check:** Commit messages follow conventional commit format
- **Check:** No typos in `feat:`, `fix:`, `BREAKING CHANGE:` patterns

### Manifest generation failed
- **Check:** Firmware and filesystem binaries exist after build
- **Check:** File paths in workflow are correct
- **Check:** No permission issues accessing build artifacts

### Tests failed, release didn't complete
- **Expected behavior** - release is blocked if tests fail
- **Fix:** Address the failing tests and push a new commit
- **Tests:** Run locally with `pio test` to debug

## Integration with UpdateManager

The `version_manifest.json` file is used by the device's UpdateManager to check for available updates:

1. Device polls the manifest URL (stored in firmware)
2. Compares `latest_version` with current version
3. If newer version available, downloads and installs
4. Uses SHA256 checksums to verify integrity

**Manifest must be accessible at:**
```
https://github.com/cbrown350/coopcontroller/releases/download/v<version>/version_manifest.json
```

## Current Version Tags

```
v0.0.1 - Initial release
v0.0.2 - Bug fixes
v0.0.3 - Feature additions
v0.1.0 - API authentication, factory reset
v1.0.0 - Event-based data capture (breaking changes)
```

## Related Documentation

- [Architecture](./architecture.md) - System design and components
- [API Reference](./api-reference.md) - REST API endpoints
- [Contributing](./contributing.md) - PR workflow and code review
- [Development Guide](./development-guide.md) - Build and test setup
