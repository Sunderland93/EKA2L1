#!/bin/bash
# Packages the Harmattan build into an installable .deb for the Nokia N9.
#
# The N9 (MeeGo Harmattan 1.2) accepts standard Debian packages when
# "Developer Mode" is enabled in Settings > Security. No Aegis manifest is
# shipped: an unsigned package installs with the default credential set,
# which is sufficient for a user-space emulator (EGL, PulseAudio socket,
# files under /opt and $HOME are all accessible without extra credentials).
#
# Every library the binary needs (Qt 4.7, unversioned SGX EGL/GLES2 libs,
# X11, eglibc 2.10) is part of the base image on every N9, and the C++
# runtime is linked statically (see src/emu/harmattan/CMakeLists.txt), so
# the control file deliberately carries no Depends line.
#
# Usage: scripts/package_harmattan_deb.sh [build dir]   (default: build-harmattan)
set -e

BUILD_DIR="${1:-build-harmattan}"
VERSION="${EKA2L1_DEB_VERSION:-0.0.1-1}"
ARCH="armel"   # Harmattan's dpkg architecture (armv7 NEON userland)

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${BUILD_DIR}/deb/eka2l1"
DEB="${BUILD_DIR}/deb/eka2l1_${VERSION}_${ARCH}.deb"

if [ ! -x "${BUILD_DIR}/bin/eka2l1" ]; then
    echo "error: ${BUILD_DIR}/bin/eka2l1 not found - build it first" >&2
    exit 1
fi
if [ ! -f "${BUILD_DIR}/bin/panic.json" ]; then
    echo "error: ${BUILD_DIR}/bin/panic.json not found - POST_BUILD staging incomplete?" >&2
    exit 1
fi

command -v dpkg-deb >/dev/null || { echo "error: dpkg-deb not available" >&2; exit 1; }
command -v convert >/dev/null || { echo "error: ImageMagick (convert) not available" >&2; exit 1; }

# Cross strip lives with the toolchain recorded by the build tree.
TOOLCHAIN="$(sed -n 's/^HARMATTAN_TOOLCHAIN:PATH=//p' "${BUILD_DIR}/CMakeCache.txt")"
if [ -z "${TOOLCHAIN}" ]; then
    TOOLCHAIN="${HARMATTAN_TOOLCHAIN:-$HOME/Dev/eka2l1-harmattan}"
fi
STRIP="${TOOLCHAIN}/bin/arm-linux-gnueabihf-strip"
if [ ! -x "${STRIP}" ]; then
    echo "error: cross strip not found at ${STRIP}" >&2
    exit 1
fi

# ---------------------------------------------------------------- staging ---
rm -rf "${BUILD_DIR}/deb"
install -d -m 0755 "${STAGE}/DEBIAN" \
                   "${STAGE}/opt/eka2l1/bin" \
                   "${STAGE}/usr/share/applications" \
                   "${STAGE}/usr/share/icons/hicolor/64x64/apps" \
                   "${STAGE}/usr/share/icons/hicolor/128x128/apps"

# Application tree: everything resolves relative to the binary's directory
# (runtime resource root, shaders, banks, panic.json). The statically linked
# libstdc++ carries debug info, so the binary is stripped for the device.
install -m 0755 "${BUILD_DIR}/bin/eka2l1"   "${STAGE}/opt/eka2l1/bin/eka2l1"
"${STRIP}" --strip-unneeded "${STAGE}/opt/eka2l1/bin/eka2l1"
install -m 0644 "${BUILD_DIR}/bin/panic.json" "${STAGE}/opt/eka2l1/bin/panic.json"
cp -a "${BUILD_DIR}/bin/resources" "${STAGE}/opt/eka2l1/bin/resources"
chmod -R u+rwX,go+rX,go-w "${STAGE}/opt/eka2l1/bin/resources"
rm -f "${STAGE}/opt/eka2l1/bin/resources/upscale/.git"

# Launcher icon, generated from the desktop frontend's duck tank artwork.
convert "${REPO_ROOT}/src/emu/qt/assets/duck_tank.png" -resize 64x64 \
    "${STAGE}/usr/share/icons/hicolor/64x64/apps/eka2l1.png"
convert "${REPO_ROOT}/src/emu/qt/assets/duck_tank.png" -resize 128x128 \
    "${STAGE}/usr/share/icons/hicolor/128x128/apps/eka2l1.png"

cat > "${STAGE}/usr/share/applications/eka2l1.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=EKA2L1
Comment=Symbian OS emulator
Exec=/opt/eka2l1/bin/eka2l1
Icon=eka2l1
Terminal=false
Categories=Game;
EOF
chmod 0644 "${STAGE}/usr/share/applications/eka2l1.desktop" \
           "${STAGE}/usr/share/icons/hicolor/"*"/apps/eka2l1.png"

# ---------------------------------------------------------------- control ---
cat > "${STAGE}/DEBIAN/control" <<EOF
Package: eka2l1
Version: ${VERSION}
Section: games
Priority: optional
Architecture: ${ARCH}
Maintainer: EKA2L1 Team
Homepage: https://github.com/EKA2L1/EKA2L1
Description: Symbian OS emulator for MeeGo Harmattan
 EKA2L1 is an experimental Symbian OS emulator. This package contains the
 native Qt 4 frontend with the OpenGL ES 2.0 graphics backend for the
 Nokia N9.
EOF

# ------------------------------------------------------------------ build ---
# Harmattan's dpkg (1.15.x) predates xz deb members and rejects anything but
# gzip ("contains ununderstood data member control.tar.xz"), so gzip is
# forced for both the control and data members.
dpkg-deb --root-owner-group -Zgzip -z6 -b "${STAGE}" "${DEB}"

echo
echo "built: ${DEB}"
dpkg-deb -I "${DEB}" | sed -n '1,12p'
echo
echo "contents:"
dpkg-deb -c "${DEB}" | awk '{print $1, $6}' | sed -n '1,12p'
