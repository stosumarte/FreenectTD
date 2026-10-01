//
//  FreenectV2.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV2.h"
#include <cstring>
#include <cmath>
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
            ++depthSeq;
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

// Run libfreenect2 registration once per depth frame and cache the results
// (undistortedFrame, registeredFrame, colorDepthMap and optionally bigdepthFrame)
// so that depth, point cloud and registered color outputs share one apply() call.
bool MyFreenect2Device::ensureRegistration(bool needBigdepth) {
    libfreenect2::Freenect2Device* localDevice = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!device) {
            LOG("[FreenectV2.cpp] ensureRegistration(): device is null");
            return false;
        }
        if (depthSeq == regSeq && (regHasBigdepth || !needBigdepth) && depthSeq != 0) {
            return true; // already registered this depth frame
        }
        localDevice = device;
        if (rgbBuffer.size() != static_cast<size_t>(RGB_WIDTH * RGB_HEIGHT * 4) ||
            depthBuffer.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT)) {
            LOG("[FreenectV2.cpp] ensureRegistration(): invalid buffers");
            return false;
        }
        // rgbFrame / depthFrame are only touched from the cook thread, so copying
        // straight into them under the lock avoids an extra 8 MB copy per frame.
        std::memcpy(rgbFrame.data, rgbBuffer.data(), RGB_WIDTH * RGB_HEIGHT * 4);
        std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
        regSeq = depthSeq;
    }

    if (!reg) {
        const auto& irParams = localDevice->getIrCameraParams();
        const auto& colorParams = localDevice->getColorCameraParams();
        LOG("[FreenectV2.cpp] ensureRegistration(): IR params fx=" + std::to_string(irParams.fx) + " fy=" + std::to_string(irParams.fy) + " cx=" + std::to_string(irParams.cx) + " cy=" + std::to_string(irParams.cy));
        LOG("[FreenectV2.cpp] ensureRegistration(): Color params fx=" + std::to_string(colorParams.fx) + " fy=" + std::to_string(colorParams.fy) + " cx=" + std::to_string(colorParams.cx) + " cy=" + std::to_string(colorParams.cy));
        colorCameraParams = colorParams;
        reg = std::make_unique<libfreenect2::Registration>(irParams, colorParams);
        if (!reg) {
            LOG("[FreenectV2.cpp] ensureRegistration(): failed to create Registration");
            return false;
        }
    }

    if (colorDepthMap.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT))
        colorDepthMap.assign(DEPTH_WIDTH * DEPTH_HEIGHT, -1);

    reg->apply(&rgbFrame, &depthFrame, &undistortedFrame, &registeredFrame,
               /*enable_filter=*/true,
               needBigdepth ? &bigdepthFrame : nullptr,
               colorDepthMap.data());
    regHasBigdepth = needBigdepth;
    return true;
}

// Output: float depth in millimetres, 0 = invalid or outside [threshMin, threshMax].
// Size is depthWidth_ x depthHeight_ (Raw/RawUndistorted) or bigdepthWidth_ x bigdepthHeight_ (Registered).
bool MyFreenect2Device::getDepthFrame(std::vector<float>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax) {
    LOG("[FreenectV2.cpp] getDepthFrame(): called with type=" + std::to_string(static_cast<int>(type)));
    std::vector<float> localDepth;
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
        hasNewDepth = false;
        if (type == depthFormatEnum::Raw) {
            localDepth = depthBuffer;
        }
        dstWidth = (type == depthFormatEnum::Registered) ? bigdepthWidth_ : depthWidth_;
        dstHeight = (type == depthFormatEnum::Registered) ? bigdepthHeight_ : depthHeight_;
    }

    const float* srcData = nullptr;
    int srcWidth = 0, srcHeight = 0;

    switch (type) {
        case depthFormatEnum::Raw: {
            if (localDepth.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT)) return false;
            srcData = localDepth.data();
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            break;
        }
        case depthFormatEnum::RawUndistorted: {
            if (!ensureRegistration(false)) return false;
            srcData = reinterpret_cast<const float*>(undistortedFrame.data);
            srcWidth = DEPTH_WIDTH;
            srcHeight = DEPTH_HEIGHT;
            break;
        }
        case depthFormatEnum::Registered: {
            if (!ensureRegistration(true)) return false;
            // bigdepth is 1920x1082 with one padding row on top and bottom
            srcData = reinterpret_cast<const float*>(bigdepthFrame.data) + BIGDEPTH_WIDTH;
            srcWidth = BIGDEPTH_WIDTH;
            srcHeight = BIGDEPTH_HEIGHT - 2;
            break;
        }
    }

    if (!srcData || dstWidth <= 0 || dstHeight <= 0) {
        LOG("[FreenectV2.cpp] getDepthFrame(): invalid source/destination dimensions");
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(dstWidth) * dstHeight;
    out.resize(pixelCount);
    resampleNearest(srcData, srcWidth, srcHeight, 1, out.data(), dstWidth, dstHeight, /*flipX=*/true);

    #pragma omp parallel for if(pixelCount > 100000)
    for (size_t i = 0; i < pixelCount; ++i) {
        const float d = out[i];
        if (!std::isfinite(d) || d <= depthThreshMin || d >= depthThreshMax) out[i] = 0.0f;
    }

    LOG("[FreenectV2.cpp] getDepthFrame(): success, size=" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight));
    return true;
}

