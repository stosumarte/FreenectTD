//
//  FreenectV1.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV1.h"
#include <algorithm>
#include <cstring>

// MyFreenectDevice class constructor
MyFreenectDevice::MyFreenectDevice(freenect_context* ctx, int index,
                                   std::atomic<bool>& rgbFlag,
                                   std::atomic<bool>& depthFlag)
    : FreenectDevice(ctx, index),
      rgbReady(rgbFlag),
      depthReady(depthFlag),
      rgbBuffer(WIDTH * HEIGHT * 3),
      depthBuffer(WIDTH * HEIGHT),
      hasNewRGB(false),
      hasNewDepth(false) {
    setVideoFormat(FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_11BIT);
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
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out, bool flip) {
    std::lock_guard<std::mutex> lock(mutex);        // Lock the mutex to ensure thread safety
    if (!hasNewRGB) return false;                   // Check if new RGB data is available
//  int width = WIDTH, height = HEIGHT;             // Get the dimensions of the RGB frame
    out.resize(WIDTH * HEIGHT * 4);
    for (int y = 0; y < HEIGHT; ++y) {
        int srcY = flip ? (HEIGHT - 1 - y) : y;
        for (int x = 0; x < WIDTH; ++x) {
            int srcIdx = (srcY * WIDTH + x) * 3;
            int dstIdx = (y * WIDTH + x) * 4;
            out[dstIdx + 0] = rgbBuffer[srcIdx + 0];
            out[dstIdx + 1] = rgbBuffer[srcIdx + 1];
            out[dstIdx + 2] = rgbBuffer[srcIdx + 2];
            out[dstIdx + 3] = 255;
        }
    }
    hasNewRGB = false;                              // Reset the flag indicating new RGB data
    return true;
}

// Get depth frame and apply scaling and flipping
bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, bool invert, bool flip) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    int width = WIDTH, height = HEIGHT;
    out.resize(width * height);
    for (int y = 0; y < height; ++y) {
        int srcY = flip ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            int idx = srcY * width + x;
            uint16_t raw = depthBuffer[idx];
            if (raw > 0 && raw < 2047) {
                uint16_t scaled = raw << 5;
                out[y * width + x] = invert ? 65535 - scaled : scaled;
            } else {
                out[y * width + x] = 0;
            }
        }
    }
    hasNewDepth = false;
    return true;
}
