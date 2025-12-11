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

// depthFormatEnum enum definition (shared between v1 and v2)
enum class depthFormatEnum {
    Raw,
    RawUndistorted,
    Registered
};

// MyFreenect2Device class constructor
MyFreenect2Device::MyFreenect2Device(
    libfreenect2::Freenect2Device* dev,
    std::atomic<bool>& rgbFlag,
    std::atomic<bool>& depthFlag,
    std::atomic<bool>& irFlag
)
: device(dev),
  listener(nullptr),
  reg(nullptr),
  rgbReady(rgbFlag),
  depthReady(depthFlag),
  irReady(irFlag),
  rgbBuffer(RGB_WIDTH * RGB_HEIGHT * 4, 0),
  depthBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0),
  hasNewRGB(false),
  hasNewDepth(false),
  // Persistent frame buffers
  depthFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  rgbFrame(RGB_WIDTH, RGB_HEIGHT, 4),
  undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  registeredFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  bigdepthFrame(1920, 1082, 4)
{
    LOG("[FreenectV2.cpp] MyFreenect2Device constructor: device=" + std::to_string(reinterpret_cast<uintptr_t>(device)));
    listener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Color |
        libfreenect2::Frame::Ir |
        libfreenect2::Frame::Depth
    );
    LOG("[FreenectV2.cpp] MyFreenect2Device constructor: listener created at " + std::to_string(reinterpret_cast<uintptr_t>(listener)));
    device->setColorFrameListener(listener);
    LOG("[FreenectV2.cpp] MyFreenect2Device constructor: setColorFrameListener done");
    device->setIrAndDepthFrameListener(listener);
    LOG("[FreenectV2.cpp] MyFreenect2Device constructor: setIrAndDepthFrameListener done");
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
    LOG("[FreenectV2.cpp] start(): called, device=" + std::to_string(reinterpret_cast<uintptr_t>(device)));
    if (!device) {
        LOG("[FreenectV2.cpp] start(): device is null, returning false");
        return false;
    }
    bool result = device->startStreams(true, true);
    LOG("[FreenectV2.cpp] start(): startStreams returned " + std::to_string(result));
    if (!result) {
        return false;
    }
    stopWorker = false;
    if (!workerThread.joinable()) {
        workerThread = std::thread(&MyFreenect2Device::runWorker, this);
    }
    return true;
}

// Stop the device streams using libfreenect2 API
void MyFreenect2Device::stop() {
    stopWorker = true;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    if (device) {
        device->stop();
        device->close();
        LOG("[FreenectV2.cpp] device->stop + device->close");
    }
}

// Set RGB, depth and IR resolutions
void MyFreenect2Device::setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int pcWidth, int pcHeight, int irWidth, int irHeight) {
    rgbWidth_ = rgbWidth;
    rgbHeight_ = rgbHeight;
    depthWidth_ = depthWidth;
    depthHeight_ = depthHeight;
    pcWidth_ = pcWidth;
    pcHeight_ = pcHeight;
    irWidth_ = irWidth;
    irHeight_ = irHeight;
    bigdepthWidth_ = rgbWidth;
    bigdepthHeight_ = rgbHeight;
    /*LOG("setResolutions RGB: " + std::to_string(rgbWidth_) + "x" + std::to_string(rgbHeight_) +
        " Depth: " + std::to_string(depthWidth_) + "x" + std::to_string(depthHeight_) +
        " PC: " + std::to_string(pcWidth_) + "x" + std::to_string(pcHeight_) +
        " IR: " + std::to_string(irWidth_) + "x" + std::to_string(irHeight_));*/
}