// Output: RGBA32F, XYZ in metres, A = 1 for valid points and 0 for invalid ones.
//  DepthCamera: 512x424 grid, XYZ relative to the depth camera (libfreenect2 getPointXYZ).
//  ColorCamera: 1920x1080 grid, XYZ relative to the color camera, pixel-aligned with
//               the RGB output and the Registered depth map so the RGB image can be
//               applied as a texture with plain (u,v) = pixel position.
bool MyFreenect2Device::getPointCloudFrame(std::vector<float>& out, pcSpaceEnum space, float depthThreshMin, float depthThreshMax,
                                           bool flipX, bool flipY, bool flipZ, const float* unknownXYZ) {
    const float unknownX = unknownXYZ[0];
    const float unknownY = unknownXYZ[1];
    const float unknownZ = unknownXYZ[2];
    LOG("[FreenectV2.cpp] getPointCloudFrame(): called, space=" + std::to_string(static_cast<int>(space)));
    int dstWidth = 0;
    int dstHeight = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        dstWidth = pcWidth_;
        dstHeight = pcHeight_;
    }
    if (dstWidth <= 0 || dstHeight <= 0) return false;

    const bool colorSpace = (space == pcSpaceEnum::ColorCamera);
    if (!ensureRegistration(colorSpace)) return false;

    const int srcWidth  = colorSpace ? BIGDEPTH_WIDTH : DEPTH_WIDTH;
    const int srcHeight = colorSpace ? (BIGDEPTH_HEIGHT - 2) : DEPTH_HEIGHT;
    const float signX = flipX ? -1.0f : 1.0f;
    const float signY = flipY ? -1.0f : 1.0f;
    const float signZ = flipZ ? -1.0f : 1.0f;

    if (pcScratch.size() != static_cast<size_t>(srcWidth) * srcHeight * 4)
        pcScratch.resize(static_cast<size_t>(srcWidth) * srcHeight * 4);
    float* points = pcScratch.data();

    if (colorSpace) {
        const auto& colorParams = colorCameraParams; // captured when the Registration object was created
        const float fxInv = 1.0f / colorParams.fx;
        const float fyInv = 1.0f / colorParams.fy;
        const float* bigdepthRows = reinterpret_cast<const float*>(bigdepthFrame.data) + BIGDEPTH_WIDTH; // skip padding row
        #pragma omp parallel for
        for (int r = 0; r < srcHeight; ++r) {
            const float* row = bigdepthRows + static_cast<size_t>(r) * BIGDEPTH_WIDTH;
            float* dstRow = points + static_cast<size_t>(r) * srcWidth * 4;
            const float yNorm = -(r + 0.5f - colorParams.cy) * fyInv; // negate: +Y up (matches depth-camera path)
            for (int c = 0; c < srcWidth; ++c) {
                const float depth = row[c];
                float* point = dstRow + c * 4;
                if (std::isfinite(depth) && depth > depthThreshMin && depth < depthThreshMax) {
                    const float z = depth * 0.001f;
                    point[0] = signX * (c + 0.5f - colorParams.cx) * fxInv * z;
                    point[1] = signY * yNorm * z;
                    point[2] = signZ * z;
                    point[3] = 1.0f;
                } else {
                    // Unknown point value, alpha stays 0
                    point[0] = unknownX;
                    point[1] = unknownY;
                    point[2] = unknownZ;
                    point[3] = 0.0f;
                }
            }
        }
    } else {
        for (int r = 0; r < srcHeight; ++r) {
            for (int c = 0; c < srcWidth; ++c) {
                float x, y, z;
                reg->getPointXYZ(&undistortedFrame, r, c, x, y, z);
                float* point = points + (static_cast<size_t>(r) * srcWidth + c) * 4;
                const float zMillimeters = z * 1000.0f;
                if (std::isfinite(z) && zMillimeters > depthThreshMin && zMillimeters < depthThreshMax) {
                    point[0] = signX * x;
                    point[1] = -signY * y;
                    point[2] = signZ * z;
                    point[3] = 1.0f;
                } else {
                    // Unknown point value, alpha stays 0
                    point[0] = unknownX;
                    point[1] = unknownY;
                    point[2] = unknownZ;
                    point[3] = 0.0f;
                }
            }
        }
    }

    out.resize(static_cast<size_t>(dstWidth) * dstHeight * 4);
    resampleNearest(points, srcWidth, srcHeight, 4, out.data(), dstWidth, dstHeight, /*flipX=*/true);

    LOG("[FreenectV2.cpp] getPointCloudFrame(): success");
    return true;
}

