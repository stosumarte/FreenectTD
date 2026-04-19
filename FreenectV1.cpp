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

// depthFormatEnum enum definition (shared between v1 and v2)
enum class depthFormatEnum {
    Raw,
    RawUndistorted,
    Registered
};

// MyFreenectDevice class constructor
MyFreenectDevice::MyFreenectDevice
    (freenect_context* ctx, int index,
     std::atomic<bool>& rgbFlag,
     std::atomic<bool>& depthFlag,
     std::atomic<bool>& irFlag) :
      FreenectDevice(ctx, index),
      rgbReady(rgbFlag),
      depthReady(depthFlag),
      irReady(irFlag),
      rgbBuffer(WIDTH * HEIGHT * 3),
      depthBuffer(WIDTH * HEIGHT),
      irBuffer(WIDTH * HEIGHT),
      hasNewRGB(false),
      hasNewDepth(false),
      hasNewIR(false)
{
    setVideoFormat(FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_MM);
}

// MyFreenectDevice class destructor
MyFreenectDevice::~MyFreenectDevice() {
    stop();
}

// VideoCallback dispatches into rgbBuffer or irBuffer depending on the currently
// active video source. libfreenect invokes this from the USB event thread.
void MyFreenectDevice::VideoCallback(void* video, uint32_t) {
    if (!video) return;
    std::lock_guard<std::mutex> lock(mutex);

    const fn1_videoSource src = currentVideoSource_.load(std::memory_order_relaxed);
    switch (src) {
        case fn1_videoSource::RGB: {
            auto* ptr = static_cast<uint8_t*>(video);
            std::copy(ptr, ptr + rgbBuffer.size(), rgbBuffer.begin());
            hasNewRGB = true;
            rgbReady = true;
            break;
        }
        case fn1_videoSource::IR_10BIT: {
            // IR_10BIT is unpacked by libfreenect: each pixel is a uint16_t in [0..1023].
            // The sensor frame is 640x488 but we only copy the first 480 rows for
            // consistency with RGB/depth outputs. The last 8 rows are unused.
            auto* ptr = static_cast<uint16_t*>(video);
            std::copy(ptr, ptr + irBuffer.size(), irBuffer.begin());
            hasNewIR = true;
            irReady = true;
            break;
        }
        case fn1_videoSource::IR_8BIT: {
            // Store 8-bit IR samples pre-scaled to the same 10-bit-equivalent
            // range used by IR_10BIT (0..1023). That lets the V1 IR Threshold
            // slider (0..1023) behave consistently in both modes and keeps the
            // downstream getIRFrame passthrough shift (<<6) correct for both.
            auto* ptr = static_cast<uint8_t*>(video);
            for (size_t i = 0; i < irBuffer.size(); ++i) {
                irBuffer[i] = static_cast<uint16_t>(ptr[i]) << 2;
            }
            hasNewIR = true;
            irReady = true;
            break;
        }
    }
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

// Called from any thread to request a source switch. The switch is deferred
// to the event thread because freenect_set_video_mode requires the video
// stream to be stopped, and that must not race with freenect_process_events.
void MyFreenectDevice::requestVideoSource(fn1_videoSource src) {
    pendingVideoSource_.store(src, std::memory_order_relaxed);
}

// Must be called from the libfreenect event thread, between process_events
// iterations. Sequence is carefully ordered to avoid a race between the USB
// callback and our source-format atomic: freenect_stop_video blocks until
// in-flight callbacks drain, so between stopVideo returning and startVideo
// being called no callback can fire — that is the only safe window to flip
// currentVideoSource_ and rebind the callback's interpretation of the
// incoming bytes. Holding the mutex during stopVideo would deadlock because
// libfreenect invokes the callback on the USB worker thread and stopVideo
// waits for that thread.
bool MyFreenectDevice::applyPendingVideoSource() {
    const fn1_videoSource pending = pendingVideoSource_.load(std::memory_order_relaxed);
    const fn1_videoSource current = currentVideoSource_.load(std::memory_order_relaxed);
    if (pending == current) return false;

    freenect_video_format fmt = FREENECT_VIDEO_RGB;
    switch (pending) {
        case fn1_videoSource::RGB:      fmt = FREENECT_VIDEO_RGB;      break;
        case fn1_videoSource::IR_10BIT: fmt = FREENECT_VIDEO_IR_10BIT; break;
        case fn1_videoSource::IR_8BIT:  fmt = FREENECT_VIDEO_IR_8BIT;  break;
    }

    // Stop first so no callback is in flight while we rewire the atomic.
    try {
        stopVideo();
    } catch (const std::exception&) {
        // If it was already stopped stopVideo throws; ignore and continue.
    }

    // Safe to update now — no callback can be running or about to run.
    {
        std::lock_guard<std::mutex> lock(mutex);
        hasNewRGB = false;
        hasNewIR = false;
        currentVideoSource_.store(pending, std::memory_order_relaxed);
    }

    try {
        // The wrapper's setVideoFormat will try an internal stopVideo first
        // (which returns <0 since we already stopped it) so wasRunning=false
        // and it won't auto-restart. That's what we want: we restart manually
        // after the atomic is in sync with the new format.
        setVideoFormat(fmt);
        startVideo();
    } catch (const std::exception& e) {
        LOG(std::string("[FreenectV1] applyPendingVideoSource failed: ") + e.what());
        // Revert so we don't retry every iteration. Caller should attempt to
        // recover by reverting the user-facing parameter.
        pendingVideoSource_.store(current, std::memory_order_relaxed);
        return false;
    }

    LOG(std::string("[FreenectV1] video source switched to ") + std::to_string(static_cast<int>(pending)));
    return true;
}

// Get RGB data
bool MyFreenectDevice::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    out = rgbBuffer;
    hasNewRGB = false;
    return true;
}

