#!/bin/bash
# mbtop macOS Package Builder
# Creates a .pkg installer for macOS

set -e

# Configuration
APP_NAME="mbtop"
VERSION="${1:-1.0.0}"
IDENTIFIER="com.marcoberger.mbtop"
INSTALL_LOCATION="/usr/local"

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/pkg"
STAGE_DIR="${BUILD_DIR}/stage"
OUTPUT_DIR="${PROJECT_ROOT}/dist"

echo "=== Building mbtop ${VERSION} macOS Package ==="

# Clean previous builds
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${STAGE_DIR}" "${OUTPUT_DIR}"

# Build mbtop if not already built
if [ ! -f "${PROJECT_ROOT}/bin/mbtop" ]; then
    echo "Building mbtop..."
    cd "${PROJECT_ROOT}"
    make clean && make -j$(sysctl -n hw.ncpu)
fi

# Create staging directory structure
echo "Creating staging directory..."
mkdir -p "${STAGE_DIR}${INSTALL_LOCATION}/bin"
mkdir -p "${STAGE_DIR}${INSTALL_LOCATION}/share/mbtop/themes"
mkdir -p "${STAGE_DIR}${INSTALL_LOCATION}/share/doc/mbtop"
mkdir -p "${STAGE_DIR}${INSTALL_LOCATION}/share/man/man1"

# Copy files
echo "Copying files..."
cp "${PROJECT_ROOT}/bin/mbtop" "${STAGE_DIR}${INSTALL_LOCATION}/bin/"
cp -r "${PROJECT_ROOT}/themes/"* "${STAGE_DIR}${INSTALL_LOCATION}/share/mbtop/themes/"
cp "${PROJECT_ROOT}/README.md" "${STAGE_DIR}${INSTALL_LOCATION}/share/doc/mbtop/"
cp "${PROJECT_ROOT}/LICENSE" "${STAGE_DIR}${INSTALL_LOCATION}/share/doc/mbtop/"

# Generate man page if lowdown is available
if command -v lowdown &> /dev/null && [ -f "${PROJECT_ROOT}/manpage.md" ]; then
    echo "Generating man page..."
    lowdown -s -Tman -o "${STAGE_DIR}${INSTALL_LOCATION}/share/man/man1/mbtop.1" "${PROJECT_ROOT}/manpage.md"
fi

# Set permissions
chmod 755 "${STAGE_DIR}${INSTALL_LOCATION}/bin/mbtop"

# Create component package
echo "Creating component package..."
pkgbuild \
    --root "${STAGE_DIR}" \
    --identifier "${IDENTIFIER}" \
    --version "${VERSION}" \
    --install-location "/" \
    "${BUILD_DIR}/${APP_NAME}-component.pkg"

# Create distribution XML
cat > "${BUILD_DIR}/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>mbtop - Apple Silicon System Monitor</title>
    <organization>${IDENTIFIER}</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>

    <welcome file="welcome.html"/>
    <license file="LICENSE"/>
    <conclusion file="conclusion.html"/>

    <choices-outline>
        <line choice="default">
            <line choice="${IDENTIFIER}"/>
        </line>
    </choices-outline>

    <choice id="default"/>
    <choice id="${IDENTIFIER}" visible="false">
        <pkg-ref id="${IDENTIFIER}"/>
    </choice>

    <pkg-ref id="${IDENTIFIER}" version="${VERSION}" onConclusion="none">${APP_NAME}-component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Create welcome HTML
cat > "${BUILD_DIR}/welcome.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { color: #E62525; }
        .feature { margin: 10px 0; }
        .check { color: #22c55e; }
    </style>
</head>
<body>
    <h1>mbtop</h1>
    <h2>The Apple Silicon System Monitor</h2>
    <p>A powerful terminal-based system monitor optimized for Apple Silicon Macs.</p>

    <h3>Features:</h3>
    <div class="feature"><span class="check">✓</span> GPU Monitoring - Real-time utilization and temperature</div>
    <div class="feature"><span class="check">✓</span> Power Monitoring - CPU, GPU, and ANE power draw</div>
    <div class="feature"><span class="check">✓</span> ANE Monitoring - Apple Neural Engine tracking</div>
    <div class="feature"><span class="check">✓</span> VRAM Tracking - Video memory usage</div>
    <div class="feature"><span class="check">✓</span> Full Process List - Sortable and filterable</div>
    <div class="feature"><span class="check">✓</span> Network & Disk I/O - Real-time monitoring</div>

    <p>Click Continue to install mbtop to /usr/local/bin</p>
</body>
</html>
EOF

# Create conclusion HTML
cat > "${BUILD_DIR}/conclusion.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { color: #22c55e; }
        .cmd { font-family: Menlo, Monaco, monospace; background: #e2e8f0; padding: 2px 6px; border-radius: 4px; }
        .terminal { font-family: Menlo, Monaco, monospace; background: #1e293b; color: #e2e8f0; padding: 15px; border-radius: 8px; margin: 15px 0; }
    </style>
</head>
<body>
    <h1>Installation Complete!</h1>
    <p>mbtop has been successfully installed.</p>

    <h3>Getting Started:</h3>
    <p>Open Terminal and run:</p>
    <div class="terminal">$ mbtop</div>

    <h3>Configuration:</h3>
    <p>Config file: <span class="cmd">~/.config/mbtop/mbtop.conf</span></p>

    <h3>Keyboard Shortcuts:</h3>
    <p>Press <b>?</b> or <b>F1</b> in mbtop to see all shortcuts.</p>

    <p>Enjoy monitoring your Apple Silicon Mac!</p>
</body>
</html>
EOF

# Copy license
cp "${PROJECT_ROOT}/LICENSE" "${BUILD_DIR}/LICENSE"

# Create final product package
echo "Creating final package..."
productbuild \
    --distribution "${BUILD_DIR}/distribution.xml" \
    --resources "${BUILD_DIR}" \
    --package-path "${BUILD_DIR}" \
    "${OUTPUT_DIR}/${APP_NAME}-${VERSION}-macos-arm64.pkg"

echo ""
echo "=== Package created successfully ==="
echo "Location: ${OUTPUT_DIR}/${APP_NAME}-${VERSION}-macos-arm64.pkg"
echo ""

# Show package info
pkgutil --payload-files "${OUTPUT_DIR}/${APP_NAME}-${VERSION}-macos-arm64.pkg" 2>/dev/null | head -20 || true
