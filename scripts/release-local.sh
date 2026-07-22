#!/bin/bash
# One-shot LOCAL release build for Fart Blaster 3000.
#
# Run this in Terminal ON the Mac — codesign and notarytool need the GUI
# session's unlocked login keychain, so it cannot run over SSH.
#
# Produces:
#   dist/FartBlaster3000-Installer.pkg  (Developer ID signed + notarized + stapled)
#   dist/FartBlaster3000-macOS.zip      (the three signed bundles, manual install)
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
cd "$(dirname "$0")/.."

DEV_ID="Developer ID Application: MICHAEL KEITH LEWIS (824BB5B8RQ)"
INSTALLER_ID="Developer ID Installer: MICHAEL KEITH LEWIS (824BB5B8RQ)"
TEAM_ID="824BB5B8RQ"

echo "==> Configuring signed Release build"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      "-DAPPLE_DEV_ID=${DEV_ID}" "-DAPPLE_TEAM_ID=${TEAM_ID}" >/dev/null

echo "==> Building (signed, hardened runtime)"
cmake --build build -j8

APPLE_INSTALLER_ID="${INSTALLER_ID}" ./scripts/build-installer.sh

echo "==> Zipping signed bundles for manual install"
R="build/FartBlaster_artefacts/Release"
STAGE="build/zip-stage"
rm -rf "${STAGE}"
mkdir -p "${STAGE}" dist
/usr/bin/ditto "${R}/VST3/Fart Blaster 3000.vst3"      "${STAGE}/Fart Blaster 3000.vst3"
/usr/bin/ditto "${R}/AU/Fart Blaster 3000.component"   "${STAGE}/Fart Blaster 3000.component"
/usr/bin/ditto "${R}/Standalone/Fart Blaster 3000.app" "${STAGE}/Fart Blaster 3000.app"
rm -f dist/FartBlaster3000-macOS.zip
(cd "${STAGE}" && /usr/bin/zip -qry "../../dist/FartBlaster3000-macOS.zip" .)

echo ""
echo "==> Release artifacts ready:"
ls -lh dist/
