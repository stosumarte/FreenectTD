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
     std::atomic<bool>& depthFlag,
     bool ir) :
      FreenectDevice(ctx, index),
      rgbReady(rgbFlag),
      depthReady(depthFlag),
      rgbBuffer(WIDTH * HEIGHT * 3),
      depthBuffer(WIDTH * HEIGHT),
      irBuffer(WIDTH * HEIGHT),
      hasNewRGB(false),
      hasNewDepth(false),
      wantIR(ir),
      streamingIR(ir)
{
    // Open straight in the requested mode: switching right after the stream starts can leave it dead
    setVideoFormat(ir ? FREENECT_VIDEO_IR_10BIT : FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_MM);
}

// MyFreenectDevice class destructor
MyFreenectDevice::~MyFreenectDevice() {
    stop();
}

// VideoCallback method to handle RGB or IR data, depending on the current video mode
void MyFreenectDevice::VideoCallback(void* video, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!video) return;
    if (streamingIR) {
        // IR_10BIT frames are 640x488 with values in 0..1023; keep the first 480 rows to match RGB and depth
        auto ptr = static_cast<uint16_t*>(video);
        std::copy(ptr, ptr + irBuffer.size(), irBuffer.begin());
        hasNewIR = true;
        return;
    }
    auto ptr = static_cast<uint8_t*>(video);
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

// Start depth and video streams (using libfreenect.hpp API)
// Depth goes first: starting it resets the IR camera, which would kill an IR video stream.
bool MyFreenectDevice::start() {
    startDepth();
    startVideo();
    return true;
}

// Stop video and depth streams (using libfreenect.hpp API)
// Runs from the destructor, so it must not throw: stopping fails when a stream is already
// stopped, e.g. after the Kinect was unplugged.
void MyFreenectDevice::stop() {
    try { stopVideo(); } catch (const std::exception&) {}
    try { stopDepth(); } catch (const std::exception&) {}
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

// Request RGB or IR on the video stream
void MyFreenectDevice::setIR(bool ir) {
    wantIR = ir;
}

// Apply the requested depth format and RGB/IR video mode.
// Must run on the event thread between freenect_process_events calls: callbacks only fire inside
// process_events, so none can see a frame in the old format after the switch.
void MyFreenectDevice::applyStreamModes() {
    const freenect_depth_format depthFormat = wantDepthFormat.load();
    const bool depthRestarted = depthFormat != getDepthFormat();
    if (depthRestarted) {
        try {
            setDepthFormat(depthFormat);
        } catch (const std::exception& e) {
            LOG(std::string("[FreenectV1.cpp] applyStreamModes: ") + e.what());
        }
    }
    const bool ir = wantIR.load();
    // Restarting depth resets the IR camera, so an IR video stream has to be restarted after it
    if (ir == streamingIR && !(ir && depthRestarted)) return;
    {
        std::lock_guard<std::mutex> lock(mutex);
        streamingIR = ir;
        hasNewRGB = false;
        hasNewIR = false;
    }
    try {
        // setVideoFormat only restarts a stream it managed to stop, so stop and start it explicitly
        try { stopVideo(); } catch (const std::exception&) {} // already stopped
        setVideoFormat(ir ? FREENECT_VIDEO_IR_10BIT : FREENECT_VIDEO_RGB);
        startVideo();
    } catch (const std::exception& e) {
        LOG(std::string("[FreenectV1.cpp] applyStreamModes: ") + e.what());
    }
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

    // Both Raw and RawUndistorted use FREENECT_DEPTH_MM for v1
    wantDepthFormat = (type == depthFormatEnum::Registered) ? FREENECT_DEPTH_REGISTERED : FREENECT_DEPTH_MM;

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    if (dstWidth <= 0 || dstHeight <= 0) return false;

    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount);

    // Nearest-neighbour resample straight from the 16-bit mm buffer, then mask the depth range
    resampleNearest(depthBuffer.data(), srcWidth, srcHeight, 1, out.data(), dstWidth, dstHeight, /*flipX=*/false);
    for (float& depth : out) {
        if (!isDepthInRange(depth, depthThreshMin, depthThreshMax)) {
            depth = 0.0f;
        }
    }

    hasNewDepth = false;
    return true;
}

// Get IR frame
bool MyFreenectDevice::getIRFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewIR) return false;
    out.resize(irBuffer.size());
    std::transform(irBuffer.begin(), irBuffer.end(), out.begin(), [](uint16_t value) {
        return static_cast<uint16_t>(value << 6);
    });
    hasNewIR = false;
    return true;
}
