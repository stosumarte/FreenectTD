//
//  FreenectV2.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV2.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <thread>
#include <Accelerate/Accelerate.h>

// MyFreenect2Device class constructor
MyFreenect2Device::MyFreenect2Device(
    libfreenect2::Freenect2Device* dev,
    std::atomic<bool>& rgbFlag,
    std::atomic<bool>& depthFlag,
    std::atomic<bool>& irFlag
 ):
    device(dev),
    listener(nullptr),
    reg(nullptr),
    rgbReady(rgbFlag),
    depthReady(depthFlag),
    irReady(irFlag),
    rgbBuffer(RGB_WIDTH * RGB_HEIGHT * 4, 0),
    depthBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0),
    hasNewRGB(false), hasNewDepth(false) {
        listener = new libfreenect2::SyncMultiFrameListener
            (libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
        device->setColorFrameListener(listener);
        device->setIrAndDepthFrameListener(listener);
    }

// MyFreenect2Device class destructor
MyFreenect2Device::~MyFreenect2Device() {
    LOG("[FreenectV2.cpp] MyFreenect2Device destructor called");
    stop();
    LOG("[FreenectV2.cpp] MyFreenect2Device stop called");
    if (listener) {
        LOG("[FreenectV2.cpp] Deleting listener");
        delete listener;
        LOG("[FreenectV2.cpp] listener deleted");
        listener = nullptr;
        LOG("[FreenectV2.cpp] listener set to nullptr");
    }
}

// Start the device streams using libfreenect2 API
bool MyFreenect2Device::start() {
    if (!device) return false;
    return device->startStreams(true, true);
}

// Stop the device streams using libfreenect2 API
void MyFreenect2Device::stop() {
    if (device) {
        device->stop();
        LOG("[FreenectV2.cpp] device->stop");
    }
}

// Close the device using libfreenect2 API
void MyFreenect2Device::close() {
    if (device) {
        device->close();
        LOG("[FreenectV2.cpp] device->close");
    }
}

// Set RGB, depth and IR resolutions
void MyFreenect2Device::setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight, int bigdepthWidth, int bigdepthHeight) {
    rgbWidth_ = rgbWidth;
    rgbHeight_ = rgbHeight;
    depthWidth_ = depthWidth;
    depthHeight_ = depthHeight;
    irWidth_ = irWidth;
    irHeight_ = irHeight;
    bigdepthWidth_ = bigdepthWidth;
    bigdepthHeight_ = bigdepthHeight;
}

// Process incoming frames
void MyFreenect2Device::processFrames() {
    if (!listener) return;
    if (!listener->hasNewFrame()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Prevent busy-waiting
        return;
    }
    libfreenect2::FrameMap frames;
    listener->waitForNewFrame(frames, 1000); // Immediately get the frame if available
    libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
    libfreenect2::Frame* ir = frames[libfreenect2::Frame::Ir];
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (rgb && rgb->data && rgb->width == RGB_WIDTH && rgb->height == RGB_HEIGHT) {
            memcpy(rgbBuffer.data(), rgb->data, RGB_WIDTH * RGB_HEIGHT * 4);
            hasNewRGB = true;
            rgbReady = true;
        }
        if (depth && depth->data && depth->width == DEPTH_WIDTH && depth->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(depth->data);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, depthBuffer.begin());
            hasNewDepth = true;
            depthReady = true;
        }
        if (ir && ir->data && ir->width == DEPTH_WIDTH && ir->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(ir->data);
            if (irBuffer.size() != DEPTH_WIDTH * DEPTH_HEIGHT)
                irBuffer.resize(DEPTH_WIDTH * DEPTH_HEIGHT);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, irBuffer.begin());
            hasNewIR = true;
            irReady = true;
        }

    }
    listener->release(frames);
}

// Get RGB data
bool MyFreenect2Device::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    out = rgbBuffer;
    hasNewRGB = false;
    return true;
}

