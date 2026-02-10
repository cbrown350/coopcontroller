#!/bin/bash
# Generate version_manifest.json for UpdateManager
# Usage: ./generate-manifest.sh <version> <firmware_url> <littlefs_url> <merged_url> <output_file>

set -euo pipefail

VERSION="${1:?Version tag required}"
FIRMWARE_URL="${2:?Firmware URL required}"
LITTLEFS_URL="${3:?LittleFS URL required}"
MERGED_URL="${4:?Merged binary URL required}"
OUTPUT_FILE="${5:?Output file path required}"

# Get current timestamp in ISO 8601 format
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Calculate SHA256 checksums if files exist locally
get_checksum() {
    local file="$1"
    if [ -f "$file" ]; then
        sha256sum "$file" | awk '{print $1}'
    else
        echo "unknown"
    fi
}

FIRMWARE_CHECKSUM=$(get_checksum ".pio/build/esp32-release/firmware.bin")
LITTLEFS_CHECKSUM=$(get_checksum ".pio/build/esp32-release/littlefs.bin")
MERGED_CHECKSUM=$(get_checksum ".pio/build/esp32-release/firmware_merged.bin")

# Generate manifest JSON
cat > "$OUTPUT_FILE" << EOF
{
  "version": "${VERSION}",
  "timestamp": "${TIMESTAMP}",
  "firmware": {
    "url": "${FIRMWARE_URL}",
    "checksum": "${FIRMWARE_CHECKSUM}",
    "type": "application/octet-stream",
    "description": "Application firmware binary (upload at 0x10000)"
  },
  "littlefs": {
    "url": "${LITTLEFS_URL}",
    "checksum": "${LITTLEFS_CHECKSUM}",
    "type": "application/octet-stream",
    "description": "LittleFS filesystem binary (upload at 0x3d0000)"
  },
  "merged": {
    "url": "${MERGED_URL}",
    "checksum": "${MERGED_CHECKSUM}",
    "type": "application/octet-stream",
    "description": "Complete merged binary (upload at 0x0, includes bootloader)"
  },
  "changelog_url": "https://github.com/cbrown350/coopcontroller/releases/tag/${VERSION}"
}
EOF

echo "✅ Manifest generated: $OUTPUT_FILE"
cat "$OUTPUT_FILE"
