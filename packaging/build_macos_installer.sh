#!/usr/bin/env bash
# Builds a Universal (Apple Silicon + Intel) Release of Chordify and packages it as
# packaging/Output/Chordify-<version>-macOS.pkg, with VST3, AU and the standalone app as separate,
# optional choices.
#
# Signing and notarization only happen when their credentials are provided, so the script also
# works before you have an Apple Developer account (the .pkg is then unsigned: fine for your own
# machines, but Gatekeeper warns anyone you send it to):
#
#   DEVELOPER_ID_APPLICATION="Developer ID Application: Your Name (TEAMID)"  signs plug-ins + app
#   DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (TEAMID)"      signs the .pkg
#   NOTARY_PROFILE=<name>   a keychain profile from `xcrun notarytool store-credentials <name>`;
#                           notarizes the signed .pkg and staples the ticket
#
# Usage: packaging/build_macos_installer.sh [--skip-build]

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
PRODUCT_NAME="Chordify"
BUNDLE_ID="com.duphon.chordify"
VERSION="$(tr -d '[:space:]' < VERSION)"
BUILD_DIR="$ROOT/cmake-build-installer"
ARTEFACTS="$BUILD_DIR/${PRODUCT_NAME}_artefacts/Release"
OUT="$ROOT/packaging/Output"
STAGE="$OUT/stage"
PKG="$OUT/${PRODUCT_NAME}-${VERSION}-macOS.pkg"

step() { printf '\n==> %s\n' "$*"; }

# cmake and ninja: use the ones on PATH, or CLion's bundled copies
ARCH="$(uname -m)"; [ "$ARCH" = "arm64" ] && ARCH="aarch64"
command -v cmake > /dev/null || export PATH="/Applications/CLion.app/Contents/bin/cmake/mac/$ARCH/bin:$PATH"
command -v ninja > /dev/null || export PATH="/Applications/CLion.app/Contents/bin/ninja/mac/$ARCH:$PATH"

if [ "${1:-}" != "--skip-build" ]; then
    step "Building $PRODUCT_NAME $VERSION (Release, arm64 + x86_64)"
    cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" > /dev/null
    cmake --build "$BUILD_DIR" --target "${PRODUCT_NAME}_VST3" "${PRODUCT_NAME}_AU" "${PRODUCT_NAME}_Standalone"
fi

rm -rf "$STAGE" "$PKG"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
# Copy without resource forks, Finder info or quarantine attributes, which codesign rejects
# ("detritus not allowed"). macOS may still attach com.apple.provenance to the new files; that one
# can't be removed, is harmless, and pkgbuild carries it along as ._ entries.
for pair in "VST3/$PRODUCT_NAME.vst3:vst3" "AU/$PRODUCT_NAME.component:au" "Standalone/$PRODUCT_NAME.app:app"; do
    ditto --norsrc --noextattr --noqtn "$ARTEFACTS/${pair%%:*}" "$STAGE/${pair##*:}/$(basename "${pair%%:*}")"
done

if [ -n "${DEVELOPER_ID_APPLICATION:-}" ]; then
    step "Signing with $DEVELOPER_ID_APPLICATION"
    ENTITLEMENTS="$BUILD_DIR/${PRODUCT_NAME}_artefacts/JuceLibraryCode"
    codesign --force --timestamp --options runtime --sign "$DEVELOPER_ID_APPLICATION" \
        --entitlements "$ENTITLEMENTS/${PRODUCT_NAME}_VST3.entitlements" "$STAGE/vst3/$PRODUCT_NAME.vst3"
    codesign --force --timestamp --options runtime --sign "$DEVELOPER_ID_APPLICATION" \
        --entitlements "$ENTITLEMENTS/${PRODUCT_NAME}_AU.entitlements" "$STAGE/au/$PRODUCT_NAME.component"
    codesign --force --timestamp --options runtime --sign "$DEVELOPER_ID_APPLICATION" \
        --entitlements "$ENTITLEMENTS/${PRODUCT_NAME}_Standalone.entitlements" "$STAGE/app/$PRODUCT_NAME.app"
    for bundle in "$STAGE"/*/*; do codesign --verify --deep --strict "$bundle"; done
else
    step "DEVELOPER_ID_APPLICATION not set: plug-ins and app stay ad-hoc signed (not for distribution)"
fi

# One component package per format. Bundles are marked non-relocatable: by default the macOS
# installer "updates" any copy of a bundle it finds elsewhere on disk (e.g. a dev build in
# ~/Library/Audio/Plug-Ins) instead of installing to the location given here.
component_pkg() {
    local name="$1" location="$2"
    pkgbuild --analyze --root "$STAGE/$name" "$STAGE/$name.plist" > /dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/$name.plist"
    pkgbuild --quiet --root "$STAGE/$name" --component-plist "$STAGE/$name.plist" \
        --identifier "$BUNDLE_ID.$name.pkg" --version "$VERSION" --install-location "$location" \
        "$STAGE/$PRODUCT_NAME.$name.pkg"
}

step "Building component packages"
component_pkg vst3 "/Library/Audio/Plug-Ins/VST3"
component_pkg au "/Library/Audio/Plug-Ins/Components"
component_pkg app "/Applications"

step "Building $PKG"
sed -e "s/\${PRODUCT_NAME}/$PRODUCT_NAME/g" -e "s/\${BUNDLE_ID}/$BUNDLE_ID/g" -e "s/\${VERSION}/$VERSION/g" \
    packaging/distribution.xml.template > "$STAGE/distribution.xml"

SIGN_ARGS=()
if [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
    SIGN_ARGS=(--sign "$DEVELOPER_ID_INSTALLER" --timestamp)
else
    echo "DEVELOPER_ID_INSTALLER not set: the .pkg is unsigned"
fi
productbuild --quiet --distribution "$STAGE/distribution.xml" --resources packaging/resources \
    --package-path "$STAGE" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG"

if [ -n "${NOTARY_PROFILE:-}" ]; then
    if [ -z "${DEVELOPER_ID_INSTALLER:-}" ] || [ -z "${DEVELOPER_ID_APPLICATION:-}" ]; then
        echo "error: notarization needs both DEVELOPER_ID_APPLICATION and DEVELOPER_ID_INSTALLER" >&2
        exit 1
    fi
    step "Notarizing (this can take a few minutes)"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
    spctl --assess --type install --verbose "$PKG"
else
    step "NOTARY_PROFILE not set: skipping notarization"
fi

cp packaging/uninstall_macos.sh "$OUT/Uninstall $PRODUCT_NAME.command"
chmod +x "$OUT/Uninstall $PRODUCT_NAME.command"
rm -rf "$STAGE"

step "Done"
echo "$PKG"
echo "$OUT/Uninstall $PRODUCT_NAME.command"
