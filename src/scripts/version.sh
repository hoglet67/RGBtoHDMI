#!/bin/bash

# Announce ourselves, so it's clear that we have run
echo "Running version.sh script"

# Lookup the last commit ID
VERSION="$(git rev-parse --short HEAD)"

# Check if any uncommitted changes in tracked files
if [ -n "$(git status --untracked-files=no --porcelain)" ]; then 
  VERSION="${VERSION}?"
fi

echo -e "version:\n    ${VERSION}"

echo "#define GITVERSION \"${VERSION}\"" > gitversion.h.tmp

# Only overwrite gitversion.h if the content has changed
# This reduced unnecessary re-compilation
rsync -checksum gitversion.h.tmp gitversion.h

echo -e "gitversion.h:\n    $(cat gitversion.h)"

rm -f gitversion.h.tmp



