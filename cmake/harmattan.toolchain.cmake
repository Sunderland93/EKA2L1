# CMake toolchain for MeeGo Harmattan 1.2 (Nokia N9).
#
# The device runs a 2011-era Maemo userland: ARMv7-A Cortex-A8 with hard-float
# VFP, a PowerVR SGX GPU that only speaks OpenGL ES 2.0, and Maemo eglibc 2.10.
# The Qt SDK that ships with Harmattan (Madde) only provides GCC 4.4.1, which
# cannot compile the C++17/20 code this project uses. A modern cross compiler
# is therefore built against the Harmattan sysroot by
# scripts/build_harmattan_toolchain.sh.
#
# Usage:
#   cmake -B build-harmattan -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/harmattan.toolchain.cmake
#
# The following cache variables may be overridden on the command line:
#   HARMATTAN_SYSROOT   - the Harmattan sysroot to build against
#   HARMATTAN_TOOLCHAIN - prefix of the cross toolchain to use

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT HARMATTAN_SYSROOT)
    set(HARMATTAN_SYSROOT "$ENV{HARMATTAN_SYSROOT}")
endif()
if(NOT HARMATTAN_SYSROOT AND NOT "$ENV{QTSDK_DIR}" STREQUAL "")
    set(HARMATTAN_SYSROOT "$ENV{QTSDK_DIR}/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim")
endif()
if(NOT HARMATTAN_SYSROOT)
    set(HARMATTAN_SYSROOT "$ENV{HOME}/Dev/QtSDK/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim")
endif()

if(NOT HARMATTAN_TOOLCHAIN)
    set(HARMATTAN_TOOLCHAIN "$ENV{HARMATTAN_TOOLCHAIN}")
endif()
if(NOT HARMATTAN_TOOLCHAIN)
    set(HARMATTAN_TOOLCHAIN "$ENV{HOME}/Dev/eka2l1-harmattan")
endif()

if(NOT EXISTS "${HARMATTAN_SYSROOT}")
    message(FATAL_ERROR
        "Harmattan sysroot not found at '${HARMATTAN_SYSROOT}'.\n"
        "Set -DHARMATTAN_SYSROOT=<path> or the HARMATTAN_SYSROOT environment variable.")
endif()
if(NOT EXISTS "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++")
    message(FATAL_ERROR
        "Cross compiler not found at '${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++'.\n"
        "Run scripts/build_harmattan_toolchain.sh first, or set -DHARMATTAN_TOOLCHAIN=<path>.")
endif()

set(CMAKE_C_COMPILER "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++")
set(CMAKE_ASM_COMPILER "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_AR "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-ar")
set(CMAKE_RANLIB "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-ranlib")
set(CMAKE_STRIP "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-strip")
set(CMAKE_OBJDUMP "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-objdump")
set(CMAKE_OBJCOPY "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-objcopy")
set(CMAKE_READELF "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-readelf")
set(CMAKE_NM "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-nm")
set(CMAKE_ADDR2LINE "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-addr2line")
set(CMAKE_LINKER "${HARMATTAN_TOOLCHAIN}/bin/arm-linux-gnueabihf-ld")
set(CMAKE_FIND_ROOT_PATH "${HARMATTAN_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_SYSTEM_VERSION "2.6.32")
set(CMAKE_SYSTEM_VERSION_STRING "Harmattan (Linux 2.6.32, Maemo eglibc 2.10)")

# The device's dynamic loader is the pre-2011 name; modern toolchains default
# to /lib/ld-linux-armhf.so.3 which does not exist on the device.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--dynamic-linker=/lib/ld-linux.so.3")

# The Harmattan sysroot carries no compiler-provided platform marker, so the
# port defines its own. src/emu/common/include/common/platform.h turns this
# into EKA2L1_PLATFORM_MEEGO.
#
# Qt 4.7.4 on the device was compiled with GCC 4.4, so it uses the pre-C++11
# copy-on-write std::string ABI. Everything that talks to Qt has to agree on
# that ABI, and the C++ standard library shipped with the toolchain provides
# both, so the old one is selected here.
#
# _GLIBCXX_USE_C99_MATH_FUNCS and the force-included header are needed because
# eglibc 2.10 does not declare the C99 long double math functions on ARM; see
# cmake/harmattan_math_compat.h.
set(CMAKE_C_FLAGS_INIT "-D__HARMATTAN__")
set(CMAKE_CXX_FLAGS_INIT "-D__HARMATTAN__ -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_C99_MATH_FUNCS=1 -include \"${CMAKE_CURRENT_LIST_DIR}/harmattan_math_compat.h\"")

# clock_gettime lives in librt on eglibc 2.10.
set(CMAKE_THREAD_LIBS_INIT "-lpthread -lrt")

# Tells the build system that this is the Harmattan target. Kept in the cache
# so that it survives the toolchain file being re-read in try_compile scopes.
set(EKA2L1_MEEGO ON CACHE INTERNAL "Building for MeeGo Harmattan")

set(HARMATTAN_SYSROOT "${HARMATTAN_SYSROOT}" CACHE PATH "Harmattan sysroot used for this build")
set(HARMATTAN_TOOLCHAIN "${HARMATTAN_TOOLCHAIN}" CACHE PATH "Cross toolchain prefix used for this build")
