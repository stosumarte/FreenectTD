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
#include <dispatch/dispatch.h>

// MyFreenect2Device class constructor
MyFreenect2Device::MyFreenect2Device(libfreenect2::Freenect2Device* dev, TD::TOP_Context* ctx)
: device(dev),
  context(ctx),
  listener(nullptr),
  // Persistent registration output frames
  undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  registeredFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  bigdepthFrame(BIGDEPTH_WIDTH, BIGDEPTH_HEIGHT, 4),
  reg(nullptr)
{
    LOG("[FreenectV2.cpp] MyFreenect2Device constructor: device=" + std::to_string(reinterpret_cast<uintptr_t>(device)));
    listener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Color |
        libfreenect2::Frame::Ir |
        libfreenect2::Frame::Depth
    );
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
    LOG("[FreenectV2.cpp] start(): called, device=" + std::to_string(reinterpret_cast<uintptr_t>(device)));
    if (!device) {
        LOG("[FreenectV2.cpp] start(): device is null, returning false");
        return false;
    }
    // libfreenect2 clips depth to 0.5-4.5 m by default. Widen that once, before streaming, to all the v2 can
    // see (it can't measure closer than 0.5 m); the node's depth range is applied in processFrames.
    // Never call setConfiguration while streaming: the OpenCL depth processor rebuilds its kernels on the
    // calling thread while its own thread is running them, which crashes.
    libfreenect2::Freenect2Device::Config config;
    config.MinDepth = 0.5f;
    config.MaxDepth = 8.0f;
    device->setConfiguration(config);
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

// Sequence number of the latest depth frame; changes whenever a new depth frame arrives
uint64_t MyFreenect2Device::getDepthSeq() {
    std::lock_guard<std::mutex> lock(mutex);
    return depthSeq;
}

void MyFreenect2Device::setSettings(const V2Settings& newSettings) {
    std::lock_guard<std::mutex> lock(mutex);
    settings = newSettings;
}

void MyFreenect2Device::takeImages(Images& out) {
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        out[i] = std::move(images[i]);
    }
}

// Turns each frame set into finished output buffers here, on the worker thread, so the cook only
// uploads them: the conversions below take tens of ms per frame set at 1920x1080, which would
// otherwise hold up TouchDesigner's main thread, once per node.
void MyFreenect2Device::processFrames() {
    if (!listener) {
        LOG("[FreenectV2.cpp] processFrames(): listener is null");
        return;
    }
    // Our libfreenect2 is built with TinyThread, where waitForNewFrame ignores its timeout and
    // blocks until a frame arrives. After an unplug none ever does and stop() hangs joining this
    // thread, so only wait once a frame is ready (this thread is the listener's only consumer).
    if (!listener->hasNewFrame()) {
        return;
    }
    libfreenect2::FrameMap frames;
    listener->waitForNewFrame(frames);
    const libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
    const libfreenect2::Frame* ir = frames[libfreenect2::Frame::Ir];
    const bool rgbOK = rgb && rgb->data && rgb->width == RGB_WIDTH && rgb->height == RGB_HEIGHT;
    const bool depthOK = depth && depth->data && depth->width == DEPTH_WIDTH && depth->height == DEPTH_HEIGHT;
    const bool irOK = ir && ir->data && ir->width == IR_WIDTH && ir->height == IR_HEIGHT;
    if (!rgbOK || !depthOK || !irOK) {
        LOG("[FreenectV2.cpp] processFrames(): invalid frame - rgb=" + std::to_string(rgbOK) +
            " depth=" + std::to_string(depthOK) + " ir=" + std::to_string(irOK));
    }

    V2Settings s;
    {
        std::lock_guard<std::mutex> lock(mutex);
        s = settings;
    }

    if (depthOK) {
        // The node's depth range, applied before registration so every output clips the same way
        float* mm = reinterpret_cast<float*>(depth->data);
        for (size_t i = 0; i < static_cast<size_t>(DEPTH_WIDTH) * DEPTH_HEIGHT; ++i) {
            if (!isDepthInRange(mm[i], s.threshMin, s.threshMax)) {
                mm[i] = 0.0f;
            }
        }
    }

    Images made;
    if (rgbOK) {
        made[0] = makeColor(*rgb, s);
    }
    if (depthOK) {
        // One registration per frame set, shared by every output that needs it
        const bool registered = (s.depthFormat == depthFormatEnum::Registered);
        const bool needReg = (s.depth && s.depthFormat != depthFormatEnum::Raw) || s.pointCloud || s.regColor || s.uv;
        bool regOK = false;
        if (needReg && rgbOK) {
            if (!reg) {
                colorCameraParams = device->getColorCameraParams();
                reg = std::make_unique<libfreenect2::Registration>(device->getIrCameraParams(), colorCameraParams);
            }
            if (colorDepthMap.size() != static_cast<size_t>(DEPTH_WIDTH * DEPTH_HEIGHT)) {
                colorDepthMap.assign(DEPTH_WIDTH * DEPTH_HEIGHT, -1);
            }
            reg->apply(rgb, depth, &undistortedFrame, &registeredFrame, /*enable_filter=*/true,
                       (registered && (s.depth || s.pointCloud)) ? &bigdepthFrame : nullptr,
                       colorDepthMap.data());
            regOK = true;
        }
        if (s.depth && (s.depthFormat == depthFormatEnum::Raw || regOK)) {
            made[1] = makeDepth(*depth, s);
        }
        if (s.pointCloud && regOK) {
            made[2] = makePointCloud(s);
        }
        if ((s.regColor || s.uv) && regOK) {
            makeRegisteredColor(s, made[4], made[5]);
        }
    }
    if (s.ir && irOK) {
        made[3] = makeIR(*ir);
    }
    listener->release(frames);

    std::lock_guard<std::mutex> lock(mutex);
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        if (made[i].buf) {
            images[i] = std::move(made[i]); // replaces one the cook hasn't taken yet
        }
    }
    if (depthOK) {
        ++depthSeq;
    }
}

