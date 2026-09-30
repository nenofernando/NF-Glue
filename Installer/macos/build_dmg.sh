#!/bin/bash
# Builds NF Glue (VST3 + AU + PACE-signed AAX, universal arm64 + x86_64) on THIS Mac and packs a DMG.
# Nothing is uploaded anywhere: the DMG is written to ~/Desktop (override with OUT_DIR).
#
# Usage (from anywhere):   bash Installer/macos/build_dmg.sh
#
# AAX (Pro Tools): built with the AAX SDK in ~/Documents/AAX_SDK and signed with PACE's `wraptool`
# using the "NF Glue - Signing Only" wrap. Your PACE/iLok account is asked for (or set WRAP_ACCOUNT);
# the password is never stored: wraptool asks for it (or set WRAP_PASSWORD for a one-off run).
#
# Optional env vars:
#   OUT_DIR=/some/folder      where the .dmg is written (default: ~/Desktop)
#   SIGN_ID="Developer ID Application: Name (TEAMID)"   Apple code-sign identity (default "-" = ad-hoc)
#   AAX_SDK_PATH=/path        AAX SDK folder (default: ~/Documents/AAX_SDK)
#   WRAPTOOL=/path/wraptool   PACE wraptool (default: found on PATH or in the standard Eden folder)
#   WRAP_ACCOUNT=name         PACE/iLok account used to sign
#   WRAP_GUID=...             wrap GUID (default: NF Glue - Signing Only)
#   EXTRA_WRAP_ARGS="..."     any extra wraptool sign options your PACE setup needs
#   SKIP_AAX=1                build a DMG WITHOUT AAX (VST3 + AU only)
#   ARCHS="arm64"             build only this architecture (default: arm64;x86_64 universal)
#
# The version is read from CMakeLists.txt (project(NFGlue VERSION X.Y.Z ...)),
# so a version bump never needs an edit here.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CMAKE_FILE="$REPO_ROOT/CMakeLists.txt"

VERSION="$(grep -m1 -oE 'project\(NFGlue VERSION [0-9]+\.[0-9]+\.[0-9]+' "$CMAKE_FILE" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
[ -n "$VERSION" ] || { echo "Could not read the version from $CMAKE_FILE" >&2; exit 1; }

PRODUCT="NF Glue"
BUILD_DIR="$REPO_ROOT/build-glue"
STAGE_DIR="$REPO_ROOT/build-glue-dmg"
OUT_DIR="${OUT_DIR:-$HOME/Desktop}"
SIGN_ID="${SIGN_ID:--}"
AAX_SDK_PATH="${AAX_SDK_PATH:-$HOME/Documents/AAX_SDK}"
WRAP_GUID="${WRAP_GUID:-3FA9A390-BCC4-11F1-8E61-00505692C25A}"
DMG_PATH="$OUT_DIR/$PRODUCT $VERSION.dmg"
WITH_AAX=1; [ "${SKIP_AAX:-0}" = "1" ] && WITH_AAX=0

