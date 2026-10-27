#!/bin/bash

# Announce ourselves, so it's clear that we have run
echo "Running version.sh script"
set -e

# Lookup the last commit ID
VERSION="$(date +"%Y%m%d") $(git rev-parse --short HEAD)"
GITVERSION_FILE="${1:-gitversion.h}"
GITVERSION_TEMP="${GITVERSION_FILE}.tmp"

# Check if any uncommitted changes in tracked files
if [ -n "$(git status --untracked-files=no --porcelain)" ]; then 
  VERSION="${VERSION}?"
fi

echo -e "version:\n    ${VERSION}"

echo "#define GITVERSION \"${VERSION}\"" > "$GITVERSION_TEMP"

# Only overwrite gitversion.h if the content has changed
# This reduced unnecessary re-compilation
rsync --checksum "$GITVERSION_TEMP" "$GITVERSION_FILE"

echo -e "gitversion.h:\n    $(cat "$GITVERSION_FILE")"

rm -f "$GITVERSION_TEMP"

