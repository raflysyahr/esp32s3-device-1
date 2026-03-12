#!/bin/bash

set -e

########################################
# VERSION SYSTEM
########################################

VERSION_FILE="VERSION"
BUILD_FILE="BUILD"
CHANGELOG_FILE="CHANGELOG.md"

if [ ! -f "$VERSION_FILE" ]; then
    echo "0.1.0" > "$VERSION_FILE"
fi

if [ ! -f "$BUILD_FILE" ]; then
    echo "0" > "$BUILD_FILE"
fi

CURRENT_VERSION=$(cat "$VERSION_FILE")
BUILD_NUMBER=$(cat "$BUILD_FILE")

IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

echo ""
echo "📦 Current Version : $CURRENT_VERSION"
echo "🔧 Current Build   : $BUILD_NUMBER"
echo ""

read -p "Apakah ini update MAJOR? (y/n): " UPDATE_MAJOR
read -p "Apakah ini update MINOR? (y/n): " UPDATE_MINOR
read -p "Apakah ini update PATCH? (y/n): " UPDATE_PATCH

UPDATE_MAJOR=$(echo "$UPDATE_MAJOR" | tr '[:upper:]' '[:lower:]')
UPDATE_MINOR=$(echo "$UPDATE_MINOR" | tr '[:upper:]' '[:lower:]')
UPDATE_PATCH=$(echo "$UPDATE_PATCH" | tr '[:upper:]' '[:lower:]')

if [[ "$UPDATE_MAJOR" == "y" ]]; then
    MAJOR=$((MAJOR+1))
    MINOR=0
    PATCH=0
elif [[ "$UPDATE_MINOR" == "y" ]]; then
    MINOR=$((MINOR+1))
    PATCH=0
elif [[ "$UPDATE_PATCH" == "y" ]]; then
    PATCH=$((PATCH+1))
fi

NEW_VERSION="$MAJOR.$MINOR.$PATCH"

BUILD_NUMBER=$((BUILD_NUMBER+1))

echo "$NEW_VERSION" > "$VERSION_FILE"
echo "$BUILD_NUMBER" > "$BUILD_FILE"

FULL_VERSION="${NEW_VERSION}+${BUILD_NUMBER}"

echo ""
echo "🚀 New Version : $FULL_VERSION"
echo ""

########################################
# CHANGELOG
########################################

DATE_NOW=$(date +"%Y-%m-%d")

if [ ! -f "$CHANGELOG_FILE" ]; then
    echo "# Changelog" > "$CHANGELOG_FILE"
fi

read -p "📝 Deskripsi perubahan: " CHANGE_DESC

{
echo ""
echo "## v$FULL_VERSION - $DATE_NOW"
echo "- $CHANGE_DESC"
} >> "$CHANGELOG_FILE"

########################################
# OTA VERSION FILE
########################################

echo "$FULL_VERSION" > firmware_version.txt

########################################
# GIT CHECK
########################################

echo "🔎 Checking git repository..."

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git init
fi

########################################
# GITHUB LOGIN
########################################

if command -v gh >/dev/null 2>&1; then
    if ! gh auth status >/dev/null 2>&1; then
        echo "🔐 GitHub belum login"
        gh auth login
    fi
else
    echo "⚠️ GitHub CLI belum terinstall"
    exit 1
fi

########################################
# REMOTE CHECK
########################################

REMOTE=$(git remote)

if [ -z "$REMOTE" ]; then
    read -p "🌐 URL Git repository: " GIT_URL
    git remote add origin "$GIT_URL"
fi

########################################
# BRANCH SETUP
########################################

CURRENT_BRANCH=$(git branch --show-current)

if [ -z "$CURRENT_BRANCH" ]; then
    git checkout -b main
    BRANCH="main"
else
    BRANCH=$CURRENT_BRANCH
fi

echo "📌 Branch: $BRANCH"

########################################
# ESP-IDF WORKFLOW CHECK
########################################

ESP_IDF_PROJECT=false

if [ -f "CMakeLists.txt" ] && [ -d "main" ]; then
    ESP_IDF_PROJECT=true
fi

if [ "$ESP_IDF_PROJECT" = true ]; then

WORKFLOW_FILE=".github/workflows/esp-idf-build.yml"

if [ ! -f "$WORKFLOW_FILE" ]; then

mkdir -p .github/workflows

cat << 'EOF' > $WORKFLOW_FILE
name: ESP-IDF Build

on:
  push:
    branches: ["*"]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: latest

      - name: Build
        run: |
          . $IDF_PATH/export.sh
          idf.py build

      - name: Upload firmware
        uses: actions/upload-artifact@v4
        with:
          name: firmware
          path: build/*.bin
EOF

fi

fi

########################################
# COMMIT
########################################

git add .

read -p "📝 Commit message (optional): " MSG
MSG=${MSG:-"release v$FULL_VERSION"}

git commit -m "$MSG" || echo "⚠️ Tidak ada perubahan"

########################################
# TAG
########################################

TAG="v$FULL_VERSION"
git tag "$TAG"

########################################
# PUSH
########################################

git push -u origin "$BRANCH"
git push origin "$TAG"

########################################
# WAIT BUILD
########################################

echo "⏳ Menunggu workflow..."

sleep 5

RUN_ID=$(gh run list --limit 1 --json databaseId -q '.[0].databaseId')

if [ -z "$RUN_ID" ]; then
    echo "❌ Workflow tidak ditemukan"
    exit 1
fi

gh run watch $RUN_ID

STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    echo "❌ Build FAILED"
    exit 1
fi

echo "✅ Build SUCCESS"