// Get depth data
bool MyFreenectDevice::getDepth(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    out = depthBuffer;
    hasNewDepth = false;
    return true;
}

// Get color frame
bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out, fn1_colorType type) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = rgbWidth_, dstHeight = rgbHeight_;

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

    if (type == depthFormatEnum::Registered) {
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_REGISTERED);
    } else {
        // Both Raw and RawUndistorted use FREENECT_DEPTH_MM for v1
        MyFreenectDevice::setDepthFormat(FREENECT_DEPTH_MM);
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
            const float min_mm = depthThreshMin;
            const float max_mm = depthThreshMax;
            if (val >= min_mm && val <= max_mm) {
                tmp[i] = static_cast<uint16_t>(
                    (static_cast<float>(val) - min_mm) / (max_mm - min_mm) * 65535.0f
                );
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
    return true;
}

// Get IR frame, normalized by threshold [min,max] into full 16-bit range.
// irBuffer always holds values in the 10-bit range [0..1023] regardless of
// source: IR_10BIT is stored as-is, IR_8BIT is pre-shifted by 2 in
// VideoCallback. The threshold slider operates in that same 10-bit range so
// its behavior is consistent across 8-bit and 10-bit modes.
bool MyFreenectDevice::getIRFrame(std::vector<uint16_t>& out, float irThreshMin, float irThreshMax) {
    const int srcWidth = WIDTH, srcHeight = HEIGHT;
    const int dstWidth = irWidth_, dstHeight = irHeight_;

    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewIR) return false;

    const size_t srcPixelCount = static_cast<size_t>(srcWidth) * srcHeight;
    const size_t dstPixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(dstPixelCount);

    const bool passthrough = (irThreshMax <= irThreshMin);
    std::vector<uint16_t> tmp(srcPixelCount);
    const float range = passthrough ? 1.0f : (irThreshMax - irThreshMin);
    #pragma omp parallel for if(srcPixelCount > 100000)
    for (size_t i = 0; i < srcPixelCount; ++i) {
        const uint16_t val = irBuffer[i];
        if (passthrough) {
            // Scale 10-bit samples to fill the 16-bit range so TD sees
            // something useful even without threshold tweaking.
            tmp[i] = static_cast<uint16_t>(val) << 6;
        } else if (val >= irThreshMin && val <= irThreshMax) {
            tmp[i] = static_cast<uint16_t>(
                (static_cast<float>(val) - irThreshMin) / range * 65535.0f
            );
        } else {
            tmp[i] = 0;
        }
    }

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

    hasNewIR = false;
    return true;
}
