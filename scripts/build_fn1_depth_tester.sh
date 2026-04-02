#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT="${BUILD_DIR}/fn1_depth_tester"
SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"

mkdir -p "${BUILD_DIR}"

xcrun clang++ \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -arch arm64 \
  -mmacosx-version-min=12.4 \
  -isysroot "${SDK_PATH}" \
  -DFNTD_DEBUG=1 \
  -DFNTD_PROFILE=0 \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/include/headers" \
  "${ROOT_DIR}/tools/fn1_depth_tester.cpp" \
  "${ROOT_DIR}/FreenectV1.cpp" \
  "${ROOT_DIR}/ofxKinectExtras/ofxKinectExtras.cpp" \
  "${ROOT_DIR}/include/libs/libfreenect_0.7.5.a" \
  "${ROOT_DIR}/include/libs/libusb_1.0.29.a" \
  -framework Accelerate \
  -framework IOKit \
  -framework CoreFoundation \
  -framework Security \
  -lpthread \
  -o "${OUTPUT}"

echo "Built ${OUTPUT}"
