#!/bin/bash
#
# Builds the libraries FreenectTD links statically from the submodules in external/, then copies them
# and their headers into include/:
#   libusb        USB access for both libraries below
#   libfreenect   Kinect v1
#   libfreenect2  Kinect v2, our fork (stosumarte/libfreenect2, branch metal) with the Metal depth pipeline
# All arm64, macOS 12.0+. Needs full Xcode (for the Metal compiler) and CMake.
#
# Usage: git submodule update --init && utils/build_deps.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXT="$ROOT/external"
BUILD="$ROOT/build/deps"
STAGE="$BUILD/stage" # libusb as the other two find it
MACOS=12.0
JOBS="$(sysctl -n hw.ncpu)"
LIBUSB_VERSION="$(git -C "$EXT/libusb" describe --tags --exact-match | sed 's/^v//')"
LIBFREENECT_VERSION="$(git -C "$EXT/libfreenect" describe --tags --exact-match | sed 's/^v//')"

rm -rf "$BUILD"
mkdir -p "$STAGE/lib" "$STAGE/include/libusb-1.0" "$BUILD/pkgconfig"

# libusb, with its own Xcode project
xcodebuild -quiet -project "$EXT/libusb/Xcode/libusb.xcodeproj" -target libusb -configuration Release \
    MACH_O_TYPE=staticlib EXECUTABLE_EXTENSION=a LLVM_LTO=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=NO MACOSX_DEPLOYMENT_TARGET="$MACOS" \
    SYMROOT="$BUILD/libusb" OBJROOT="$BUILD/libusb/obj"
cp "$BUILD"/libusb/Release/*.a "$STAGE/lib/libusb-1.0.a"
cp "$EXT/libusb/libusb/libusb.h" "$STAGE/include/libusb-1.0/"
# libfreenect2 finds libusb through pkg-config
cat > "$BUILD/pkgconfig/libusb-1.0.pc" <<EOF
Name: libusb-1.0
Description: libusb built by build_deps.sh
Version: $LIBUSB_VERSION
Libs: -L$STAGE/lib -lusb-1.0
Cflags: -I$STAGE/include/libusb-1.0
EOF

COMMON=(
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_BUILD_TYPE=RelWithDebInfo # keeps source lines in crash reports
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS"
    -DBUILD_EXAMPLES=OFF
)

# libfreenect
cmake -S "$EXT/libfreenect" -B "$BUILD/libfreenect" "${COMMON[@]}" \
    -DLIBUSB_1_INCLUDE_DIRS="$STAGE/include/libusb-1.0" \
    -DLIBUSB_1_LIBRARIES="$STAGE/lib/libusb-1.0.a" \
    -DBUILD_FAKENECT=OFF -DBUILD_C_SYNC=OFF -DBUILD_CPP=OFF -DBUILD_REDIST_PACKAGE=ON
cmake --build "$BUILD/libfreenect" --target freenectstatic -j "$JOBS"

# libfreenect2: Metal and CPU depth, VideoToolbox color (so no TurboJPEG), TinyThread threading
# (FreenectV2.cpp relies on its waitForNewFrame behaviour)
PKG_CONFIG_LIBDIR="$BUILD/pkgconfig" cmake -S "$EXT/libfreenect2" -B "$BUILD/libfreenect2" "${COMMON[@]}" \
    -DENABLE_METAL=ON -DENABLE_OPENCL=OFF -DENABLE_OPENGL=OFF -DENABLE_CUDA=OFF -DENABLE_VAAPI=OFF \
    -DENABLE_TEGRAJPEG=OFF -DENABLE_CXX11=OFF -DBUILD_OPENNI2_DRIVER=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_TurboJPEG=ON \
    -DCMAKE_INSTALL_PREFIX="$BUILD/libfreenect2/install"
cmake --build "$BUILD/libfreenect2" --target install -j "$JOBS"
# CMake quietly builds without Metal when the Metal compiler can't run
if ! grep -q "^#define LIBFREENECT2_WITH_METAL_SUPPORT" "$BUILD/libfreenect2/install/include/libfreenect2/config.h"; then
    echo "libfreenect2 built without Metal: install full Xcode (on Xcode 26+ also: xcodebuild -downloadComponent MetalToolchain)" >&2
    exit 1
fi

# Into include/
LIBS="$ROOT/include/libs"
HEADERS="$ROOT/include/headers"
rm -f "$LIBS"/libusb_*.a "$LIBS"/libfreenect_*.a
cp "$STAGE/lib/libusb-1.0.a" "$LIBS/libusb_$LIBUSB_VERSION.a"
cp "$STAGE/include/libusb-1.0/libusb.h" "$HEADERS/libusb.h"
cp "$BUILD/libfreenect/lib/libfreenect.a" "$LIBS/libfreenect_$LIBFREENECT_VERSION.a"
# The libfreenect headers in include/headers/libfreenect stay as they are: 0.7.5's, with a doc comment typo
# fixed (it warns with -Wdocumentation) and libusb.h included with quotes
# libfreenect2 still calls itself 0.2.0
cp "$BUILD/libfreenect2/install/lib/libfreenect2.a" "$LIBS/libfreenect2_0.2.0.a"
rm -rf "$HEADERS/libfreenect2"
cp -R "$BUILD/libfreenect2/install/include/libfreenect2" "$HEADERS/libfreenect2"

echo "Built libusb $LIBUSB_VERSION, libfreenect $LIBFREENECT_VERSION, libfreenect2 $(git -C "$EXT/libfreenect2" rev-parse --short HEAD) into include/"
