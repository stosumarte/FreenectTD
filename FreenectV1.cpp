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

// Get color frame and flip it
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out) {
    static int frameCount = 0;
    static auto lastTime = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(mutex);        // Lock the mutex to ensure thread safety
    if (!hasNewRGB) return false;                   // Check if new RGB data is available
    
    // Log FPS before processing
    frameCount++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
    if (elapsed >= 1) {
        std::cout << "[FreenectV1] FPS BEFORE image manipulation: " << frameCount << std::endl;
        frameCount = 0;
        lastTime = now;
    }
    
    out.resize(WIDTH * HEIGHT * 4);
    for (int y = 0; y < HEIGHT; ++y) {
        int srcY = HEIGHT - 1 - y; // Always flip vertically
        for (int x = 0; x < WIDTH; ++x) {
            int srcIdx = (srcY * WIDTH + x) * 3;
            int dstIdx = (y * WIDTH + x) * 4;
            out[dstIdx + 0] = rgbBuffer[srcIdx + 0];
            out[dstIdx + 1] = rgbBuffer[srcIdx + 1];
            out[dstIdx + 2] = rgbBuffer[srcIdx + 2];
            out[dstIdx + 3] = 255;
        }
    }
    
    // Log FPS after processing
    static int afterFrameCount = 0;
    static auto afterLastTime = std::chrono::steady_clock::now();
    afterFrameCount++;
    auto afterNow = std::chrono::steady_clock::now();
    auto afterElapsed = std::chrono::duration_cast<std::chrono::seconds>(afterNow - afterLastTime).count();
    if (afterElapsed >= 1) {
        std::cout << "[FreenectV1] FPS AFTER image manipulation: " << afterFrameCount << std::endl;
        afterFrameCount = 0;
        afterLastTime = afterNow;
    }
    
    hasNewRGB = false;                              // Reset the flag indicating new RGB data
    return true;
}

// Get depth frame for FREENECT_DEPTH_REGISTERED format (depth in mm, aligned to RGB) - Using Accelerate
bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    
    const size_t pixelCount = WIDTH * HEIGHT;
    if (out.size() != pixelCount) out.resize(pixelCount);
    
    // Convert uint16_t depth buffer to float for vImage processing (similar to FreenectV2)
    std::vector<float> floatDepth(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i) {
        floatDepth[i] = static_cast<float>(depthBuffer[i]);
    }
    
    vImage_Buffer src = {
        .data = floatDepth.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = WIDTH * sizeof(float)
    };
    
    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = WIDTH * sizeof(float)
    };
    
    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    vImageHorizontalReflect_PlanarF(&dst, &dst, kvImageNoFlags);
    
    // Convert back to uint16_t with depth mapping
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float depth_mm = flippedData[i];
        
        // FREENECT_DEPTH_REGISTERED provides depth in mm (0-10000mm range)
        // Map to 0-65535 range like FreenectV2, but use full FREENECT_DEPTH_MM_MAX_VALUE range
        if (depth_mm > 100.0f && depth_mm <= static_cast<float>(FREENECT_DEPTH_MM_MAX_VALUE)) {
            // Map from mm range to 16-bit range
            out[i] = static_cast<uint16_t>(depth_mm / static_cast<float>(FREENECT_DEPTH_MM_MAX_VALUE) * 65535.0f);
        } else {
            // No valid depth data
            out[i] = 0;
        }
    }
    
    hasNewDepth = false;
    return true;
}