// Process incoming frames
void MyFreenect2Device::processFrames() {
    if (!listener) {
        LOG("[FreenectV2.cpp] processFrames(): listener is null");
        return;
    }
    libfreenect2::FrameMap frames;
    if (!listener->waitForNewFrame(frames, 50)) {
        return;
    }
    libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
    libfreenect2::Frame* ir = frames[libfreenect2::Frame::Ir];
    LOG("[FreenectV2.cpp] processFrames(): got frame pointers - rgb=" + std::to_string(reinterpret_cast<uintptr_t>(rgb)) +
        " depth=" + std::to_string(reinterpret_cast<uintptr_t>(depth)) +
        " ir=" + std::to_string(reinterpret_cast<uintptr_t>(ir)));
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (rgb && rgb->data && rgb->width == RGB_WIDTH && rgb->height == RGB_HEIGHT) {
            std::memcpy(rgbBuffer.data(), rgb->data, RGB_WIDTH * RGB_HEIGHT * 4);
            hasNewRGB = true;
            rgbReady = true;
            LOG("[FreenectV2.cpp] processFrames(): RGB frame copied");
        } else if (rgb) {
            LOG("[FreenectV2.cpp] processFrames(): RGB frame invalid - data=" +
                std::to_string(reinterpret_cast<uintptr_t>(rgb->data)) +
                " size=" + std::to_string(rgb->width) + "x" + std::to_string(rgb->height));
        }
        if (depth && depth->data && depth->width == DEPTH_WIDTH && depth->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(depth->data);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, depthBuffer.begin());
            hasNewDepth = true;
            depthReady = true;
            LOG("[FreenectV2.cpp] processFrames(): Depth frame copied");
        } else if (depth) {
            LOG("[FreenectV2.cpp] processFrames(): Depth frame invalid - data=" +
                std::to_string(reinterpret_cast<uintptr_t>(depth->data)) +
                " size=" + std::to_string(depth->width) + "x" + std::to_string(depth->height));
        }
        if (ir && ir->data && ir->width == DEPTH_WIDTH && ir->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(ir->data);
            if (irBuffer.size() != DEPTH_WIDTH * DEPTH_HEIGHT)
                irBuffer.resize(DEPTH_WIDTH * DEPTH_HEIGHT);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, irBuffer.begin());
            hasNewIR = true;
            irReady = true;
            LOG("[FreenectV2.cpp] processFrames(): IR frame copied");
        } else if (ir) {
            LOG("[FreenectV2.cpp] processFrames(): IR frame invalid - data=" +
                std::to_string(reinterpret_cast<uintptr_t>(ir->data)) +
                " size=" + std::to_string(ir->width) + "x" + std::to_string(ir->height));
        }

    }
    LOG("[FreenectV2.cpp] processFrames(): calling listener->release");
    listener->release(frames);
    LOG("[FreenectV2.cpp] processFrames(): complete");
}

void MyFreenect2Device::runWorker() {
    LOG("[FreenectV2.cpp] runWorker(): thread started");
    while (!stopWorker.load()) {
        processFrames();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    LOG("[FreenectV2.cpp] runWorker(): thread exiting");
}

// NOTE: Call each getter at most once per TD cook; they are non-blocking and
// return false immediately when no new frame is available.
bool MyFreenect2Device::getRGB(std::vector<uint8_t>& out) {
    std::vector<uint8_t> local;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewRGB) return false;
        local = rgbBuffer;
        hasNewRGB = false;
    }
    out = std::move(local);
    return true;
}

bool MyFreenect2Device::getDepth(std::vector<float>& out) {
    std::vector<float> local;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewDepth) {
            return false;
        }
        local = depthBuffer;
        hasNewDepth = false;
    }
    out = std::move(local);
    return true;
}

bool MyFreenect2Device::getIR(std::vector<float>& out) {
    std::vector<float> local;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewIR) {
            return false;
        }
        local = irBuffer;
        hasNewIR = false;
    }
    out = std::move(local);
    return true;
}

bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out) {
    std::vector<uint8_t> localRGB;
    int dstWidth = 0;
    int dstHeight = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewRGB) return false;
        localRGB = rgbBuffer;
        hasNewRGB = false;
        dstWidth = rgbWidth_;
        dstHeight = rgbHeight_;
    }

    const int srcWidth  = RGB_WIDTH;
    const int srcHeight = RGB_HEIGHT;
    const size_t dstSize = static_cast<size_t>(dstWidth) * dstHeight * 4;
    if (localRGB.size() != static_cast<size_t>(srcWidth * srcHeight * 4) || dstWidth <= 0 || dstHeight <= 0) {
        return false;
    }
    out.resize(dstSize);

    vImage_Buffer src = {
        .data = localRGB.data(),
        .height = (vImagePixelCount)srcHeight,
        .width  = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4)
    };

    std::vector<uint8_t> tmpFlip(srcWidth * srcHeight * 4);
    vImage_Buffer tmpFlipBuf = {
        .data = tmpFlip.data(),
        .height = (vImagePixelCount)srcHeight,
        .width  = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4)
    };

    vImageHorizontalReflect_ARGB8888(&src, &tmpFlipBuf, kvImageDoNotTile);

    vImage_Buffer dst = {
        .data = out.data(),
        .height = (vImagePixelCount)dstHeight,
        .width  = (vImagePixelCount)dstWidth,
        .rowBytes = static_cast<size_t>(dstWidth * 4)
    };

    if (dstWidth != srcWidth || dstHeight != srcHeight) {
        vImageScale_ARGB8888(&tmpFlipBuf, &dst, nullptr,
                             kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(out.data(), tmpFlip.data(), tmpFlip.size());
    }

    const uint8_t permuteMap[4] = {2, 1, 0, 3};
    vImagePermuteChannels_ARGB8888(&dst, &dst, permuteMap, kvImageNoFlags);

    LOG("[FreenectV2.cpp] getColorFrame(): success, size=" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight));
    return true;
}

bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax) {
    LOG("[FreenectV2.cpp] getDepthFrame(): called with type=" + std::to_string(static_cast<int>(type)));
    std::vector<float> localDepth;
    std::vector<uint8_t> localRGB;
    libfreenect2::Freenect2Device* localDevice = nullptr;
    int dstWidth = 0;
    int dstHeight = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewDepth) {
            LOG("[FreenectV2.cpp] getDepthFrame(): no new depth data available");
            return false;
        }
        if (!device) {
            LOG("[FreenectV2.cpp] getDepthFrame(): device is null");
            return false;
        }
        localDevice = device;
        localDepth = depthBuffer;
        hasNewDepth = false;
        if (type == depthFormatEnum::Registered) {
            localRGB = rgbBuffer;
        }
        dstWidth = (type == depthFormatEnum::Registered) ? bigdepthWidth_ : depthWidth_;
        dstHeight = (type == depthFormatEnum::Registered) ? bigdepthHeight_ : depthHeight_;
    }

    if (!localDevice || localDepth.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT)) {
        LOG("[FreenectV2.cpp] getDepthFrame(): invalid local buffers");
        return false;
    }

    if (!reg) {
        LOG("[FreenectV2.cpp] getDepthFrame(): creating Registration object");
        const auto& irParams = localDevice->getIrCameraParams();
        const auto& colorParams = localDevice->getColorCameraParams();
        reg = std::make_unique<libfreenect2::Registration>(irParams, colorParams);
        if (!reg) {
            LOG("[FreenectV2.cpp] getDepthFrame(): Failed to create Registration object");
            return false;
        }
        LOG("[FreenectV2.cpp] getDepthFrame(): Registration object created successfully");
    }

    const float* srcData = nullptr;
    int srcWidth = 0, srcHeight = 0;

    switch (type) {
        case depthFormatEnum::Raw: {
            srcData = localDepth.data();
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            break;
        }
        case depthFormatEnum::RawUndistorted: {
            std::memcpy(depthFrame.data, localDepth.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->undistortDepth(&depthFrame, &undistortedFrame);
            srcData = reinterpret_cast<float*>(undistortedFrame.data);
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            break;
        }
        case depthFormatEnum::Registered: {
            if (localRGB.size() != static_cast<size_t>(RGB_WIDTH * RGB_HEIGHT * 4)) {
                LOG("[FreenectV2.cpp] getDepthFrame(): RGB buffer missing for registered depth");
                return false;
            }
            std::memcpy(rgbFrame.data, localRGB.data(), RGB_WIDTH * RGB_HEIGHT * 4);
            std::memcpy(depthFrame.data, localDepth.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
            reg->apply(&rgbFrame, &depthFrame, &undistortedFrame, &registeredFrame, true, &bigdepthFrame);

            const float* bigDepthData = reinterpret_cast<float*>(bigdepthFrame.data);
            const int croppedH = BIGDEPTH_HEIGHT - 2;

            if (registeredCroppedBuffer.size() != static_cast<size_t>(BIGDEPTH_WIDTH * croppedH))
                registeredCroppedBuffer.resize(BIGDEPTH_WIDTH * croppedH);

            for (int y = 0; y < croppedH; ++y) {
                const float* srcRow = bigDepthData + (y + 1) * BIGDEPTH_WIDTH;
                float* dstRow = registeredCroppedBuffer.data() + y * BIGDEPTH_WIDTH;
                std::memcpy(dstRow, srcRow, BIGDEPTH_WIDTH * sizeof(float));
            }

            int validPixels = 0;
            for (float& v : registeredCroppedBuffer) {
                if (!std::isfinite(v)) v = 0.f;
                if (v > 100.0f && v < 4500.0f) validPixels++;
            }
            int totalPixels = BIGDEPTH_WIDTH * croppedH;
            int requiredValidPixels = totalPixels / 10;
            lastRegisteredDepthValid = (validPixels >= requiredValidPixels);
            if (!lastRegisteredDepthValid) {
                LOG("[FreenectV2.cpp] Not enough valid pixels in registered depth: " + std::to_string(validPixels) + "/" + std::to_string(totalPixels));
                return false;
            }
            static int frameCounter = 0;
            frameCounter++;
            if (frameCounter % 30 == 0) {
                LOG("[FreenectV2.cpp] Frame " + std::to_string(frameCounter) + ": Valid pixels: " + std::to_string(validPixels) + "/" + std::to_string(totalPixels));
            }
            srcData = registeredCroppedBuffer.data();
            srcWidth = BIGDEPTH_WIDTH;
            srcHeight = croppedH;
            break;
        }
    }

    if (!srcData || dstWidth <= 0 || dstHeight <= 0) {
        LOG("[FreenectV2.cpp] getDepthFrame(): invalid source/destination dimensions");
        return false;
    }

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

    std::vector<float> scaled(static_cast<size_t>(dstWidth) * dstHeight);
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

    const size_t pixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(pixelCount);
    const float denom = std::max(depthThreshMax - depthThreshMin, 1.0f);

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        const float d = scaled[i];
        if (!std::isfinite(d) || d <= depthThreshMin || d >= depthThreshMax) {
            out[i] = 0;
        } else {
            float normalized = (d - depthThreshMin) / denom;
            normalized = std::clamp(normalized, 0.0f, 1.0f);
            out[i] = static_cast<uint16_t>(normalized * 65535.0f);
        }
    }

    LOG("[FreenectV2.cpp] getDepthFrame(): success, size=" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight));
    return true;
}

