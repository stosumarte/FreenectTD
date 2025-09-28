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
MyFreenect2Device::MyFreenect2Device(libfreenect2::Freenect2Device* dev,
                                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag)
    : device(dev), listener(nullptr), rgbReady(rgbFlag), depthReady(depthFlag),
      rgbBuffer(WIDTH * HEIGHT * 4, 0), depthBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0),
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
    return device->startStreams(true,true);
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

// Set RGB buffer and mark as ready
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
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (rgb && rgb->data && rgb->width == WIDTH && rgb->height == HEIGHT) {
            memcpy(rgbBuffer.data(), rgb->data, WIDTH * HEIGHT * 4);
            hasNewRGB = true;
            rgbReady = true;
        }
        if (depth && depth->data && depth->width == DEPTH_WIDTH && depth->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(depth->data);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, depthBuffer.begin());
            hasNewDepth = true;
            depthReady = true;
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

// Get color frame with flipping and optional downscaling
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    const int srcWidth = WIDTH;
    const int srcHeight = HEIGHT;
    const int dstWidth = downscale ? SCALED_WIDTH : WIDTH;
    const int dstHeight = downscale ? SCALED_HEIGHT : HEIGHT;
    const size_t dstSize = dstWidth * dstHeight * 4;
    if (out.size() != dstSize) out.resize(dstSize);

    vImage_Buffer src = {
        .data = rgbBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = srcWidth * 4
    };

    vImage_Buffer dst = {
        .data = out.data(),
        .height = (vImagePixelCount)dstHeight,
        .width = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * 4)
    };

    // Resize (bilinear)
    vImageScale_ARGB8888(&src, &dst, nullptr, kvImageHighQualityResampling);

    // Flip vertically
    vImageVerticalReflect_ARGB8888(&dst, &dst, kvImageNoFlags);

    // Flip horizontally
    vImageHorizontalReflect_ARGB8888(&dst, &dst, kvImageNoFlags);

    // BGRA -> RGBA (permute channels)
    const uint8_t permuteMap[4] = {2, 1, 0, 3}; // B,G,R,A -> R,G,B,A
    vImagePermuteChannels_ARGB8888(&dst, &dst, permuteMap, kvImageNoFlags);

    hasNewRGB = false;
    return true;
}

// Get depth frame with always horizontal mirroring
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewDepth) return false;

    // Flip depthBuffer vertically using Accelerate
    const size_t pixelCount = DEPTH_WIDTH * DEPTH_HEIGHT;
    if (out.size() != pixelCount) out.resize(pixelCount);

    vImage_Buffer src = {
        .data = (void*)depthBuffer.data(),
        .height = (vImagePixelCount)DEPTH_HEIGHT,
        .width = (vImagePixelCount)DEPTH_WIDTH,
        .rowBytes = DEPTH_WIDTH * sizeof(float)
    };

    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)DEPTH_HEIGHT,
        .width = (vImagePixelCount)DEPTH_WIDTH,
        .rowBytes = DEPTH_WIDTH * sizeof(float)
    };

    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    vImageHorizontalReflect_PlanarF(&dst, &dst, kvImageNoFlags);

    // Convert to uint16_t
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = flippedData[i];
        out[i] = (d > 100.0f && d < 4500.0f) ?
                 static_cast<uint16_t>(d / 4500.0f * 65535.0f) : 0;
    }

    hasNewDepth = false;
    return true;
}

// Get undistorted depth frame using libfreenect2::Registration
bool MyFreenect2Device::getUndistortedDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewDepth || !device) return false;

    static libfreenect2::Registration* reg = nullptr;
    static libfreenect2::Frame depthFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));
    static libfreenect2::Frame undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));

    if (!reg) {
        const libfreenect2::Freenect2Device::IrCameraParams& irParams = device->getIrCameraParams();
        const libfreenect2::Freenect2Device::ColorCameraParams& colorParams = device->getColorCameraParams();
        reg = new libfreenect2::Registration(irParams, colorParams);
    }

    std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
    reg->undistortDepth(&depthFrame, &undistortedFrame);

    // Flip undistortedFrame vertically using Accelerate
    const size_t pixelCount = DEPTH_WIDTH * DEPTH_HEIGHT;
    if (out.size() != pixelCount) out.resize(pixelCount);

    vImage_Buffer src = {
        .data = undistortedFrame.data,
        .height = (vImagePixelCount)DEPTH_HEIGHT,
        .width = (vImagePixelCount)DEPTH_WIDTH,
        .rowBytes = DEPTH_WIDTH * sizeof(float)
    };

    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)DEPTH_HEIGHT,
        .width = (vImagePixelCount)DEPTH_WIDTH,
        .rowBytes = DEPTH_WIDTH * sizeof(float)
    };

    vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    vImageHorizontalReflect_PlanarF(&dst, &dst, kvImageNoFlags);

    // Convert to uint16_t
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = flippedData[i];
        out[i] = (d > 100.0f && d < 4500.0f) ?
                 static_cast<uint16_t>(d / 4500.0f * 65535.0f) : 0;
    }

    hasNewDepth = false;
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
