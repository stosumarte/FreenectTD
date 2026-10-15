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
    videoStartedAt = std::chrono::steady_clock::now(); // for callers that start the streams themselves
}

// MyFreenectDevice class destructor
MyFreenectDevice::~MyFreenectDevice() {
    stop();
}

// VideoCallback method to handle RGB or IR data, depending on the current video mode
void MyFreenectDevice::VideoCallback(void* video, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!video) return;
    videoFrameSinceStart = true;
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
    videoStartedAt = std::chrono::steady_clock::now();
    videoFrameSinceStart = false;
    return true;
}

// Stop video and depth streams (using libfreenect.hpp API)
// Runs from the destructor, so it must not throw: stopping fails when a stream is already
// stopped, e.g. after the Kinect was unplugged.
void MyFreenectDevice::stop() {
    try { stopVideo(); } catch (const std::exception&) {}
    try { stopDepth(); } catch (const std::exception&) {}
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
    // NOTE: the first IR -> RGB switch after opening in IR often starts a stream that never sends a
    // frame, with no error from libfreenect. Starting it again fixes it, so restart a silent video stream.
    const auto now = std::chrono::steady_clock::now();
    const bool videoStalled = !videoFrameSinceStart && now - videoStartedAt > std::chrono::seconds(1);
    // Restarting depth resets the IR camera, so an IR video stream has to be restarted after it
    if (ir == streamingIR && !(ir && depthRestarted) && !videoStalled) return;
    if (videoStalled) {
        LOG("[FreenectV1.cpp] applyStreamModes: no video frames for 1 s, restarting the video stream");
    }
    videoStartedAt = now;
    videoFrameSinceStart = false;
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

// Get color frame (RGBA, 640x480)
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out, fn1_colorType type) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    out.resize(static_cast<size_t>(WIDTH) * HEIGHT * 4);
    vImage_Buffer src = {
        .data = rgbBuffer.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = static_cast<size_t>(WIDTH * 3)
    };
    vImage_Buffer dst = {
        .data = out.data(),
        .height = (vImagePixelCount)HEIGHT,
        .width = (vImagePixelCount)WIDTH,
        .rowBytes = static_cast<size_t>(WIDTH * 4)
    };
    // RGB -> ARGB, then ARGB -> RGBA in place
    vImageConvert_RGB888toARGB8888(&src, nullptr, 255, &dst, false, kvImageNoFlags);
    vImagePermuteChannels_ARGB8888(&dst, &dst, (uint8_t[]){1, 2, 3, 0}, kvImageNoFlags);

    hasNewRGB = false;
    return true;
}

// Get depth frame
bool MyFreenectDevice::getDepthFrame(std::vector<float>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax) {
    // Both Raw and RawUndistorted use FREENECT_DEPTH_MM for v1
    wantDepthFormat = (type == depthFormatEnum::Registered) ? FREENECT_DEPTH_REGISTERED : FREENECT_DEPTH_MM;

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;

    // 16-bit mm to float, then mask the depth range
    out.assign(depthBuffer.begin(), depthBuffer.end());
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
