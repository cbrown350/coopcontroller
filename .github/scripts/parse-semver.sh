#!/bin/bash
# Parse semantic versioning from conventional commits
# Usage: ./parse-semver.sh [base_version]
# Returns: MAJOR.MINOR.PATCH version bump type and new version

set -euo pipefail

BASE_VERSION="${1:-}"

# Get the latest tag
LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "")

if [ -z "$LATEST_TAG" ]; then
    echo "ERROR: No previous tags found. Cannot determine version bump." >&2
    exit 1
fi

# Extract current version components
VERSION_REGEX='v?([0-9]+)\.([0-9]+)\.([0-9]+)'
if [[ $LATEST_TAG =~ $VERSION_REGEX ]]; then
    MAJOR="${BASH_REMATCH[1]}"
    MINOR="${BASH_REMATCH[2]}"
    PATCH="${BASH_REMATCH[3]}"
else
    echo "ERROR: Could not parse version from tag: $LATEST_TAG" >&2
    exit 1
fi

# Get commits since last tag
COMMITS=$(git log "${LATEST_TAG}..HEAD" --pretty=format:"%s %b" 2>/dev/null || echo "")

if [ -z "$COMMITS" ]; then
    echo "ERROR: No commits since last tag. Nothing to release." >&2
    exit 1
fi

# Determine version bump type
BUMP_TYPE="PATCH"  # Default to patch

# Check for BREAKING CHANGE (highest priority)
if echo "$COMMITS" | grep -qi "BREAKING CHANGE"; then
    BUMP_TYPE="MAJOR"
fi

# If not major, check for feat: (middle priority)
if [ "$BUMP_TYPE" != "MAJOR" ] && echo "$COMMITS" | grep -qi "^feat"; then
    BUMP_TYPE="MINOR"
fi

# Apply version bump
case "$BUMP_TYPE" in
    MAJOR)
        NEW_MAJOR=$((MAJOR + 1))
        NEW_MINOR=0
        NEW_PATCH=0
        ;;
    MINOR)
        NEW_MINOR=$((MINOR + 1))
        NEW_PATCH=0
        NEW_MAJOR=$MAJOR
        ;;
    PATCH)
        NEW_PATCH=$((PATCH + 1))
        NEW_MAJOR=$MAJOR
        NEW_MINOR=$MINOR
        ;;
esac

NEW_VERSION="v${NEW_MAJOR}.${NEW_MINOR}.${NEW_PATCH}"

# Output results (one per line for easy parsing)
echo "BUMP_TYPE=$BUMP_TYPE"
echo "NEW_VERSION=$NEW_VERSION"
echo "MAJOR=$NEW_MAJOR"
echo "MINOR=$NEW_MINOR"
echo "PATCH=$NEW_PATCH"