// Get depth data
bool MyFreenect2Device::getDepth(std::vector<float>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) {
        return false;
    }
    else {
        out = depthBuffer;
        hasNewDepth = false;
        return true;
    }
}

// Get IR data
bool MyFreenect2Device::getIR(std::vector<float>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewIR) {
        return false;
    }
    else {
        out = irBuffer;
        hasNewIR = false;
        return true;
    }
}

// Get color frame with flipping and optional downscaling
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    const int srcWidth  = RGB_WIDTH;
    const int srcHeight = RGB_HEIGHT;
    const int dstWidth  = rgbWidth_;
    const int dstHeight = rgbHeight_;
    const size_t dstSize = static_cast<size_t>(dstWidth) * dstHeight * 4;
    out.resize(dstSize);

    // Source image (BGRA8888)
    vImage_Buffer src = {
        .data = rgbBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width  = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4)
    };

    // Intermediate buffer for horizontal reflection
    std::vector<uint8_t> tmpFlip(srcWidth * srcHeight * 4);
    vImage_Buffer tmpFlipBuf = {
        .data = tmpFlip.data(),
        .height = (vImagePixelCount)srcHeight,
        .width  = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4)
    };

    // Perform horizontal flip (src → tmpFlip)
    vImageHorizontalReflect_ARGB8888(&src, &tmpFlipBuf, kvImageDoNotTile);

    // Destination buffer
    vImage_Buffer dst = {
        .data = out.data(),
        .height = (vImagePixelCount)dstHeight,
        .width  = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * 4)
    };

    // Scale if necessary (tmpFlip → dst)
    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_ARGB8888(&tmpFlipBuf, &dst, nullptr,
                             kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(out.data(), tmpFlip.data(), tmpFlip.size());
    }

    // BGRA → RGBA (safe in-place permutation)
    const uint8_t permuteMap[4] = {2, 1, 0, 3};
    vImagePermuteChannels_ARGB8888(&dst, &dst, permuteMap, kvImageNoFlags);

    hasNewRGB = false;
    return true;
}

// All-in-one depth frame retrieval (raw/undistorted/registered)
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out, fn2_depthType type) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth || !device) return false;

    // Lazy init of registration object
    if (!reg) {
        const auto& irParams = device->getIrCameraParams();
        const auto& colorParams = device->getColorCameraParams();
        reg = std::make_unique<libfreenect2::Registration>(irParams, colorParams);
        if (!reg) {
            LOG("Failed to create Registration object");
            return false;
        }
    }

    libfreenect2::Frame depthFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));
    libfreenect2::Frame rgbFrame(RGB_WIDTH, RGB_HEIGHT, 4);
    libfreenect2::Frame undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));
    libfreenect2::Frame registeredFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4);
    libfreenect2::Frame bigdepthFrame(BIGDEPTH_WIDTH, BIGDEPTH_HEIGHT, sizeof(float));

    const float* srcData = nullptr;
    int srcWidth = 0, srcHeight = 0;
    int dstWidth = 0, dstHeight = 0;

    switch (type) {
        case fn2_depthType::Raw: {
            srcData = depthBuffer.data();
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            dstWidth = depthWidth_;
            dstHeight = depthHeight_;
            break;
        }
        case fn2_depthType::Undistorted: {
            std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->undistortDepth(&depthFrame, &undistortedFrame);
            srcData = reinterpret_cast<float*>(undistortedFrame.data);
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            dstWidth = depthWidth_;
            dstHeight = depthHeight_;
            break;
        }
        case fn2_depthType::Registered: {
            std::memcpy(rgbFrame.data, rgbBuffer.data(), RGB_WIDTH * RGB_HEIGHT * 4);
            std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->apply(&rgbFrame, &depthFrame, &undistortedFrame, &registeredFrame, true, &bigdepthFrame);

            const float* bigDepthData = reinterpret_cast<float*>(bigdepthFrame.data);
            const int bigW = BIGDEPTH_WIDTH;
            const int bigH = BIGDEPTH_HEIGHT;
            const int croppedH = bigH - 2;

            std::vector<float> cropped(bigW * croppedH);
            for (int y = 0; y < croppedH; ++y) {
                const float* srcRow = bigDepthData + (y + 1) * bigW;
                float* dstRow = cropped.data() + y * bigW;
                std::memcpy(dstRow, srcRow, bigW * sizeof(float));
            }

            for (float& v : cropped) {
                if (!std::isfinite(v)) v = 0.f;
            }

            srcData = cropped.data();
            srcWidth = bigW;
            srcHeight = croppedH;
            dstWidth = bigdepthWidth_;
            dstHeight = bigdepthHeight_;
            break;
        }
    }

    // Step 1. Horizontal flip
    std::vector<float> flipped(srcWidth * srcHeight);
    vImage_Buffer srcBuf = {
        .data = const_cast<float*>(srcData),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * sizeof(float))
    };
    vImage_Buffer flipBuf = {
        .data = flipped.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * sizeof(float))
    };
    vImageHorizontalReflect_PlanarF(&srcBuf, &flipBuf, kvImageDoNotTile);

    // Step 2. Downscale if necessary
    std::vector<float> scaled(dstWidth * dstHeight);
    vImage_Buffer dstBuf = {
        .data = scaled.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * sizeof(float))
    };

    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_PlanarF(&flipBuf, &dstBuf, nullptr, kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(scaled.data(), flipped.data(), flipped.size() * sizeof(float));
    }

    // Step 3. Convert float depth (mm) → uint16_t normalized (0–65535)
    const size_t pixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(pixelCount);

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        const float d = scaled[i];
        out[i] = (d > 100.0f && d < 4500.0f)
                 ? static_cast<uint16_t>(d / 4500.0f * 65535.0f)
                 : 0;
    }

    hasNewDepth = false;
    return true;
}