bool MyFreenect2Device::getPointCloudFrame(std::vector<float>& out) {
    LOG("[FreenectV2.cpp] getPointCloudFrame(): called");
    std::vector<uint8_t> localRGB;
    std::vector<float> localDepth;
    libfreenect2::Freenect2Device* localDevice = nullptr;
    int dstWidth = 0;
    int dstHeight = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!device) {
            LOG("[FreenectV2.cpp] getPointCloudFrame(): device is null");
            return false;
        }
        /*if (!hasNewDepth) {
            LOG("[FreenectV2.cpp] getPointCloudFrame(): no new depth data");
            return false;
        }*/
        localDevice = device;
        localDepth = depthBuffer;
        localRGB = rgbBuffer;
        dstWidth = pcWidth_;
        dstHeight = pcHeight_;
    }

    LOG("[FreenectV2.cpp] getPointCloudFrame(): localDepth size = " + std::to_string(localDepth.size()));
    LOG("[FreenectV2.cpp] getPointCloudFrame(): localRGB size = " + std::to_string(localRGB.size()));
    LOG("[FreenectV2.cpp] getPointCloudFrame(): DEPTH_WIDTH * DEPTH_HEIGHT = " + std::to_string(DEPTH_WIDTH * DEPTH_HEIGHT));
    LOG("[FreenectV2.cpp] getPointCloudFrame(): RGB_WIDTH * RGB_HEIGHT * 4 = " + std::to_string(RGB_WIDTH * RGB_HEIGHT * 4));

    if (!localDevice || localDepth.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT)) {
        LOG("[FreenectV2.cpp] getPointCloudFrame(): device is null or depth size mismatch");
        return false;
    }

    if (!reg) {
        LOG("[FreenectV2.cpp] getPointCloudFrame(): creating Registration object");
        const auto& irParams = localDevice->getIrCameraParams();
        const auto& colorParams = localDevice->getColorCameraParams();
        LOG("[FreenectV2.cpp] getPointCloudFrame(): IR params: fx = " + std::to_string(irParams.fx) + ", fy = " + std::to_string(irParams.fy) + ", cx = " + std::to_string(irParams.cx) + ", cy = " + std::to_string(irParams.cy));
        LOG("[FreenectV2.cpp] getPointCloudFrame(): Color params: fx = " + std::to_string(colorParams.fx) + ", fy = " + std::to_string(colorParams.fy) + ", cx = " + std::to_string(colorParams.cx) + ", cy = " + std::to_string(colorParams.cy));
        reg = std::make_unique<libfreenect2::Registration>(irParams, colorParams);
        if (!reg) {
            LOG("[FreenectV2.cpp] getPointCloudFrame(): Failed to create Registration object");
            return false;
        }
        LOG("[FreenectV2.cpp] getPointCloudFrame(): Registration object created");
    }

    std::memcpy(rgbFrame.data, localRGB.data(), RGB_WIDTH * RGB_HEIGHT * 4);
    std::memcpy(depthFrame.data, localDepth.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
    reg->apply(&rgbFrame, &depthFrame, &undistortedFrame, &registeredFrame, true, nullptr);

    const int srcWidth = DEPTH_WIDTH;
    const int srcHeight = DEPTH_HEIGHT;

    out.resize(static_cast<size_t>(srcWidth) * srcHeight * 4);
    float* outPtr = out.data();
    for (int r = 0; r < srcHeight; ++r) {
        for (int c = 0; c < srcWidth; ++c) {
            float x, y, z;
            reg->getPointXYZ(&undistortedFrame, r, c, x, y, z);
            size_t idx = (r * srcWidth + c) * 4;
            if (z > 0) {
                outPtr[idx + 0] = x;
                outPtr[idx + 1] = -y;
                outPtr[idx + 2] = z;
            } else {
                outPtr[idx + 0] = 0.0f;
                outPtr[idx + 1] = 0.0f;
                outPtr[idx + 2] = 0.0f;
            }
            outPtr[idx + 3] = 1.0f;
        }
    }

    const float* srcData = out.data();
    const int pcDstWidth = dstWidth;
    const int pcDstHeight = dstHeight;

    std::vector<float> flipped(static_cast<size_t>(srcWidth) * srcHeight * 4);
    vImage_Buffer srcBuf = {
        .data = const_cast<float*>(srcData),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4 * sizeof(float))
    };
    vImage_Buffer flipBuf = {
        .data = flipped.data(),
        .height = (vImagePixelCount)srcHeight,
        .width = (vImagePixelCount)srcWidth,
        .rowBytes = static_cast<size_t>(srcWidth * 4 * sizeof(float))
    };
    vImageHorizontalReflect_ARGBFFFF(&srcBuf, &flipBuf, kvImageDoNotTile);

    std::vector<float> scaled(static_cast<size_t>(pcDstWidth) * pcDstHeight * 4);
    vImage_Buffer dstBuf = {
        .data = scaled.data(),
        .height = (vImagePixelCount)pcDstHeight,
        .width = (vImagePixelCount)pcDstWidth,
        .rowBytes = static_cast<size_t>(pcDstWidth * 4 * sizeof(float))
    };

    if (pcDstWidth != srcWidth || pcDstHeight != srcHeight) {
        vImageScale_ARGBFFFF(&flipBuf, &dstBuf, nullptr, kvImageHighQualityResampling | kvImageDoNotTile);
    } else {
        std::memcpy(scaled.data(), flipped.data(), flipped.size() * sizeof(float));
    }

    out = std::move(scaled);

    LOG("[FreenectV2.cpp] getPointCloudFrame(): success");
    return true;
}

