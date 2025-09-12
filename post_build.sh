#!/bin/bash
#if [ "${CONFIGURATION}" = "Release" ]; then
set -euo pipefail

# --------------------------------------------------------
#        Generate macOS installer package (.pkg)
# --------------------------------------------------------

PLUGIN_NAME="${PRODUCT_NAME}"
IDENTIFIER="ee.marte.${PRODUCT_NAME}"
VERSION="${MARKETING_VERSION}-${CURRENT_PROJECT_VERSION}"

# Xcode build variables

BUILD_OUTPUT="${BUILT_PRODUCTS_DIR}/${PLUGIN_NAME}.plugin"
PKG_WORK_DIR="${BUILD_DIR}/pkg"
ROOT_DIR="${PKG_WORK_DIR}/root"
COMPONENT_PKG="${PKG_WORK_DIR}/IntermediateComponent.pkg"
FINAL_PKG="${PKG_WORK_DIR}/${PLUGIN_NAME}.pkg"
DISTRIBUTION_XML="${PKG_WORK_DIR}/distribution.xml"

codesign --force --deep --sign - "$BUILD_OUTPUT"

if [ "${CONFIGURATION}" = "Debug" ]; then
    VERSION="${VERSION}-Debug"
fi

# Staging area within the package
INSTALL_LOCATION="/tmp/FreenectTD-Installer"

# Cleanup
rm -rf "$PKG_WORK_DIR"
mkdir -p "$ROOT_DIR$INSTALL_LOCATION"

# Copy build output to staging area
ditto "$BUILD_OUTPUT" "$ROOT_DIR$INSTALL_LOCATION/$(basename "$BUILD_OUTPUT")"

# Create component package
pkgbuild \
    --root "$ROOT_DIR" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    --scripts "$SRCROOT/pkg_scripts" \
    "$COMPONENT_PKG"

# Create distribution.xml
cat > "$DISTRIBUTION_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${PLUGIN_NAME} v${VERSION}</title>
    <options allow-external-scripts="no" hostArchitectures="arm64"/>
    <allowed-os-versions>
        <os-version min="12.4"/>
    </allowed-os-versions>
    <allowed-architectures>
        <architecture>arm64</architecture>
    </allowed-architectures>
    <readme file="readme.txt"/>
    <choices-outline>
        <line choice="default"/>
    </choices-outline>
    <choice id="default" visible="false" title="${PLUGIN_NAME} ${VERSION}">
        <pkg-ref id="$IDENTIFIER"/>
    </choice>
    <pkg-ref id="$IDENTIFIER" version="$VERSION" auth="Root">$(basename "$COMPONENT_PKG")</pkg-ref>
</installer-gui-script>
EOF

# Make finale package
productbuild \
    --distribution "$DISTRIBUTION_XML" \
    --package-path "$PKG_WORK_DIR" \
    "$FINAL_PKG"

# Step 4: Rinomina e sposta in BUILT_PRODUCTS_DIR
INSTALLERS_DIR_NAME="Installers"
INSTALLERS_DIR="${BUILT_PRODUCTS_DIR}/${INSTALLERS_DIR_NAME}"
mkdir -p "${INSTALLERS_DIR}"
FINAL_NAMED_PKG="${INSTALLERS_DIR}/Install_${PLUGIN_NAME}_v${VERSION}.pkg"
mv "$FINAL_PKG" "$FINAL_NAMED_PKG"

echo "Installer created at: $FINAL_NAMED_PKG"

# Cleanup temporary files
rm -rf "$PKG_WORK_DIR"

# --------------------------------------------------------
#      Make distribution files for GitHub releases
# --------------------------------------------------------

# Directory for versioned release
VERSIONED_DIR="${BUILT_PRODUCTS_DIR}/${PLUGIN_NAME}_v${VERSION}"
mkdir -p "$VERSIONED_DIR"

# Move pkg to versioned directory
FINAL_NAMED_VERSIONED_PKG="${VERSIONED_DIR}/${PLUGIN_NAME}_v${VERSION}_Installer.pkg"
mv "$FINAL_NAMED_PKG" "$FINAL_NAMED_VERSIONED_PKG"

# Delete installer directory
rm -rf "${INSTALLERS_DIR}"

# Move .plugin to versioned directory
FINAL_NAMED_VERSIONED_PLUGIN="${VERSIONED_DIR}/${PLUGIN_NAME}.plugin"
cp -r "$BUILD_OUTPUT" "${FINAL_NAMED_VERSIONED_PLUGIN}"

# Zip the plugin itself
PLUGIN_ZIP="${VERSIONED_DIR}/${PLUGIN_NAME}_v${VERSION}.zip"
ditto -c -k --sequesterRsrc --keepParent "${FINAL_NAMED_VERSIONED_PLUGIN}" "${PLUGIN_ZIP}"

# Delete zipped plugin
rm -rf "${FINAL_NAMED_VERSIONED_PLUGIN}"

# DISABLED - Create DMG containing the pkg
#DMG_NAME="${PLUGIN_NAME}_v${VERSION}.dmg"
#DMG_PATH="${VERSIONED_DIR}/${DMG_NAME}"
#hdiutil create -volname "${PLUGIN_NAME} Installer" -srcfolder "$FINAL_NAMED_VERSIONED_PKG" -ov -format UDZO "$DMG_PATH"
#fi

exit 0