// Registered color + depth-to-color UV map (512x424, aligned with the depth-camera point cloud)
// color: RGBA8, the RGB image re-sampled onto the depth grid (A = 255 where a color pixel exists, 0 otherwise)
// uv:    RGBA32F, (u, v, 0, valid) giving where each depth pixel lands in the RGB output, in
//        TouchDesigner UV convention (0..1, origin bottom-left, already mirrored to match the flipped RGB output).
//        Feed it to a Remap TOP together with the RGB output to get registered color at full resolution.
bool MyFreenect2Device::getRegisteredColorFrame(std::vector<uint8_t>& color, std::vector<float>& uv) {
    if (!ensureRegistration(false)) return false;

    const int width = DEPTH_WIDTH, height = DEPTH_HEIGHT;
    color.resize(static_cast<size_t>(width) * height * 4);
    uv.resize(static_cast<size_t>(width) * height * 4);

    const uint8_t* registeredData = registeredFrame.data; // BGRX
    const float invRgbWidth = 1.0f / RGB_WIDTH;
    const float invRgbHeight = 1.0f / RGB_HEIGHT;

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            const size_t srcIndex = static_cast<size_t>(r) * width + c;
            const size_t dstIndex = static_cast<size_t>(r) * width + (width - 1 - c); // mirror to match other outputs
            const int colorIndex = colorDepthMap[srcIndex];
            const bool valid = colorIndex >= 0;

            uint8_t* colorPixel = color.data() + dstIndex * 4;
            colorPixel[0] = registeredData[srcIndex * 4 + 2];
            colorPixel[1] = registeredData[srcIndex * 4 + 1];
            colorPixel[2] = registeredData[srcIndex * 4 + 0];
            colorPixel[3] = valid ? 255 : 0;

            float* uvPixel = uv.data() + dstIndex * 4;
            if (valid) {
                const int colorX = colorIndex % RGB_WIDTH;
                const int colorY = colorIndex / RGB_WIDTH;
                uvPixel[0] = 1.0f - (colorX + 0.5f) * invRgbWidth; // RGB output is mirrored
                uvPixel[1] = 1.0f - (colorY + 0.5f) * invRgbHeight; // TD UV origin is bottom-left
                uvPixel[2] = 0.0f;
                uvPixel[3] = 1.0f;
            } else {
                uvPixel[0] = uvPixel[1] = uvPixel[2] = uvPixel[3] = 0.0f;
            }
        }
    }
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
    ++depthSeq;
    hasNewDepth = markReady;
    if (markReady) depthReady = true;
}