void MyFreenect2Device::runWorker() {
    LOG("[FreenectV2.cpp] runWorker(): thread started");
    while (!stopWorker.load()) {
        processFrames();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    LOG("[FreenectV2.cpp] runWorker(): thread exiting");
}

// An empty output buffer; its buf stays empty if TouchDesigner couldn't make one
V2Image MyFreenect2Device::newImage(int width, int height, TD::OP_PixelFormat format, size_t bytesPerPixel) {
    V2Image img;
    const uint64_t bytes = static_cast<uint64_t>(width) * height * bytesPerPixel;
    if (context && width > 0 && height > 0) {
        img.buf = context->createOutputBuffer(bytes, TD::TOP_BufferFlags::None, nullptr);
    }
    if (!img.buf) {
        LOG("[FreenectV2.cpp] newImage(): couldn't create a " + std::to_string(width) + "x" + std::to_string(height) + " buffer");
        return img;
    }
    img.width = width;
    img.height = height;
    img.format = format;
    return img;
}

// RGB output: RGBA8 at the RGB output size, mirrored like every other output
V2Image MyFreenect2Device::makeColor(const libfreenect2::Frame& rgb, const V2Settings& s) {
    V2Image img = newImage(s.colorW, s.colorH, TD::OP_PixelFormat::RGBA8Fixed, 4);
    if (!img.buf) return img;

    vImage_Buffer src = {
        .data = rgb.data,
        .height = (vImagePixelCount)RGB_HEIGHT,
        .width  = (vImagePixelCount)RGB_WIDTH,
        .rowBytes = static_cast<size_t>(RGB_WIDTH * 4)
    };
    vImage_Buffer dst = {
        .data = img.buf->data,
        .height = (vImagePixelCount)s.colorH,
        .width  = (vImagePixelCount)s.colorW,
        .rowBytes = static_cast<size_t>(s.colorW * 4)
    };
    if (s.colorW == RGB_WIDTH && s.colorH == RGB_HEIGHT) {
        vImageHorizontalReflect_ARGB8888(&src, &dst, kvImageNoFlags);
    } else {
        rgbScratch.resize(static_cast<size_t>(RGB_WIDTH) * RGB_HEIGHT * 4);
        vImage_Buffer flipped = src;
        flipped.data = rgbScratch.data();
        vImageHorizontalReflect_ARGB8888(&src, &flipped, kvImageNoFlags);
        vImageScale_ARGB8888(&flipped, &dst, nullptr, kvImageHighQualityResampling);
    }

    // libfreenect2 delivers BGRX
    const uint8_t permuteMap[4] = {2, 1, 0, 3};
    vImagePermuteChannels_ARGB8888(&dst, &dst, permuteMap, kvImageNoFlags);
    return img;
}

// Depth output, packed for Depth Output. 512x424 (Raw/RawUndistorted) or the RGB output size (Registered).
V2Image MyFreenect2Device::makeDepth(const libfreenect2::Frame& depth, const V2Settings& s) {
    const float* srcData = nullptr;
    int srcWidth = DEPTH_WIDTH, srcHeight = DEPTH_HEIGHT;
    switch (s.depthFormat) {
        case depthFormatEnum::Raw:
            srcData = reinterpret_cast<const float*>(depth.data);
            break;
        case depthFormatEnum::RawUndistorted:
            srcData = reinterpret_cast<const float*>(undistortedFrame.data);
            break;
        case depthFormatEnum::Registered:
            // bigdepth is 1920x1082 with one padding row on top and bottom
            srcData = reinterpret_cast<const float*>(bigdepthFrame.data) + BIGDEPTH_WIDTH;
            srcWidth = BIGDEPTH_WIDTH;
            srcHeight = BIGDEPTH_HEIGHT - 2;
            break;
    }
    const bool registered = (s.depthFormat == depthFormatEnum::Registered);
    const int dstWidth = registered ? s.colorW : DEPTH_WIDTH;
    const int dstHeight = registered ? s.colorH : DEPTH_HEIGHT;
    const size_t pixelCount = static_cast<size_t>(dstWidth) * dstHeight;

    // Millimetres, 0 = invalid or outside [threshMin, threshMax]
    depthScratch.resize(pixelCount);
    resampleNearest(srcData, srcWidth, srcHeight, 1, depthScratch.data(), dstWidth, dstHeight, /*flipX=*/true);
    for (float& d : depthScratch) {
        if (!isDepthInRange(d, s.threshMin, s.threshMax)) {
            d = 0.0f;
        }
    }

    const bool packed16 = (s.depthOutput == depthOutputEnum::Normalized);
    V2Image img = newImage(dstWidth, dstHeight, packed16 ? TD::OP_PixelFormat::Mono16Fixed : TD::OP_PixelFormat::Mono32Float,
                           packed16 ? sizeof(uint16_t) : sizeof(float));
    if (img.buf) {
        packDepth(depthScratch.data(), pixelCount, img.buf->data, s.depthOutput, s.threshMin, s.threshMax, s.unknownDepth);
    }
    return img;
}

// Point cloud output: RGBA32F, XYZ in metres, A = 1 for valid points and 0 for invalid ones.
//  Raw / Raw undistorted (depth camera): 512x424 grid, XYZ relative to the depth camera (libfreenect2 getPointXYZ).
//  Registered (color camera): RGB output grid, XYZ relative to the color camera, pixel-aligned with
//               the RGB output and the Registered depth map so the RGB image can be
//               applied as a texture with plain (u,v) = pixel position.
// flipX/flipY/flipZ negate the corresponding axis (e.g. flipZ makes +Z point toward the viewer, TouchDesigner style);
// invalid points get the Unknown Point Value as XYZ.
V2Image MyFreenect2Device::makePointCloud(const V2Settings& s) {
    const float unknownX = s.unknownPoint[0];
    const float unknownY = s.unknownPoint[1];
    const float unknownZ = s.unknownPoint[2];
    const float threshMin = s.threshMin, threshMax = s.threshMax;
    const bool colorSpace = (s.depthFormat == depthFormatEnum::Registered);
    const int dstWidth = colorSpace ? s.colorW : DEPTH_WIDTH;
    const int dstHeight = colorSpace ? s.colorH : DEPTH_HEIGHT;
    const int srcWidth  = colorSpace ? BIGDEPTH_WIDTH : DEPTH_WIDTH;
    const int srcHeight = colorSpace ? (BIGDEPTH_HEIGHT - 2) : DEPTH_HEIGHT;
    const float signX = s.flipX ? -1.0f : 1.0f;
    const float signY = s.flipY ? -1.0f : 1.0f;
    const float signZ = s.flipZ ? -1.0f : 1.0f;

    if (pcScratch.size() != static_cast<size_t>(srcWidth) * srcHeight * 4)
        pcScratch.resize(static_cast<size_t>(srcWidth) * srcHeight * 4);
    float* points = pcScratch.data();

    if (colorSpace) {
        const auto& colorParams = colorCameraParams; // captured when the Registration object was created
        const float fxInv = 1.0f / colorParams.fx;
        const float fyInv = 1.0f / colorParams.fy;
        const float* bigdepthRows = reinterpret_cast<const float*>(bigdepthFrame.data) + BIGDEPTH_WIDTH; // skip padding row
        // Rows are independent, so spread them across cores (dispatch_apply returns when all are done)
        dispatch_apply(srcHeight, DISPATCH_APPLY_AUTO, ^(size_t r) {
            const float* row = bigdepthRows + r * BIGDEPTH_WIDTH;
            float* dstRow = points + r * srcWidth * 4;
            const float yNorm = -(r + 0.5f - colorParams.cy) * fyInv; // negate: +Y up (matches depth-camera path)
            for (int c = 0; c < srcWidth; ++c) {
                const float depth = row[c];
                float* point = dstRow + c * 4;
                if (isDepthInRange(depth, threshMin, threshMax)) {
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
        });
    } else {
        libfreenect2::Registration* registration = reg.get();
        libfreenect2::Frame* undistorted = &undistortedFrame;
        dispatch_apply(srcHeight, DISPATCH_APPLY_AUTO, ^(size_t r) {
            for (int c = 0; c < srcWidth; ++c) {
                float x, y, z;
                registration->getPointXYZ(undistorted, static_cast<int>(r), c, x, y, z);
                float* point = points + (r * srcWidth + c) * 4;
                const float zMillimeters = z * 1000.0f;
                if (isDepthInRange(zMillimeters, threshMin, threshMax)) {
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
        });
    }

    V2Image img = newImage(dstWidth, dstHeight, TD::OP_PixelFormat::RGBA32Float, 4 * sizeof(float));
    if (img.buf) {
        resampleNearest(points, srcWidth, srcHeight, 4, static_cast<float*>(img.buf->data), dstWidth, dstHeight, /*flipX=*/true);
    }
    return img;
}

// Registered color + depth-to-color UV map (512x424, aligned with the depth-camera point cloud); each only if enabled
// color: RGBA8, the RGB image re-sampled onto the depth grid (A = 255 where a color pixel exists, 0 otherwise)
// uv:    RGBA32F, (u, v, 0, valid) giving where each depth pixel lands in the RGB output, in
//        TouchDesigner UV convention (0..1, origin bottom-left, already mirrored to match the flipped RGB output).
//        Feed it to a Remap TOP together with the RGB output to get registered color at full resolution.
void MyFreenect2Device::makeRegisteredColor(const V2Settings& s, V2Image& colorImg, V2Image& uvImg) {
    const int width = DEPTH_WIDTH, height = DEPTH_HEIGHT;
    if (s.regColor) colorImg = newImage(width, height, TD::OP_PixelFormat::RGBA8Fixed, 4);
    if (s.uv) uvImg = newImage(width, height, TD::OP_PixelFormat::RGBA32Float, 4 * sizeof(float));
    uint8_t* color = colorImg.buf ? static_cast<uint8_t*>(colorImg.buf->data) : nullptr;
    float* uv = uvImg.buf ? static_cast<float*>(uvImg.buf->data) : nullptr;

    const uint8_t* registeredData = registeredFrame.data; // BGRX
    const float invRgbWidth = 1.0f / RGB_WIDTH;
    const float invRgbHeight = 1.0f / RGB_HEIGHT;

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            const size_t srcIndex = static_cast<size_t>(r) * width + c;
            const size_t dstIndex = static_cast<size_t>(r) * width + (width - 1 - c); // mirror to match other outputs
            const int colorIndex = colorDepthMap[srcIndex];
            const bool valid = colorIndex >= 0;

            if (color) {
                uint8_t* colorPixel = color + dstIndex * 4;
                colorPixel[0] = registeredData[srcIndex * 4 + 2];
                colorPixel[1] = registeredData[srcIndex * 4 + 1];
                colorPixel[2] = registeredData[srcIndex * 4 + 0];
                colorPixel[3] = valid ? 255 : 0;
            }
            if (uv) {
                float* uvPixel = uv + dstIndex * 4;
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
    }
}

// IR output: 16-bit, mirrored like every other output
V2Image MyFreenect2Device::makeIR(const libfreenect2::Frame& ir) {
    V2Image img = newImage(IR_WIDTH, IR_HEIGHT, TD::OP_PixelFormat::Mono16Fixed, sizeof(uint16_t));
    if (!img.buf) return img;
    const float* src = reinterpret_cast<const float*>(ir.data);
    uint16_t* dst = static_cast<uint16_t*>(img.buf->data);
    for (int r = 0; r < IR_HEIGHT; ++r) {
        for (int c = 0; c < IR_WIDTH; ++c) {
            float d = src[r * IR_WIDTH + (IR_WIDTH - 1 - c)];
            if (!std::isfinite(d) || d <= 0.f) d = 0.f;
            dst[r * IR_WIDTH + c] = static_cast<uint16_t>(std::min(d, 65535.f));
        }
    }
    return img;
}
