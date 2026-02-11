#!/usr/bin/env python3
"""
Generate version_manifest.json for OTA update checking.

This script is called by GitHub Actions during release builds to create
the manifest file that devices poll to check for available updates.

Usage:
    python3 generate_version_manifest.py \
        --version v1.0.0 \
        --firmware-path .pio/build/esp32-release/firmware.bin \
        --filesystem-path .pio/build/esp32-release/littlefs.bin
"""

import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path


def calculate_sha256(file_path):
    """Calculate SHA256 checksum of a file."""
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()


def get_file_size(file_path):
    """Get file size in bytes."""
    return os.path.getsize(file_path)


def get_github_repo():
    """Get GitHub repository from environment or default."""
    return os.getenv("GITHUB_REPOSITORY", "cbrown350/coopcontroller")


def generate_manifest(version, firmware_path, filesystem_path):
    """
    Generate the version manifest JSON structure.
    
    Args:
        version: Semantic version string (e.g., "v1.0.0")
        firmware_path: Path to firmware.bin
        filesystem_path: Path to littlefs.bin
    
    Returns:
        Dictionary with manifest structure
    """
    # Normalize version (remove 'v' prefix if present)
    version_str = version.lstrip('v')
    
    # Verify files exist
    if not os.path.exists(firmware_path):
        print(f"ERROR: Firmware file not found: {firmware_path}")
        sys.exit(1)
    
    if not os.path.exists(filesystem_path):
        print(f"ERROR: Filesystem file not found: {filesystem_path}")
        sys.exit(1)
    
    # Calculate checksums and sizes
    firmware_sha256 = calculate_sha256(firmware_path)
    filesystem_sha256 = calculate_sha256(filesystem_path)
    firmware_size = get_file_size(firmware_path)
    filesystem_size = get_file_size(filesystem_path)
    
    # Get repository info
    repo = get_github_repo()
    
    # Build download URLs
    firmware_url = f"https://github.com/{repo}/releases/download/v{version_str}/firmware.bin"
    filesystem_url = f"https://github.com/{repo}/releases/download/v{version_str}/littlefs.bin"
    
    # Get current timestamp in ISO 8601 format
    release_date = datetime.now(timezone.utc).isoformat()
    
    # Build manifest structure
    manifest = {
        "latest_version": version_str,
        "firmware": {
            "version": version_str,
            "url": firmware_url,
            "size_bytes": firmware_size,
            "sha256": firmware_sha256
        },
        "filesystem": {
            "version": version_str,
            "url": filesystem_url,
            "size_bytes": filesystem_size,
            "sha256": filesystem_sha256
        },
        "release_date": release_date,
        "github_repo": repo
    }
    
    return manifest


def main():
    parser = argparse.ArgumentParser(
        description="Generate version manifest for OTA updates"
    )
    parser.add_argument(
        "--version",
        required=True,
        help="Version string (e.g., v1.0.0 or 1.0.0)"
    )
    parser.add_argument(
        "--firmware-path",
        required=True,
        help="Path to firmware.bin"
    )
    parser.add_argument(
        "--filesystem-path",
        required=True,
        help="Path to littlefs.bin"
    )
    parser.add_argument(
        "--output",
        default="version_manifest.json",
        help="Output file path (default: version_manifest.json)"
    )
    
    args = parser.parse_args()
    
    print(f"Generating version manifest...")
    print(f"  Version: {args.version}")
    print(f"  Firmware: {args.firmware_path}")
    print(f"  Filesystem: {args.filesystem_path}")
    
    # Generate manifest
    manifest = generate_manifest(
        args.version,
        args.firmware_path,
        args.filesystem_path
    )
    
    # Write manifest to file
    with open(args.output, 'w') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"\n[OK] Manifest generated successfully: {args.output}")
    print(f"\nManifest contents:")
    print(json.dumps(manifest, indent=2))
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
