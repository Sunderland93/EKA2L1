#!/usr/bin/env bash
#
# Builds a modern ARM cross toolchain that targets MeeGo Harmattan 1.2
# (Nokia N9).
#
# Why this is needed: the Qt SDK that ships with Harmattan (Madde) only
# provides GCC 4.4.1, which cannot compile the C++17/20 code EKA2L1 uses.
# The toolchain produced here is a plain GCC 14 cross compiler configured
# against the Harmattan sysroot, so the generated libstdc++ only references
# symbols that exist in the device's Maemo eglibc 2.10.
#
# The script does not need root: bison and flex are staged into a private
# prefix with "apt-get download" + "dpkg -x" when they are missing.
#
# Usage:
#   scripts/build_harmattan_toolchain.sh [prefix]
#
# Environment:
#   QTSDK_DIR            - Qt SDK root (default: $HOME/Dev/QtSDK)
#   HARMATTAN_SYSROOT    - sysroot to build against
#   GCC_VERSION          - GCC version to build (default: 14.2.0)
#   BINUTILS_VERSION     - binutils version to build (default: 2.42)
#   JOBS                 - parallel make jobs (default: nproc)

set -euo pipefail

PREFIX="${1:-${HARMATTAN_TOOLCHAIN:-$HOME/Dev/eka2l1-harmattan}}"
QTSDK_DIR="${QTSDK_DIR:-$HOME/Dev/QtSDK}"
GCC_VERSION="${GCC_VERSION:-14.2.0}"
BINUTILS_VERSION="${BINUTILS_VERSION:-2.42}"
JOBS="${JOBS:-$(nproc)}"
TARGET="arm-linux-gnueabihf"

if [ -z "${HARMATTAN_SYSROOT:-}" ]; then
    HARMATTAN_SYSROOT="$QTSDK_DIR/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim"
fi

if [ ! -d "$HARMATTAN_SYSROOT" ]; then
    echo "error: Harmattan sysroot not found at '$HARMATTAN_SYSROOT'" >&2
    echo "       set HARMATTAN_SYSROOT or QTSDK_DIR" >&2
    exit 1
fi

SRC_DIR="$PREFIX/src"
BUILD_DIR="$PREFIX/build"
TOOLS_DIR="$PREFIX/tools"

mkdir -p "$SRC_DIR" "$BUILD_DIR" "$TOOLS_DIR"

echo "==> prefix:      $PREFIX"
echo "==> sysroot:     $HARMATTAN_SYSROOT"
echo "==> gcc version: $GCC_VERSION"

# ---------------------------------------------------------------------------
# bison / flex are required to build GCC. They are frequently missing from
# minimal systems and cannot be installed without root, so they are unpacked
# into a private prefix instead.
# ---------------------------------------------------------------------------
need_build_tools=0
command -v bison >/dev/null 2>&1 || need_build_tools=1
command -v flex >/dev/null 2>&1 || need_build_tools=1

if [ "$need_build_tools" = "1" ]; then
    echo "==> staging bison/flex into $TOOLS_DIR"
    if [ ! -x "$TOOLS_DIR/usr/bin/bison" ] || [ ! -x "$TOOLS_DIR/usr/bin/flex" ]; then
        stage_dir="$(mktemp -d)"
        (
            cd "$stage_dir"
            apt-get download bison flex
            for deb in ./*.deb; do
                dpkg -x "$deb" "$TOOLS_DIR"
            done
        )
        rm -rf "$stage_dir"
    fi
    export PATH="$TOOLS_DIR/usr/bin:$PATH"
fi

bison --version | head -1
flex --version

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
fetch() {
    local url="$1" dest="$2"
    if [ ! -f "$dest" ]; then
        echo "==> fetching $url"
        curl -sSL -O "$url" -o "$dest.tmp" && mv "$dest.tmp" "$dest"
    fi
    if [ ! -d "$dest" ]; then
        tar xf "$dest" -C "$SRC_DIR"
    fi
}

fetch "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz" \
      "$SRC_DIR/binutils-$BINUTILS_VERSION.tar.xz"
fetch "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" \
      "$SRC_DIR/gcc-$GCC_VERSION.tar.xz"

# ---------------------------------------------------------------------------
# binutils
# ---------------------------------------------------------------------------
if [ ! -x "$PREFIX/bin/$TARGET-ld" ]; then
    echo "==> building binutils $BINUTILS_VERSION"
    mkdir -p "$BUILD_DIR/binutils"
    (
        cd "$BUILD_DIR/binutils"
        "$SRC_DIR/binutils-$BINUTILS_VERSION/configure" \
            --target="$TARGET" \
            --prefix="$PREFIX" \
            --with-sysroot="$HARMATTAN_SYSROOT" \
            --with-arch=armv7-a \
            --with-float=hard \
            --with-abi=aapcs-linux \
            --disable-nls \
            --disable-werror \
            --disable-gdb \
            --disable-sim \
            --disable-readline \
            --disable-libdecnumber
        make -j"$JOBS"
        make install
    )
fi

# ---------------------------------------------------------------------------
# GCC
# ---------------------------------------------------------------------------
if [ ! -x "$PREFIX/bin/$TARGET-g++" ]; then
    echo "==> building gcc $GCC_VERSION"
    mkdir -p "$BUILD_DIR/gcc"
    (
        cd "$BUILD_DIR/gcc"
        "$SRC_DIR/gcc-$GCC_VERSION/configure" \
            --target="$TARGET" \
            --prefix="$PREFIX" \
            --with-sysroot="$HARMATTAN_SYSROOT" \
            --with-headers="$HARMATTAN_SYSROOT/usr/include" \
            --enable-languages=c,c++ \
            --with-arch=armv7-a \
            --with-tune=cortex-a8 \
            --with-fpu=vfpv3-d16 \
            --with-float=hard \
            --with-abi=aapcs-linux \
            --disable-nls \
            --disable-multilib \
            --disable-bootstrap \
            --enable-libstdcxx-time=rt \
            --disable-libquadmath \
            --disable-libstdcxx-pch
        make -j"$JOBS"
        make install
    )
fi

# ---------------------------------------------------------------------------
# Runtime staging directory: a copy of the sysroot that carries the freshly
# built libstdc++/libgcc instead of the device's ancient ones. It is used to
# run ARM binaries under qemu-user and mirrors the layout of the deployed
# application, which ships the same two libraries next to the binary.
# ---------------------------------------------------------------------------
ROOTFS="$PREFIX/rootfs"
if [ ! -d "$ROOTFS" ]; then
    echo "==> staging runtime rootfs at $ROOTFS"
    cp -al "$HARMATTAN_SYSROOT" "$ROOTFS"
    rm -f "$ROOTFS"/usr/lib/libstdc++.so.6* "$ROOTFS"/lib/libgcc_s.so.1
    libstdcxx="$(ls "$PREFIX/$TARGET/lib"/libstdc++.so.6.* | head -1)"
    cp -L "$libstdcxx" "$ROOTFS/usr/lib/"
    ln -sf "$(basename "$libstdcxx")" "$ROOTFS/usr/lib/libstdc++.so.6"
    cp -L "$PREFIX/$TARGET/lib/libgcc_s.so.1" "$ROOTFS/lib/libgcc_s.so.1"
fi

cat <<EOF

Toolchain ready.

  compiler:   $PREFIX/bin/$TARGET-g++
  sysroot:    $HARMATTAN_SYSROOT
  rootfs:     $ROOTFS

Configure a build with:

  cmake -B build-harmattan -G Ninja \\
    -DCMAKE_TOOLCHAIN_FILE=cmake/harmattan.toolchain.cmake
EOF
