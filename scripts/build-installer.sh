#!/bin/bash
# Build a signed and notarized .pkg installer for Fart Blaster 3000.
#
# Inputs (env vars or first arg):
#   APPLE_INSTALLER_ID  - "Developer ID Installer: NAME (TEAMID)" (required for signing)
#   NOTARY_PROFILE      - notarytool keychain profile name (default: fartco-notary)
#   VERSION             - package version (default: read from CMakeLists.txt)
#
# Expects the Release build to already exist at:
#   build/FartBlaster_artefacts/Release/{VST3,AU,Standalone}/
#
# Outputs:
#   dist/FartBlaster3000-Installer.pkg  (signed, notarized, stapled if cert provided)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${REPO_ROOT}/build/FartBlaster_artefacts/Release"
DIST_DIR="${REPO_ROOT}/dist"
INSTALLER_DIR="${REPO_ROOT}/installer"
STAGE_DIR="${REPO_ROOT}/build/installer-stage"
PKG_NAME="Fart Blaster 3000"
APPLE_INSTALLER_ID="${APPLE_INSTALLER_ID:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-fartco-notary}"

# Pull version from CMakeLists.txt if not provided.
if [ -z "${VERSION:-}" ]; then
    VERSION=$(grep -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1 | awk '{print $2}')
fi
echo "==> Building installer for v${VERSION}"

# Sanity-check that the signed bundles exist.
for f in "VST3/${PKG_NAME}.vst3" "AU/${PKG_NAME}.component" "Standalone/${PKG_NAME}.app"; do
    if [ ! -d "${BUILD_DIR}/${f}" ]; then
        echo "ERROR: missing build artifact ${BUILD_DIR}/${f}" >&2
        echo "Run cmake build with -DAPPLE_DEV_ID=... first." >&2
        exit 1
    fi
done

mkdir -p "${STAGE_DIR}" "${DIST_DIR}"
rm -rf "${STAGE_DIR}"/*

# Stage payload roots -- one per component pkg. The directory passed to
# pkgbuild --root will be mirrored into the install-location.
mkdir -p "${STAGE_DIR}/payload/VST3" \
         "${STAGE_DIR}/payload/AU"   \
         "${STAGE_DIR}/payload/App"
/usr/bin/ditto "${BUILD_DIR}/VST3/${PKG_NAME}.vst3"       "${STAGE_DIR}/payload/VST3/${PKG_NAME}.vst3"
/usr/bin/ditto "${BUILD_DIR}/AU/${PKG_NAME}.component"    "${STAGE_DIR}/payload/AU/${PKG_NAME}.component"
/usr/bin/ditto "${BUILD_DIR}/Standalone/${PKG_NAME}.app"  "${STAGE_DIR}/payload/App/${PKG_NAME}.app"

# Strip extended attributes that can break notarization.
xattr -cr "${STAGE_DIR}/payload"

echo "==> Building component packages"
mkdir -p "${STAGE_DIR}/components"

# The preinstall script lives in installer/scripts and runs for every component.
SCRIPTS_DIR="${INSTALLER_DIR}/scripts"

pkgbuild --root "${STAGE_DIR}/payload/VST3" \
         --identifier "com.FartCo.FartBlaster.vst3" \
         --version "${VERSION}" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         --scripts "${SCRIPTS_DIR}" \
         "${STAGE_DIR}/components/FartBlaster-VST3.pkg"

pkgbuild --root "${STAGE_DIR}/payload/AU" \
         --identifier "com.FartCo.FartBlaster.au" \
         --version "${VERSION}" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         --scripts "${SCRIPTS_DIR}" \
         "${STAGE_DIR}/components/FartBlaster-AU.pkg"

pkgbuild --root "${STAGE_DIR}/payload/App" \
         --identifier "com.FartCo.FartBlaster.app" \
         --version "${VERSION}" \
         --install-location "/Applications" \
         --scripts "${SCRIPTS_DIR}" \
         "${STAGE_DIR}/components/FartBlaster-App.pkg"

echo "==> Composing distribution package"
UNSIGNED_PKG="${STAGE_DIR}/FartBlaster3000-unsigned.pkg"
FINAL_PKG="${DIST_DIR}/FartBlaster3000-Installer.pkg"

productbuild --distribution "${INSTALLER_DIR}/distribution.xml" \
             --package-path "${STAGE_DIR}/components" \
             --resources "${INSTALLER_DIR}/resources" \
             "${UNSIGNED_PKG}"

if [ -n "${APPLE_INSTALLER_ID}" ]; then
    echo "==> Signing installer with: ${APPLE_INSTALLER_ID}"
    productsign --sign "${APPLE_INSTALLER_ID}" --timestamp \
                "${UNSIGNED_PKG}" "${FINAL_PKG}"
    pkgutil --check-signature "${FINAL_PKG}"

    echo "==> Notarizing installer"
    xcrun notarytool submit "${FINAL_PKG}" \
        --keychain-profile "${NOTARY_PROFILE}" --wait

    echo "==> Stapling installer"
    xcrun stapler staple "${FINAL_PKG}"
    xcrun stapler validate "${FINAL_PKG}"
else
    echo "==> APPLE_INSTALLER_ID not set; producing UNSIGNED installer"
    cp "${UNSIGNED_PKG}" "${FINAL_PKG}"
fi

echo ""
echo "==> Done"
ls -lh "${FINAL_PKG}"
