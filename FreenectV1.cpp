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
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out) {
    const int srcWidth = rgbWidth_, srcHeight = rgbHeight_;
    const int dstWidth = srcWidth, dstHeight = srcHeight;
    
    std::lock_guard<std::mutex> lock(mutex);        // Lock the mutex to ensure thread safety
    if (!hasNewRGB) return false;                   // Check if new RGB data is available
    
    const size_t pixelCount = srcWidth * srcHeight;
    out.resize(pixelCount * 4);
    
    // Convert RGB to RGBA using Accelerate
    std::vector<uint8_t> rgbaBuffer(pixelCount * 4);
    vImage_Buffer vImage_RGB = {
        .data = rgbBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 3)
    };
    vImage_Buffer vImage_RGBA = {
        .data = rgbaBuffer.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * 4)
    };
    vImage_Error vImageErr = vImageConvert_RGB888toRGBA8888(
        &vImage_RGB,    // RGB source
        NULL,           // no alpha plane, use constant instead
        255,            // constant alpha
        &vImage_RGBA,   // RGBA destination
        false,          // premultiply = false
        kvImageNoFlags  // flags
    );
    if (vImageErr != kvImageNoError) {
        printf("vImage error: %ld\n", vImageErr);
    }


    // Copy the flipped RGBA data to the output buffer
    std::memcpy(out.data(), rgbaBuffer.data(), pixelCount * 4);
    
    hasNewRGB = false;
    
    return true;
}

bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, fn1_depthType type) {
    const int srcWidth = depthWidth_, srcHeight = depthHeight_;

    // Select format
    if (type == fn1_depthType::Raw) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_11BIT);
    } else if (type == fn1_depthType::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    const size_t pixelCount = srcWidth * srcHeight;
    if (out.size() != pixelCount) out.resize(pixelCount);

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        uint16_t val = depthBuffer[i];

        if (type == fn1_depthType::Raw) {
            val &= 0x07FF; // keep only 11 bits
            if (val > 0 && val <= 2047) {
                float inv = 2047.0f - static_cast<float>(val);
                out[i] = static_cast<uint16_t>(inv / 2047.0f * 65535.0f);
            } else {
                out[i] = 0;
            }
        } else if (type == fn1_depthType::Registered) {
            const float min_mm = 400.0f;
            const float max_mm = 4500.0f;
            if (val >= min_mm && val <= max_mm) {
                out[i] = static_cast<uint16_t>(
                    (static_cast<float>(val) - min_mm) / (max_mm - min_mm) * 65535.0f
                );
            } else {
                out[i] = 0;
            }
        }
    }

    hasNewDepth = false;

    return true;
}


