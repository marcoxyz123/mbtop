#!/bin/bash
# mbtop Debian Package Builder
# Creates a .deb installer for Debian/Ubuntu

set -e

# Configuration
APP_NAME="mbtop"
VERSION="${1:-1.0.0}"
ARCH="${2:-amd64}"

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/deb"
PKG_DIR="${BUILD_DIR}/${APP_NAME}_${VERSION}_${ARCH}"
OUTPUT_DIR="${PROJECT_ROOT}/dist"

echo "=== Building mbtop ${VERSION} Debian Package (${ARCH}) ==="

# Clean previous builds
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"

# Create package directory structure
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/bin"
mkdir -p "${PKG_DIR}/usr/share/mbtop/themes"
mkdir -p "${PKG_DIR}/usr/share/doc/mbtop"
mkdir -p "${PKG_DIR}/usr/share/man/man1"
mkdir -p "${PKG_DIR}/usr/share/applications"
mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/48x48/apps"
mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps"

# Build mbtop if not already built
if [ ! -f "${PROJECT_ROOT}/bin/mbtop" ]; then
    echo "Building mbtop..."
    cd "${PROJECT_ROOT}"
    make clean && make -j$(nproc)
fi

# Copy binary
echo "Copying files..."
cp "${PROJECT_ROOT}/bin/mbtop" "${PKG_DIR}/usr/bin/"
chmod 755 "${PKG_DIR}/usr/bin/mbtop"

# Copy themes
cp -r "${PROJECT_ROOT}/themes/"* "${PKG_DIR}/usr/share/mbtop/themes/"

# Copy documentation
cp "${PROJECT_ROOT}/README.md" "${PKG_DIR}/usr/share/doc/mbtop/"
cp "${PROJECT_ROOT}/LICENSE" "${PKG_DIR}/usr/share/doc/mbtop/copyright"

# Copy desktop file and icons
cp "${PROJECT_ROOT}/mbtop.desktop" "${PKG_DIR}/usr/share/applications/"
cp "${PROJECT_ROOT}/Img/icon.png" "${PKG_DIR}/usr/share/icons/hicolor/48x48/apps/mbtop.png"
cp "${PROJECT_ROOT}/Img/icon.svg" "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps/mbtop.svg"

# Generate man page if lowdown is available
if command -v lowdown &> /dev/null && [ -f "${PROJECT_ROOT}/manpage.md" ]; then
    echo "Generating man page..."
    lowdown -s -Tman -o "${PKG_DIR}/usr/share/man/man1/mbtop.1" "${PROJECT_ROOT}/manpage.md"
    gzip -9 "${PKG_DIR}/usr/share/man/man1/mbtop.1"
fi

# Update control file with correct version and architecture
sed -e "s/^Version:.*/Version: ${VERSION}/" \
    -e "s/^Architecture:.*/Architecture: ${ARCH}/" \
    "${SCRIPT_DIR}/DEBIAN/control" > "${PKG_DIR}/DEBIAN/control"

# Calculate installed size
INSTALLED_SIZE=$(du -sk "${PKG_DIR}" | cut -f1)
echo "Installed-Size: ${INSTALLED_SIZE}" >> "${PKG_DIR}/DEBIAN/control"

# Create postinst script
cat > "${PKG_DIR}/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e
# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
# Update desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 755 "${PKG_DIR}/DEBIAN/postinst"

# Create postrm script
cat > "${PKG_DIR}/DEBIAN/postrm" << 'EOF'
#!/bin/sh
set -e
# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "${PKG_DIR}/DEBIAN/postrm"

# Build the package
echo "Building package..."
dpkg-deb --build --root-owner-group "${PKG_DIR}"

# Move to output directory
mv "${BUILD_DIR}/${APP_NAME}_${VERSION}_${ARCH}.deb" "${OUTPUT_DIR}/"

echo ""
echo "=== Package created successfully ==="
echo "Location: ${OUTPUT_DIR}/${APP_NAME}_${VERSION}_${ARCH}.deb"
echo ""

# Show package info
dpkg-deb --info "${OUTPUT_DIR}/${APP_NAME}_${VERSION}_${ARCH}.deb"
