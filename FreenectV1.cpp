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
    
    const size_t pixelCount = WIDTH * HEIGHT;
    out.resize(pixelCount * 4);
    
    // Create temporary buffer for RGB to RGBA conversion
    std::vector<uint8_t> tempBuffer(pixelCount * 4);
    
    // Manual RGB to RGBA conversion (alpha set to 255)
    for (size_t i = 0, j = 0; i < pixelCount; ++i, j += 4) {
        tempBuffer[j + 0] = rgbBuffer[i * 3 + 0]; // R
        tempBuffer[j + 1] = rgbBuffer[i * 3 + 1]; // G
        tempBuffer[j + 2] = rgbBuffer[i * 3 + 2]; // B
        tempBuffer[j + 3] = 255;                 // A
    }
    
    // Create a new buffer for the vertical reflection
    std::vector<uint8_t> flippedBuffer(pixelCount * 4);

    vImage_Buffer srcRGBA = {
        .data = tempBuffer.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = WIDTH * 4
    };

    vImage_Buffer dstFlipped = {
        .data = flippedBuffer.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = WIDTH * 4
    };

    // Perform vertical flip (using 4 channels since we now have RGBA data)
    vImageVerticalReflect_ARGB8888(&srcRGBA, &dstFlipped, kvImageNoFlags);

    // Copy the flipped RGBA data to the output buffer
    std::memcpy(out.data(), flippedBuffer.data(), pixelCount * 4);
    
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
/*
// Get depth frame for FREENECT_DEPTH_11BIT format (depth already float)
bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out) {
    
    MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_11BIT); // Ensure depth format is 11-bit
    
    const int width = WIDTH, height = HEIGHT;

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    const size_t pixelCount = width * height;
    if (out.size() != pixelCount) out.resize(pixelCount);

    // Convert 11-bit depth values (already unpacked) to float
    std::vector<float> floatDepth(pixelCount);
    std::vector<float> inverted(pixelCount);
    
    for (size_t i = 0; i < pixelCount; ++i) {
        // Depth values are 0–2047 (11 bits)
        uint16_t raw = depthBuffer[i] & 0x07FF;
        floatDepth[i] = static_cast<float>(raw);
    }

    vImage_Buffer src = {
        .data = floatDepth.data(),
        .height = (vImagePixelCount)height,
        .width  = (vImagePixelCount)width,
        .rowBytes = WIDTH * sizeof(float)
    };

    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)height,
        .width  = (vImagePixelCount)width,
        .rowBytes = WIDTH * sizeof(float)
    };

    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    

    // Map back to 16-bit range
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float depth_val = flippedData[i];

        if (depth_val > 0.0f && depth_val <= 2047.0f) {
            float inv = 2047.0f - depth_val;
            //inv = 2047.0f - inv;
            out[i] = static_cast<uint16_t>(inv / 2047.0f * 65535.0f);
            //out[i] = static_cast<uint16_t>(2047.0f * 65535.0f);
        } else {
            out[i] = 0;
        }
    }

    hasNewDepth = false;
    return true;
    
}

// Get depth frame for FREENECT_DEPTH_REGISTERED format (depth in mm, aligned to RGB) - Using Accelerate
bool MyFreenectDevice::getDepthFrameRegistered(std::vector<uint16_t>& out) {
    
    MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED); // Ensure depth format is REGISTERED
    
    const int width = WIDTH, height = HEIGHT;
    
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    
    const size_t pixelCount = width * height;
    if (out.size() != pixelCount) out.resize(pixelCount);
    
    // Convert uint16_t depth buffer to float for vImage processing (similar to FreenectV2)
    std::vector<float> floatDepth(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i) {
        floatDepth[i] = static_cast<float>(depthBuffer[i]);
    }
    
    vImage_Buffer src = {
        .data = floatDepth.data(),
        .height = (vImagePixelCount)height,
        .width = (vImagePixelCount)width,
        .rowBytes = WIDTH * sizeof(float)
    };
    
    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)height,
        .width = (vImagePixelCount)width,
        .rowBytes = WIDTH * sizeof(float)
    };
    
    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    
    // Convert back to uint16_t with depth mapping
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float depth_mm = flippedData[i];

        const float min_mm = 400.0f;   // lower threshold (mm)
        const float max_mm = 4500.0f;  // upper threshold (mm)

        if (depth_mm >= min_mm && depth_mm <= max_mm) {
            out[i] = static_cast<uint16_t>(
                (depth_mm - min_mm) / (max_mm - min_mm) * 65535.0f
            );
        } else {
            out[i] = 0;
        }
    }
    
    hasNewDepth = false;
    return true;
}
*/

bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, fn1_depthType type) {
    // Select format
    if (type == fn1_depthType::Raw) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_11BIT);
    } else if (type == fn1_depthType::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    }

    const int width = WIDTH, height = HEIGHT;
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    const size_t pixelCount = width * height;
    if (out.size() != pixelCount) out.resize(pixelCount);

    // Convert depth buffer to float for vImage
    std::vector<float> floatDepth(pixelCount);
    if (type == fn1_depthType::Raw) {
        for (size_t i = 0; i < pixelCount; ++i) {
            uint16_t raw = depthBuffer[i] & 0x07FF; // 11 bits
            floatDepth[i] = static_cast<float>(raw);
        }
    } else {
        for (size_t i = 0; i < pixelCount; ++i) {
            floatDepth[i] = static_cast<float>(depthBuffer[i]);
        }
    }

    vImage_Buffer src = {
        .data = floatDepth.data(),
        .height = (vImagePixelCount)height,
        .width  = (vImagePixelCount)width,
        .rowBytes = width * sizeof(float)
    };

    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)height,
        .width  = (vImagePixelCount)width,
        .rowBytes = width * sizeof(float)
    };

    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);

    // Convert back to uint16_t with mapping
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float val = flippedData[i];

        if (type == fn1_depthType::Raw) {
            if (val > 0.0f && val <= 2047.0f) {
                float inv = 2047.0f - val;
                out[i] = static_cast<uint16_t>(inv / 2047.0f * 65535.0f);
            } else {
                out[i] = 0;
            }
        } else if (type == fn1_depthType::Registered) {
            const float min_mm = 400.0f;
            const float max_mm = 4500.0f;
            if (val >= min_mm && val <= max_mm) {
                out[i] = static_cast<uint16_t>(
                    (val - min_mm) / (max_mm - min_mm) * 65535.0f
                );
            } else {
                out[i] = 0;
            }
        }
    }

    hasNewDepth = false;
    return true;
}

