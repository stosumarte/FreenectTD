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

// Get RGB, depth and IR resolutions
void MyFreenect2Device::setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight) {
    rgbWidth_ = rgbWidth;
    rgbHeight_ = rgbHeight;
    depthWidth_ = depthWidth;
    depthHeight_ = depthHeight;
    irWidth_ = irWidth;
    irHeight_ = irHeight;
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
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool downscale, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    const int srcWidth = RGB_WIDTH;
    const int srcHeight = RGB_HEIGHT;
    const int dstWidth = downscale ? SCALED_WIDTH : RGB_WIDTH;
    const int dstHeight = downscale ? SCALED_HEIGHT : RGB_HEIGHT;
    const size_t dstSize = dstWidth * dstHeight * 4;
    if (out.size() != dstSize) out.resize(dstSize);
    
    // Flip and downscale with Accelerate
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

    // Flip horizontally
    vImageHorizontalReflect_ARGB8888(&dst, &dst, kvImageNoFlags);

    // BGRA -> RGBA (permute channels)
    const uint8_t permuteMap[4] = {2, 1, 0, 3}; // B,G,R,A -> R,G,B,A
    vImagePermuteChannels_ARGB8888(&dst, &dst, permuteMap, kvImageNoFlags);

    hasNewRGB = false;
    
    width  = dstWidth;
    height = dstHeight;
    
    return true;
}

// All-in-one depth frame retrieval (raw/undistorted/registered)
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out, fn2_depthType type, bool downscale, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth || !device) return false;
    
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

    const float* flipSrcData = nullptr;
    
    int flipSrcWidth;
    int flipSrcHeight;
    
    int flipDstWidth;
    int flipDstHeight;

    switch (type) {
        case fn2_depthType::Raw:
            flipSrcData = depthBuffer.data();
            flipSrcWidth = DEPTH_WIDTH;
            flipSrcHeight = DEPTH_HEIGHT;
            flipDstWidth = DEPTH_WIDTH;
            flipDstHeight = DEPTH_HEIGHT;
            break;

        case fn2_depthType::Undistorted:
            std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->undistortDepth(&depthFrame, &undistortedFrame);
            flipSrcData = reinterpret_cast<float*>(undistortedFrame.data);
            flipSrcWidth = DEPTH_WIDTH;
            flipSrcHeight = DEPTH_HEIGHT;
            flipDstWidth = DEPTH_WIDTH;
            flipDstHeight = DEPTH_HEIGHT;
            break;

        case fn2_depthType::Registered:
            std::memcpy(rgbFrame.data, rgbBuffer.data(), RGB_WIDTH * RGB_HEIGHT * 4);
            std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->apply(&rgbFrame, &depthFrame, &undistortedFrame, &registeredFrame, true, &bigdepthFrame);
            
            const float* bigDepthData = reinterpret_cast<float*>(bigdepthFrame.data);
            
            // Remove top and bottom 1 pixel rows from bigdepthFrame
            int bigdepthHeightCropped = BIGDEPTH_HEIGHT - 2;
            int bigdepthWidthCropped  = BIGDEPTH_WIDTH;
            std::vector<float> bigdepthBufferCropped(bigdepthWidthCropped * bigdepthHeightCropped);
            for (int y = 0; y < bigdepthHeightCropped; ++y) {
                const float* srcRow = bigDepthData + (y + 1) * bigdepthWidthCropped; // +1 skips top row
                float* dstRow       = bigdepthBufferCropped.data() + y * bigdepthWidthCropped;
                std::memcpy(dstRow, srcRow, bigdepthWidthCropped * sizeof(float));
            }
            // Sanitize cropped depth data
            for (float& v : bigdepthBufferCropped) {
                if (!std::isfinite(v)) v = 0.f;
            }
            
            flipSrcWidth  = bigdepthWidthCropped;
            flipSrcHeight = bigdepthHeightCropped;
            flipDstWidth  = downscale ? SCALED_WIDTH : bigdepthWidthCropped;
            flipDstHeight = downscale ? SCALED_HEIGHT : bigdepthHeightCropped;
            flipSrcData = bigdepthBufferCropped.data();

            break;
    }

    // Flip and downscale with Accelerate
    vImage_Buffer flipSrc = {
        .data = const_cast<float*>(flipSrcData),
        .height = (vImagePixelCount)flipSrcHeight,
        .width = (vImagePixelCount)flipSrcWidth,
        .rowBytes = flipSrcWidth * sizeof(float)
    };

    std::vector<float> flipDstBuffer(flipDstWidth * flipDstHeight);
    
    vImage_Buffer flipDst = {
        .data = flipDstBuffer.data(),
        .height = (vImagePixelCount)flipDstHeight,
        .width = (vImagePixelCount)flipDstWidth,
        .rowBytes = flipDstWidth * sizeof(float)
    };
    
    vImageScale_PlanarF(&flipSrc, &flipDst, nullptr, kvImageHighQualityResampling | kvImageDoNotTile);
    vImageHorizontalReflect_PlanarF(&flipDst, &flipDst, kvImageDoNotTile);
    
    LOG("flipSrcWidth: " << flipSrcWidth << ", flipSrcHeight: " << flipSrcHeight);
    LOG("flipDstWidth: " << flipDstWidth << ", flipDstHeight: " << flipDstHeight);

    // Final buffer to convert
    const float* outputBuffer = flipDstBuffer.data();
    
    size_t pixelCount = flipDstWidth * flipDstHeight;

    if (out.size() != pixelCount) out.resize(pixelCount);

    // Convert to uint16_t
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = outputBuffer[i];
        out[i] = (d > 100.0f && d < 4500.0f)
                 ? static_cast<uint16_t>(d / 4500.0f * 65535.0f)
                 : 0;
    }

    hasNewDepth = false;
    
    width  = flipDstWidth;
    height = flipDstHeight;

    return true;
}


// Get IR frame
bool MyFreenect2Device::getIRFrame(std::vector<uint16_t>& out, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex);
    
    int srcWidth = IR_WIDTH, srcHeight = IR_HEIGHT;

    if (!hasNewIR) return false;

    // Flip irBuffer vertically using Accelerate
    const size_t pixelCount = srcWidth * srcHeight;
    if (out.size() != pixelCount) out.resize(pixelCount);

    vImage_Buffer src = {
        .data = (void*)irBuffer.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = srcWidth * sizeof(float)
    };

    std::vector<float> flipped(pixelCount);
    vImage_Buffer dst = {
        .data = flipped.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = srcWidth * sizeof(float)
    };

    //vImageVerticalReflect_PlanarF(&src, &dst, kvImageNoFlags);
    vImageHorizontalReflect_PlanarF(&src, &dst, kvImageNoFlags);

    // Convert to uint16_t
    const float* flippedData = flipped.data();
    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = flippedData[i];
        out[i] = (d > 0.0f) ?
        static_cast<uint16_t>(std::min(d, 65535.0f)) : 0;
    }
    hasNewIR = false;
    
    width  = srcWidth;
    height = srcHeight;
    
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