bool MyFreenect2Device::getIRFrame(std::vector<uint16_t>& out) {
    LOG("[FreenectV2.cpp] getIRFrame(): called");
    std::vector<float> localIR;
    int dstWidth = 0;
    int dstHeight = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasNewIR) {
            LOG("[FreenectV2.cpp] getIRFrame(): no new IR data");
            return false;
        }
        localIR = irBuffer;
        hasNewIR = false;
        dstWidth = irWidth_;
        dstHeight = irHeight_;
    }

    const int srcWidth = IR_WIDTH;
    const int srcHeight = IR_HEIGHT;
    const size_t srcPixelCount = static_cast<size_t>(srcWidth * srcHeight);
    if (localIR.size() != srcPixelCount || dstWidth <= 0 || dstHeight <= 0) {
        return false;
    }

    vImage_Buffer src = {
        .data = const_cast<float*>(localIR.data()),
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

    std::vector<float> scaled(static_cast<size_t>(dstWidth) * dstHeight);
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

    const float* srcData = scaled.data();
    const size_t pixelCount = static_cast<size_t>(dstWidth) * dstHeight;

    if (out.size() != pixelCount) {
        out.resize(pixelCount);
    }

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        float d = srcData[i];
        if (!std::isfinite(d) || d <= 0.f) d = 0.f;
        out[i] = static_cast<uint16_t>(std::min(d, 65535.f));
    }

    LOG("[FreenectV2.cpp] getIRFrame(): success, size=" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight));
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
