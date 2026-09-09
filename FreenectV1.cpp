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
#include <limits>
#include <Accelerate/Accelerate.h>

namespace {

const char* depthFormatName(depthFormatEnum type) {
    switch (type) {
        case depthFormatEnum::Raw:
            return "Raw";
        case depthFormatEnum::RawUndistorted:
            return "RawUndistorted";
        case depthFormatEnum::Registered:
            return "Registered";
    }
    return "Unknown";
}

const char* freenectDepthFormatName(freenect_depth_format type) {
    switch (type) {
        case FREENECT_DEPTH_REGISTERED:
            return "FREENECT_DEPTH_REGISTERED";
        case FREENECT_DEPTH_MM:
            return "FREENECT_DEPTH_MM";
        case FREENECT_DEPTH_11BIT:
            return "FREENECT_DEPTH_11BIT";
        case FREENECT_DEPTH_10BIT:
            return "FREENECT_DEPTH_10BIT";
        case FREENECT_DEPTH_11BIT_PACKED:
            return "FREENECT_DEPTH_11BIT_PACKED";
        case FREENECT_DEPTH_10BIT_PACKED:
            return "FREENECT_DEPTH_10BIT_PACKED";
        default:
            return "UNKNOWN_FREENECT_DEPTH_FORMAT";
    }
}

} // namespace

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
bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = depthWidth_, dstHeight = depthHeight_;
    const freenect_depth_format requestedFormat = (type == depthFormatEnum::Registered)
        ? FREENECT_DEPTH_REGISTERED
        : FREENECT_DEPTH_MM;

    if (type != lastRequestedDepthFormat_ || requestedFormat != getDepthFormat()) {
        LOG("[FreenectV1] getDepthFrame: requestedType=" + std::string(depthFormatName(type)) +
            " requestedFormat=" + std::string(freenectDepthFormatName(requestedFormat)) +
            " currentFormat=" + std::string(freenectDepthFormatName(getDepthFormat())) +
            " thresholds=[" + std::to_string(depthThreshMin) + ", " + std::to_string(depthThreshMax) + "]");
    }

    if (type == depthFormatEnum::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    } else {
        // Both Raw and RawUndistorted use FREENECT_DEPTH_MM for v1
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_MM);
    }
    lastRequestedDepthFormat_ = type;

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    const size_t srcPixelCount = static_cast<size_t>(srcWidth) * srcHeight;
    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount);

    // Step 1. Normalize depth data into 16-bit linear buffer
    std::vector<uint16_t> tmp(srcPixelCount);
    size_t rawNonZero = 0;
    uint16_t rawMin = std::numeric_limits<uint16_t>::max();
    uint16_t rawMax = 0;
    size_t mappedNonZero = 0;
    uint16_t mappedMin = std::numeric_limits<uint16_t>::max();
    uint16_t mappedMax = 0;
    for (size_t i = 0; i < srcPixelCount; ++i) {
        uint16_t val = depthBuffer[i];
        if (val != 0) {
            rawNonZero++;
            rawMin = std::min(rawMin, val);
            rawMax = std::max(rawMax, val);
        }
        const float min_mm = depthThreshMin;
        const float max_mm = depthThreshMax;
        if (val >= min_mm && val <= max_mm) {
            const uint16_t mappedValue = static_cast<uint16_t>(
                (static_cast<float>(val) - min_mm) / (max_mm - min_mm) * 65535.0f
            );
            tmp[i] = mappedValue;
            if (mappedValue != 0) {
                mappedNonZero++;
                mappedMin = std::min(mappedMin, mappedValue);
                mappedMax = std::max(mappedMax, mappedValue);
            }
        } else {
            tmp[i] = 0;
        }
    }

    // Step 2. Use vImage to scale depth map (single channel 16-bit)
    vImage_Buffer srcBuf = {
        .data = tmp.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * sizeof(uint16_t))
    };

    vImage_Buffer dstBuf = {
        .data = out.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * sizeof(uint16_t))
    };

    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_Planar16U(&srcBuf, &dstBuf, nullptr, kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(out.data(), tmp.data(), tmp.size() * sizeof(uint16_t));
    }

    hasNewDepth = false;
    depthFrameCounter_++;

    if (rawNonZero == 0) {
        rawMin = 0;
    }
    if (mappedNonZero == 0) {
        mappedMin = 0;
    }

    if (depthFrameCounter_ == 1 || depthFrameCounter_ % 30 == 0 || (rawNonZero > 0 && mappedNonZero == 0)) {
        LOG("[FreenectV1] depth frame #" + std::to_string(depthFrameCounter_) +
            " type=" + std::string(depthFormatName(type)) +
            " rawNonZero=" + std::to_string(rawNonZero) + "/" + std::to_string(srcPixelCount) +
            " rawMin=" + std::to_string(rawMin) +
            " rawMax=" + std::to_string(rawMax) +
            " mappedNonZero=" + std::to_string(mappedNonZero) + "/" + std::to_string(srcPixelCount) +
            " mappedMin=" + std::to_string(mappedMin) +
            " mappedMax=" + std::to_string(mappedMax) +
            " outputSize=" + std::to_string(dstPixelCount));
    }

    return true;
}
