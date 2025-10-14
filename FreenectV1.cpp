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
    setDepthFormat(FREENECT_DEPTH_REGISTERED);  // Changed from FREENECT_DEPTH_11BIT to FREENECT_DEPTH_REGISTERED
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
bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, fn1_depthType type) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = depthWidth_, dstHeight = depthHeight_;

    if (type == fn1_depthType::Raw) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_11BIT);
    } else if (type == fn1_depthType::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    const size_t srcPixelCount = static_cast<size_t>(srcWidth) * srcHeight;
    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount);

    // Step 1. Normalize depth data into 16-bit linear buffer
    std::vector<uint16_t> tmp(srcPixelCount);
    #pragma omp parallel for if(srcPixelCount > 100000)
    for (size_t i = 0; i < srcPixelCount; ++i) {
        uint16_t val = depthBuffer[i];
        if (type == fn1_depthType::Raw) {
            val &= 0x07FF;
            if (val > 0 && val <= 2047) {
                float inv = 2047.0f - static_cast<float>(val);
                tmp[i] = static_cast<uint16_t>(inv / 2047.0f * 65535.0f);
            } else {
                tmp[i] = 0;
            }
        } else if (type == fn1_depthType::Registered) {
            const float min_mm = 400.0f;
            const float max_mm = 4500.0f;
            if (val >= min_mm && val <= max_mm) {
                tmp[i] = static_cast<uint16_t>(
                    (static_cast<float>(val) - min_mm) / (max_mm - min_mm) * 65535.0f
                );
            } else {
                tmp[i] = 0;
            }
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
        vImageScale_Planar16U(&srcBuf, &dstBuf, nullptr,
                              kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(out.data(), tmp.data(), tmp.size() * sizeof(uint16_t));
    }

    hasNewDepth = false;
    return true;
}
