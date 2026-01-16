#!/bin/bash
# mbtop RPM Package Builder
# Creates an .rpm installer for Fedora/RHEL/CentOS

set -e

# Configuration
APP_NAME="mbtop"
VERSION="${1:-1.0.0}"

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/rpm"
OUTPUT_DIR="${PROJECT_ROOT}/dist"

echo "=== Building mbtop ${VERSION} RPM Package ==="

# Check for rpmbuild
if ! command -v rpmbuild &> /dev/null; then
    echo "Error: rpmbuild not found. Install rpm-build package."
    exit 1
fi

# Clean previous builds
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
mkdir -p "${OUTPUT_DIR}"

# Create source tarball
echo "Creating source tarball..."
TARBALL_DIR="${BUILD_DIR}/SOURCES/${APP_NAME}-${VERSION}"
mkdir -p "${TARBALL_DIR}"

# Copy source files
cp -r "${PROJECT_ROOT}/src" "${TARBALL_DIR}/"
cp -r "${PROJECT_ROOT}/themes" "${TARBALL_DIR}/"
cp -r "${PROJECT_ROOT}/Img" "${TARBALL_DIR}/"
cp "${PROJECT_ROOT}/Makefile" "${TARBALL_DIR}/"
cp "${PROJECT_ROOT}/README.md" "${TARBALL_DIR}/"
cp "${PROJECT_ROOT}/LICENSE" "${TARBALL_DIR}/"
cp "${PROJECT_ROOT}/mbtop.desktop" "${TARBALL_DIR}/"
[ -f "${PROJECT_ROOT}/manpage.md" ] && cp "${PROJECT_ROOT}/manpage.md" "${TARBALL_DIR}/"

# Create tarball
cd "${BUILD_DIR}/SOURCES"
tar czf "${APP_NAME}-${VERSION}.tar.gz" "${APP_NAME}-${VERSION}"
rm -rf "${APP_NAME}-${VERSION}"

# Update spec file with version
sed "s/^Version:.*/Version:        ${VERSION}/" \
    "${SCRIPT_DIR}/mbtop.spec" > "${BUILD_DIR}/SPECS/mbtop.spec"

# Build RPM
echo "Building RPM..."
rpmbuild \
    --define "_topdir ${BUILD_DIR}" \
    -ba "${BUILD_DIR}/SPECS/mbtop.spec"

# Copy output
cp "${BUILD_DIR}"/RPMS/*/*.rpm "${OUTPUT_DIR}/" 2>/dev/null || true
cp "${BUILD_DIR}"/SRPMS/*.rpm "${OUTPUT_DIR}/" 2>/dev/null || true

echo ""
echo "=== Package(s) created successfully ==="
ls -la "${OUTPUT_DIR}"/*.rpm 2>/dev/null || echo "No RPM files found in output directory"