command -v cmake >/dev/null || { echo "cmake not found (brew install cmake)" >&2; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools not found (xcode-select --install)" >&2; exit 1; }

# ---- AAX prerequisites: fail early, before the long build --------------------------------------
if [ "$WITH_AAX" = "1" ]; then
  [ -d "$AAX_SDK_PATH" ] || { echo "AAX SDK not found at $AAX_SDK_PATH (set AAX_SDK_PATH, or SKIP_AAX=1 for a DMG without AAX)" >&2; exit 1; }
  if [ -z "${WRAPTOOL:-}" ]; then
    for c in "$(command -v wraptool || true)" \
             "/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool"; do
      [ -n "$c" ] && [ -x "$c" ] && WRAPTOOL="$c" && break
    done
  fi
  [ -n "${WRAPTOOL:-}" ] && [ -x "$WRAPTOOL" ] || { echo "PACE wraptool not found (set WRAPTOOL=/path/to/wraptool, or SKIP_AAX=1)" >&2; exit 1; }
  if [ -z "${WRAP_ACCOUNT:-}" ]; then
    read -r -p "PACE/iLok account for signing: " WRAP_ACCOUNT
  fi
fi

echo "==> Building $PRODUCT $VERSION"
CMAKE_ARGS=(-S "$REPO_ROOT" -B "$BUILD_DIR" -G Xcode)
[ -n "${ARCHS:-}" ] && CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=$ARCHS")
TARGETS=(NFGlue_VST3 NFGlue_AU)
if [ "$WITH_AAX" = "1" ]; then
  CMAKE_ARGS+=(-DNFGlue_ENABLE_AAX=ON "-DNFGlue_AAX_SDK_PATH=$AAX_SDK_PATH")
  TARGETS+=(NFGlue_AAX)
else
  CMAKE_ARGS+=(-DNFGlue_ENABLE_AAX=OFF)
fi
cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --config Release --target "${TARGETS[@]}"

VST3="$(find "$BUILD_DIR" -name "$PRODUCT.vst3" -type d | head -n 1)"
AU="$(find "$BUILD_DIR" -name "$PRODUCT.component" -type d | head -n 1)"
[ -d "$VST3" ] && [ -d "$AU" ] || { echo "Built plug-ins not found under $BUILD_DIR" >&2; exit 1; }

echo "==> Staging"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp -R "$VST3" "$STAGE_DIR/"
cp -R "$AU" "$STAGE_DIR/"

echo "==> Signing VST3 + AU (identity: $SIGN_ID)"
codesign --force --deep --sign "$SIGN_ID" "$STAGE_DIR/$PRODUCT.vst3"
codesign --force --deep --sign "$SIGN_ID" "$STAGE_DIR/$PRODUCT.component"

if [ "$WITH_AAX" = "1" ]; then
  AAX="$(find "$BUILD_DIR" -name "$PRODUCT.aaxplugin" -type d | head -n 1)"
  [ -d "$AAX" ] || { echo "Built AAX not found under $BUILD_DIR" >&2; exit 1; }
  echo "==> Signing AAX with PACE wraptool (wrap $WRAP_GUID)"
  WRAP_ARGS=(sign --verbose --account "$WRAP_ACCOUNT" --wcguid "$WRAP_GUID" --in "$AAX" --out "$STAGE_DIR/$PRODUCT.aaxplugin")
  [ -n "${WRAP_PASSWORD:-}" ] && WRAP_ARGS+=(--password "$WRAP_PASSWORD")
  [ "$SIGN_ID" != "-" ] && WRAP_ARGS+=(--signid "$SIGN_ID")
  # shellcheck disable=SC2086
  "$WRAPTOOL" "${WRAP_ARGS[@]}" ${EXTRA_WRAP_ARGS:-}
  [ -d "$STAGE_DIR/$PRODUCT.aaxplugin" ] || { echo "wraptool did not produce the signed AAX" >&2; exit 1; }
  # Do NOT re-run codesign on the .aaxplugin: it would break the PACE signature.
  "$WRAPTOOL" verify --in "$STAGE_DIR/$PRODUCT.aaxplugin" --verbose || echo "(wraptool verify reported a problem, check the output above)"
fi

# Drag-and-drop targets, so the DMG works like a classic installer window.
ln -s "/Library/Audio/Plug-Ins/VST3" "$STAGE_DIR/VST3 (drop NF Glue.vst3 here)"
ln -s "/Library/Audio/Plug-Ins/Components" "$STAGE_DIR/Components (drop NF Glue.component here)"
[ "$WITH_AAX" = "1" ] && ln -s "/Library/Application Support/Avid/Audio/Plug-Ins" "$STAGE_DIR/Pro Tools AAX (drop NF Glue.aaxplugin here)"

echo "==> Creating DMG"
mkdir -p "$OUT_DIR"
rm -f "$DMG_PATH"
hdiutil create -volname "$PRODUCT $VERSION" -srcfolder "$STAGE_DIR" -ov -format UDZO "$DMG_PATH" >/dev/null

echo
echo "Done: $DMG_PATH"
[ "$WITH_AAX" = "1" ] && echo "Includes: VST3, AU and the PACE-signed AAX." || echo "Includes: VST3 and AU (AAX skipped)."
echo "(If macOS blocks the plug-ins after installing an ad-hoc signed build, run:"
echo "  sudo xattr -cr \"/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3\" \"/Library/Audio/Plug-Ins/Components/$PRODUCT.component\")"