// Get IR frame
bool MyFreenect2Device::getIRFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewIR) return false;

    const int srcWidth = IR_WIDTH;
    const int srcHeight = IR_HEIGHT;
    const int dstWidth = irWidth_;
    const int dstHeight = irHeight_;

    const size_t srcPixelCount = srcWidth * srcHeight;
    if (out.size() != static_cast<size_t>(dstWidth * dstHeight)) {
        out.resize(static_cast<size_t>(dstWidth * dstHeight));
    }

    vImage_Buffer src = {
        .data = (void*)irBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = srcWidth * sizeof(float)
    };

    std::vector<float> reflected(srcPixelCount);
    vImage_Buffer tmp = {
        .data = reflected.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = srcWidth * sizeof(float)
    };

    vImageHorizontalReflect_PlanarF(&src, &tmp, kvImageNoFlags);

    std::vector<float> scaled(dstWidth * dstHeight);
    vImage_Buffer dst = {
        .data = scaled.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = dstWidth * sizeof(float)
    };

    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_PlanarF(&tmp, &dst, nullptr, kvImageHighQualityResampling);
    } else {
        std::memcpy(scaled.data(), reflected.data(), srcPixelCount * sizeof(float));
    }

    // ---- Convert to uint16_t ----
    const float* srcData = scaled.data();
    const size_t pixelCount = static_cast<size_t>(dstWidth * dstHeight);

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = srcData[i];
        if (!std::isfinite(d) || d <= 0.f) d = 0.f;
        out[i] = static_cast<uint16_t>(std::min(d, 65535.f));
    }

    hasNewIR = false;
    return true;
}

// Set RGB buffer and mark as ready
void MyFreenect2Device::setRGBBuffer(const std::vector<uint8_t>& buffer, bool markReady) {
    std::lock_guard<std::mutex> lock(mutex);
    rgbBuffer = buffer;
    hasNewRGB = markReady;
    if (markReady) rgbReady = true;
}

// Set depth buffer and mark as ready
void MyFreenect2Device::setDepthBuffer(const std::vector<float>& buffer, bool markReady) {
    std::lock_guard<std::mutex> lock(mutex);
    depthBuffer = buffer;
    hasNewDepth = markReady;
    if (markReady) depthReady = true;
}
