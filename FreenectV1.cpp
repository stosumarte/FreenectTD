//
//  FreenectV1.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV1.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <chrono>
#include <Accelerate/Accelerate.h>


// MyFreenectDevice class constructor
MyFreenectDevice::MyFreenectDevice
    (freenect_context* ctx, int index,
     std::atomic<bool>& rgbFlag,
     std::atomic<bool>& depthFlag) :
      FreenectDevice(ctx, index),
      rgbReady(rgbFlag),
      depthReady(depthFlag),
      rgbBuffer(WIDTH * HEIGHT * 3),
      depthBuffer(WIDTH * HEIGHT),
      hasNewRGB(false),
      hasNewDepth(false)
{
    setVideoFormat(FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_MM);
}

// MyFreenectDevice class destructor
MyFreenectDevice::~MyFreenectDevice() {
    stop();
}

// VideoCallback method to handle RGB data
void MyFreenectDevice::VideoCallback(void* rgb, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!rgb) return;
    auto ptr = static_cast<uint8_t*>(rgb);
    std::copy(ptr, ptr + rgbBuffer.size(), rgbBuffer.begin());
    hasNewRGB = true;
    rgbReady = true;
}

// DepthCallback method to handle depth data
void MyFreenectDevice::DepthCallback(void* depth, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!depth) return;
    auto ptr = static_cast<uint16_t*>(depth);
    std::copy(ptr, ptr + depthBuffer.size(), depthBuffer.begin());
    hasNewDepth = true;
    depthReady = true;
}

// Start video and depth streams (using libfreenect.hpp API)
bool MyFreenectDevice::start() {
    startVideo();
    startDepth();
    return true;
}

// Stop video and depth streams (using libfreenect.hpp API)
void MyFreenectDevice::stop() {
    stopVideo();
    stopDepth();
}

// Set RGB, depth and IR resolutions
void MyFreenectDevice::setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight) {
    rgbWidth_ = rgbWidth;
    rgbHeight_ = rgbHeight;
    depthWidth_ = depthWidth;
    depthHeight_ = depthHeight;
    irWidth_ = irWidth;
    irHeight_ = irHeight;
}

// Get RGB data
bool MyFreenectDevice::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);         // Lock the mutex to ensure thread safety
    if (!hasNewRGB) return false;                    // Check if new RGB data is available
    out = rgbBuffer;                                 // Copy the RGB buffer to the output vector
    hasNewRGB = false;                               // Reset the flag indicating new RGB data
    return true;
}

// Get depth data
bool MyFreenectDevice::getDepth(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);        // Lock the mutex to ensure thread safety
    if (!hasNewDepth) return false;                 // Check if new depth data is available
    out = depthBuffer;                              // Copy the depth buffer to the output vector
    hasNewDepth = false;                            // Reset the flag indicating new depth data
    return true;
}

// Get color frame
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out, fn1_colorType type) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = rgbWidth_, dstHeight = rgbHeight_;
    
    /*switch (type) {
        case fn1_colorType::RGB:
            MyFreenectDevice::setVideoFormat(FREENECT_VIDEO_RGB);
            break;
        case fn1_colorType::IR:
            MyFreenectDevice::setVideoFormat(FREENECT_VIDEO_IR_10BIT);
            break;
        default:
            MyFreenectDevice::setVideoFormat(FREENECT_VIDEO_RGB);
            break;
    }*/

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount * 4);

    // Source buffer (RGB888)
    vImage_Buffer src = {
        .data = rgbBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 3)
    };

    // Temporary ARGB buffer (same size as source)
    std::vector<uint8_t> tmpARGB(srcWidth * srcHeight * 4);
    vImage_Buffer tmpARGBbuf = {
        .data = tmpARGB.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4)
    };

    // Destination RGBA buffer (scaled)
    vImage_Buffer dst = {
        .data = out.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * 4)
    };

    // Convert RGB → ARGB
    vImageConvert_RGB888toARGB8888(&src, nullptr, 255, &tmpARGBbuf, false, kvImageNoFlags);

    // Scale ARGB
    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_ARGB8888(&tmpARGBbuf, &dst, nullptr, kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        // Same size, copy directly
        std::memcpy(out.data(), tmpARGB.data(), tmpARGB.size());
    }

    // Convert ARGB → RGBA in place (safe, same 4 bytes per pixel)
    vImagePermuteChannels_ARGB8888(&dst, &dst, (uint8_t[]){1, 2, 3, 0}, kvImageNoFlags);

    hasNewRGB = false;
    return true;
}

// Get depth frame
bool MyFreenectDevice::getDepthFrame(std::vector<float>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = depthWidth_, dstHeight = depthHeight_;

    if (type == depthFormatEnum::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    } else {
        // Both Raw and RawUndistorted use FREENECT_DEPTH_MM for v1
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_MM);
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    if (dstWidth <= 0 || dstHeight <= 0) return false;

    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount);

    // Nearest-neighbour resample straight from the 16-bit mm buffer; depth must not be interpolated.
    for (int y = 0; y < dstHeight; ++y) {
        const int sy = (dstHeight == srcHeight) ? y : std::min(srcHeight - 1, static_cast<int>((y + 0.5f) * srcHeight / dstHeight));
        const uint16_t* srcRow = depthBuffer.data() + static_cast<size_t>(sy) * srcWidth;
        float* dstRow = out.data() + static_cast<size_t>(y) * dstWidth;
        for (int x = 0; x < dstWidth; ++x) {
            const int sx = (dstWidth == srcWidth) ? x : std::min(srcWidth - 1, static_cast<int>((x + 0.5f) * srcWidth / dstWidth));
            const float val = static_cast<float>(srcRow[sx]);
            dstRow[x] = (val >= depthThreshMin && val <= depthThreshMax) ? val : 0.0f;
        }
    }

    hasNewDepth = false;
    return true;
}
