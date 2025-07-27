// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include "FreenectV1.h"
#include "FreenectV2.h"
#include "libfreenect.hpp"
#include <sys/time.h>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <thread>
#include <string>
#include <cstring>
#include <vector>
#include <libfreenect2/registration.h>

#ifndef DLLEXPORT
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#ifndef FREENECTTOP_VERSION
#define FREENECTTOP_VERSION "dev"
#endif

#define WIDTH 1920
#define HEIGHT 1080

// TouchDesigner Entrypoints ------------------------------------------------
extern "C" {
DLLEXPORT void FillTOPPluginInfo(TOP_PluginInfo* info) {
    info->apiVersion = TOPCPlusPlusAPIVersion;
    info->executeMode = TOP_ExecuteMode::CPUMem;
    info->customOPInfo.opType->setString("Freenecttop");
    info->customOPInfo.opLabel->setString("FreenectTOP");
    info->customOPInfo.opIcon->setString("KNT");
    info->customOPInfo.authorName->setString("Matteo (Marte) Tagliabue");
    info->customOPInfo.authorEmail->setString("");
    info->customOPInfo.minInputs = 0;
    info->customOPInfo.maxInputs = 0;
}

DLLEXPORT TOP_CPlusPlusBase* CreateTOPInstance(const OP_NodeInfo* info,
                                               TOP_Context* context) {
    return new FreenectTOP(info, context);
}

DLLEXPORT void DestroyTOPInstance(TOP_CPlusPlusBase* instance,
                                  TOP_Context* context) {
    delete static_cast<FreenectTOP*>(instance);
}
} // extern "C"

// FreenectTOP Implementation -----------------------------------------------

void FreenectTOP::cleanupDevice() {
    runEvents = false;
    if (eventThread.joinable()) {
        eventThread.join();
    }
    // Remove Kinect v2 cleanup
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (device) {
        device->stopVideo();
        device->stopDepth();
        delete device;
        device = nullptr;
    }
    if (freenectContext) {
        freenect_shutdown(freenectContext);
        freenectContext = nullptr;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// --- Kinect v2 (libfreenect2) ---
bool FreenectTOP::initDeviceV2() {
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (fn2_ctx) return true;
    fn2_ctx = new libfreenect2::Freenect2();
    if (fn2_ctx->enumerateDevices() == 0) {
        fprintf(stderr, "[FreenectTOP] No Kinect v2 devices found\n");
        delete fn2_ctx;
        fn2_ctx = nullptr;
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::OpenCLPacketPipeline();
        fprintf(stderr, "[FreenectTOP] Using OpenCLPacketPipeline for Kinect v2\n");
    } catch (...) {
        fn2_pipeline = nullptr;
    }
    if (!fn2_pipeline) {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
        fprintf(stderr, "[FreenectTOP] Falling back to CpuPacketPipeline for Kinect v2\n");
    }
    fn2_dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    if (!fn2_dev) {
        fprintf(stderr, "[FreenectTOP] Failed to open Kinect v2 device\n");
        delete fn2_ctx;
        fn2_ctx = nullptr;
        delete fn2_pipeline;
        fn2_pipeline = nullptr;
        return false;
    }
    int types = libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth;
    fn2_listener = new libfreenect2::SyncMultiFrameListener(types);
    fn2_dev->setColorFrameListener(fn2_listener);
    fn2_dev->setIrAndDepthFrameListener(fn2_listener);
    if (!fn2_dev->start()) {
        fprintf(stderr, "[FreenectTOP] Failed to start Kinect v2 streams\n");
        fn2_dev->close();
        delete fn2_dev;
        fn2_dev = nullptr;
        delete fn2_ctx;
        fn2_ctx = nullptr;
        delete fn2_pipeline;
        fn2_pipeline = nullptr;
        delete fn2_listener;
        fn2_listener = nullptr;
        return false;
    }
    fn2_started = true;
    fn2_rgbBuffer.resize(WIDTH * HEIGHT * 4, 0);
    fn2_depthBuffer.resize(512 * 424, 0.0f);
    // Start background acquisition thread
    fn2_acquireRunning = true;
    fn2_acquireThread = std::thread([this]() {
        while (fn2_acquireRunning) {
            libfreenect2::FrameMap frames;
            if (fn2_listener && fn2_listener->waitForNewFrame(frames, 100)) {
                libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
                libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
                {
                    std::lock_guard<std::mutex> lock(fn2_acquireMutex);
                    if (rgb && rgb->data) {
                        memcpy(fn2_rgbBuffer.data(), rgb->data, WIDTH * HEIGHT * 4);
                    }
                    if (depth && depth->data) {
                        const float* src = reinterpret_cast<const float*>(depth->data);
                        std::copy(src, src + 512 * 424, fn2_depthBuffer.begin());
                    }
                }
                fn2_listener->release(frames);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    return true;
}

void FreenectTOP::cleanupDeviceV2() {
    fn2_acquireRunning = false;
    if (fn2_acquireThread.joinable()) fn2_acquireThread.join();
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (fn2_dev) {
        if (fn2_started) fn2_dev->stop();
        fn2_dev->close();
        delete fn2_dev;
        fn2_dev = nullptr;
    }
    if (fn2_listener) {
        delete fn2_listener;
        fn2_listener = nullptr;
    }
    if (fn2_pipeline) {
        delete fn2_pipeline;
        fn2_pipeline = nullptr;
    }
    if (fn2_ctx) {
        delete fn2_ctx;
        fn2_ctx = nullptr;
    }
    fn2_started = false;
    fn2_rgbBuffer.clear();
    fn2_depthBuffer.clear();
}

bool FreenectTOP::initDevice() {
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&freenectContext, nullptr) < 0) {
        fprintf(stderr, "[FreenectTOP] freenect_init failed\n");
        return false;
    }
    freenect_set_log_level(freenectContext, FREENECT_LOG_WARNING);

    int numDevices = freenect_num_devices(freenectContext);
    if (numDevices <= 0) {
        fprintf(stderr, "[FreenectTOP] No devices found\n");
        freenect_shutdown(freenectContext);
        freenectContext = nullptr;
        return false;
    }

    try {
        firstRGBReady = false;
        firstDepthReady = false;

        device = new MyFreenectDevice(freenectContext, 0, firstRGBReady, firstDepthReady);
        device->startVideo();
        device->startDepth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        runEvents = true;
        eventThread = std::thread([this]() {
            while (runEvents.load()) {
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (!freenectContext) break;
                int err = freenect_process_events(freenectContext);
                if (err < 0) {
                    fprintf(stderr, "[FreenectTOP] Error in freenect_process_events (%d)\n", err);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    } catch (...) {
        fprintf(stderr, "[FreenectTOP] Failed to start device\n");
        cleanupDevice();
        return false;
    }
    return true;
}

FreenectTOP::FreenectTOP(const OP_NodeInfo* info, TOP_Context* context)
    : myNodeInfo(info),
      myContext(context),
      freenectContext(nullptr),
      device(nullptr),
      firstRGBReady(false),
      firstDepthReady(false),
      lastRGB(640 * 480 * 3, 0),
      lastDepth(640 * 480, 0) {
    initDevice();
}

FreenectTOP::~FreenectTOP() {
    cleanupDevice();
}

void FreenectTOP::setupParameters(OP_ParameterManager* manager, void*) {
    // Show version in header
    OP_StringParameter versionHeader;
    versionHeader.name = "Version";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION;
    versionHeader.label = versionLabel.c_str();
    manager->appendHeader(versionHeader);

    // Tilt angle parameter
    OP_NumericParameter np;
    np.name = "Tilt";
    np.label = "Tilt Angle";
    np.defaultValues[0] = 0.0;
    np.minValues[0] = -30.0;
    np.maxValues[0] = 30.0;
    np.minSliders[0] = -30.0;
    np.maxSliders[0] = 30.0;
    manager->appendFloat(np);

    // Invert depth map toggle
    OP_NumericParameter invertParam;
    invertParam.name = "Invertdepth";
    invertParam.label = "Invert Depth Map";
    invertParam.defaultValues[0] = 0.0;
    invertParam.minValues[0] = 0.0;
    invertParam.maxValues[0] = 1.0;
    invertParam.minSliders[0] = 0.0;
    invertParam.maxSliders[0] = 1.0;
    invertParam.clampMins[0] = true;
    invertParam.clampMaxes[0] = true;
    manager->appendToggle(invertParam);

    // Device type dropdown
    OP_StringParameter deviceTypeParam;
    deviceTypeParam.name = "Devicetype";
    deviceTypeParam.label = "Device Type";
    deviceTypeParam.defaultValue = "Kinect v1";
    const char* deviceTypeNames[] = {"Kinect v1", "Kinect v2"};
    const char* deviceTypeLabels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
    manager->appendMenu(deviceTypeParam, 2, deviceTypeNames, deviceTypeLabels);
    
    // Limit resolution to 1280x720 for Kinect V2 (toggle for non-commercial license)
    OP_NumericParameter resLimitParam;
    resLimitParam.name = "Resolutionlimit";
    resLimitParam.label = "Resolution Limit";
    resLimitParam.defaultValues[0] = 1.0; // Default to enabled
    resLimitParam.minValues[0] = 0.0;
    resLimitParam.maxValues[0] = 1.0;
    resLimitParam.minSliders[0] = 0.0;
    resLimitParam.maxSliders[0] = 1.0;
    resLimitParam.clampMins[0] = true;
    resLimitParam.clampMaxes[0] = true;
    manager->appendToggle(resLimitParam);

}

void FreenectTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

void FreenectTOP::execute(TOP_Output* output, const OP_Inputs* inputs, void*) {
    if (!inputs) {
        fprintf(stderr, "[FreenectTOP] ERROR: inputs is null!\n");
        return;
    }
    const char* devTypeCStr = inputs->getParString("Devicetype");
    std::string deviceTypeStr = devTypeCStr ? devTypeCStr : "Kinect v1";
    int newDeviceType = (deviceTypeStr == "Kinect v2") ? 1 : 0;
    deviceType = newDeviceType;

    if (deviceTypeStr != lastDeviceTypeStr) {
        cleanupDevice();
        lastDeviceTypeStr = deviceTypeStr;
    }

    if (deviceType == 0) {
        int colorWidth = 640, colorHeight = 480, depthWidth = 640, depthHeight = 480;
        if (!device) {
            cleanupDevice();
            if (!initDevice()) {
                OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(colorWidth * colorHeight * 4, TOP_BufferFlags::None, nullptr) : nullptr;
                if (buf) {
                    uint8_t* out = static_cast<uint8_t*>(buf->data);
                    for (int i = 0; i < colorWidth * colorHeight; ++i) {
                        out[i * 4 + 0] = 255;
                        out[i * 4 + 1] = 0;
                        out[i * 4 + 2] = 0;
                        out[i * 4 + 3] = 255;
                    }
                    const int thickness = 31;
                    const float slope = static_cast<float>(colorHeight) / colorWidth;
                    const float halfT = thickness * 0.5f;
                    for (int y = 0; y < colorHeight; ++y) {
                        for (int x = 0; x < colorWidth; ++x) {
                            float d1 = std::fabs(y - slope * x);
                            float d2 = std::fabs(y - ((colorHeight - 1) - slope * x));
                            if (d1 <= halfT || d2 <= halfT) {
                                int idx = y * colorWidth + x;
                                out[idx * 4 + 0] = 255;
                                out[idx * 4 + 1] = 255;
                                out[idx * 4 + 2] = 255;
                                out[idx * 4 + 3] = 255;
                            }
                        }
                    }
                    TOP_UploadInfo info;
                    info.textureDesc.width = colorWidth;
                    info.textureDesc.height = colorHeight;
                    info.textureDesc.texDim = OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex = 0;
                    output->uploadBuffer(&buf, info, nullptr);
                }
                OP_SmartRef<TOP_Buffer> depthBuf = myContext ? myContext->createOutputBuffer(depthWidth * depthHeight * 2, TOP_BufferFlags::None, nullptr) : nullptr;
                if (depthBuf) {
                    uint16_t* depthOut = static_cast<uint16_t*>(depthBuf->data);
                    for (int i = 0; i < depthWidth * depthHeight; ++i)
                        depthOut[i] = 0;
                    for (int x = 0; x < depthWidth; ++x) {
                        int idx = (depthHeight - 1 - 20) * depthWidth + x;
                        depthOut[idx] = 65535;
                    }
                    TOP_UploadInfo depthInfo;
                    depthInfo.textureDesc.width = depthWidth;
                    depthInfo.textureDesc.height = depthHeight;
                    depthInfo.textureDesc.texDim = OP_TexDim::e2D;
                    depthInfo.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                    depthInfo.colorBufferIndex = 1;
                    output->uploadBuffer(&depthBuf, depthInfo, nullptr);
                }
                return;
            }
        }
        if (!device) {
            fprintf(stderr, "[FreenectTOP] ERROR: device is null after init!\n");
            return;
        }
        float tilt = static_cast<float>(inputs->getParDouble("Tilt"));
        try {
            device->setTiltDegrees(tilt);
        } catch (const std::exception& e) {
            fprintf(stderr, "[FreenectTOP] Failed to set tilt: %s\n", e.what());
            cleanupDevice();
            device = nullptr;
            return;
        }
        bool flip = true; // always flip for v1
        bool invert = inputs->getParInt("Invertdepth") != 0;
        std::vector<uint8_t> colorFrame;
        if (device->getColorFrame(colorFrame, flip)) {
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(colorWidth * colorHeight * 4, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                std::memcpy(buf->data, colorFrame.data(), colorWidth * colorHeight * 4);
                TOP_UploadInfo info;
                info.textureDesc.width = colorWidth;
                info.textureDesc.height = colorHeight;
                info.textureDesc.texDim = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                info.colorBufferIndex = 0;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
        std::vector<uint16_t> depthFrame;
        if (device->getDepthFrame(depthFrame, invert, flip)) {
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(depthWidth * depthHeight * 2, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                std::memcpy(buf->data, depthFrame.data(), depthWidth * depthHeight * 2);
                TOP_UploadInfo info;
                info.textureDesc.width = depthWidth;
                info.textureDesc.height = depthHeight;
                info.textureDesc.texDim = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex = 1;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
    } else {
        // Kinect v2 (libfreenect2)
        bool v2InitOk = true;
        if (!fn2_dev) {
            v2InitOk = initDeviceV2();
            if (!v2InitOk) {
                fprintf(stderr, "[FreenectTOP] Kinect v2 init failed or no device found\n");
            }
        }
        if (!fn2_dev || !v2InitOk) {
            // Error screen placeholder
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint8_t* out = static_cast<uint8_t*>(buf->data);
                for (int i = 0; i < WIDTH * HEIGHT; ++i) {
                    out[i * 4 + 0] = 0;
                    out[i * 4 + 1] = 0;
                    out[i * 4 + 2] = 255;
                    out[i * 4 + 3] = 255;
                }
                TOP_UploadInfo info;
                info.textureDesc.width = WIDTH;
                info.textureDesc.height = HEIGHT;
                info.textureDesc.texDim = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                info.colorBufferIndex = 0;
                output->uploadBuffer(&buf, info, nullptr);
            }
            OP_SmartRef<TOP_Buffer> depthBuf = myContext ? myContext->createOutputBuffer(512 * 424 * 2, TOP_BufferFlags::None, nullptr) : nullptr;
            if (depthBuf) {
                uint16_t* depthOut = static_cast<uint16_t*>(depthBuf->data);
                for (int i = 0; i < 512 * 424; ++i)
                    depthOut[i] = 0;
                for (int x = 0; x < 512; ++x) {
                    int idx = (424 - 1 - 20) * 512 + x;
                    depthOut[idx] = 65535;
                }
                TOP_UploadInfo depthInfo;
                depthInfo.textureDesc.width = 512;
                depthInfo.textureDesc.height = 424;
                depthInfo.textureDesc.texDim = OP_TexDim::e2D;
                depthInfo.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                depthInfo.colorBufferIndex = 1;
                output->uploadBuffer(&depthBuf, depthInfo, nullptr);
            }
            return;
        }
        // Wait for new frame
        libfreenect2::FrameMap frames;
        if (!fn2_listener->waitForNewFrame(frames, 1000)) {
            fprintf(stderr, "[FreenectTOP] Kinect v2: timeout waiting for frame\n");
            return;
        }
        libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
        libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
        // Color: convert 1920x1080 BGRA to 1920x1080 RGBA
        if (rgb && rgb->data) {
            std::lock_guard<std::mutex> lock(freenectMutex);
            int srcW = WIDTH, srcH = HEIGHT;
            for (int y = 0; y < HEIGHT; ++y) {
                int srcY = y;
                for (int x = 0; x < WIDTH; ++x) {
                    int srcX = x;
                    int srcIdx = (srcY * srcW + srcX) * 4;
                    int dstIdx = (y * WIDTH + x) * 4;
                    fn2_rgbBuffer[dstIdx + 0] = rgb->data[srcIdx + 2]; // R
                    fn2_rgbBuffer[dstIdx + 1] = rgb->data[srcIdx + 1]; // G
                    fn2_rgbBuffer[dstIdx + 2] = rgb->data[srcIdx + 0]; // B
                    fn2_rgbBuffer[dstIdx + 3] = 255;
                }
            }
            if (inputs->getParInt("Resolutionlimit") != 0) {
                // Downscale Kinect v2 color stream to 1280x720
                int downscaleWidth = 1280;
                int downscaleHeight = 720;
                std::vector<uint8_t> downscaledBuffer(downscaleWidth * downscaleHeight * 4, 0);

                for (int y = 0; y < downscaleHeight; ++y) {
                    for (int x = 0; x < downscaleWidth; ++x) {
                        int srcX = x * WIDTH / downscaleWidth;
                        int srcY = y * HEIGHT / downscaleHeight;
                        int srcIdx = (srcY * WIDTH + srcX) * 4;
                        int dstIdx = (y * downscaleWidth + x) * 4;
                        downscaledBuffer[dstIdx + 0] = fn2_rgbBuffer[srcIdx + 0]; // R
                        downscaledBuffer[dstIdx + 1] = fn2_rgbBuffer[srcIdx + 1]; // G
                        downscaledBuffer[dstIdx + 2] = fn2_rgbBuffer[srcIdx + 2]; // B
                        downscaledBuffer[dstIdx + 3] = fn2_rgbBuffer[srcIdx + 3]; // A
                    }
                }

                OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(downscaleWidth * downscaleHeight * 4, TOP_BufferFlags::None, nullptr) : nullptr;
                if (buf) {
                    uint8_t* out = static_cast<uint8_t*>(buf->data);
                    std::memcpy(out, downscaledBuffer.data(), downscaleWidth * downscaleHeight * 4);
                    TOP_UploadInfo info;
                    info.textureDesc.width = downscaleWidth;
                    info.textureDesc.height = downscaleHeight;
                    info.textureDesc.texDim = OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex = 0;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            } else {
                // Original resolution
                OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr) : nullptr;
                if (buf) {
                    uint8_t* out = static_cast<uint8_t*>(buf->data);
                    std::memcpy(out, fn2_rgbBuffer.data(), WIDTH * HEIGHT * 4);
                    TOP_UploadInfo info;
                    info.textureDesc.width = WIDTH;
                    info.textureDesc.height = HEIGHT;
                    info.textureDesc.texDim = OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex = 0;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
        }
        // Depth: use undistorted depth from registration
        if (depth && depth->data) {
            std::lock_guard<std::mutex> lock(freenectMutex);
            int srcW = 512, srcH = 424;
            bool invert = inputs->getParInt("Invertdepth") != 0;
            for (int y = 0; y < srcH; ++y) {
                for (int x = 0; x < srcW; ++x) {
                    int srcIdx = y * srcW + x;
                    float d = reinterpret_cast<float*>(depth->data)[srcIdx];
                    uint16_t val = 0;
                    if (d > 0.1f && d < 4.5f) {
                        val = static_cast<uint16_t>(d / 4.5f * 65535.0f);
                        if (invert) val = 65535 - val;
                    }
                    fn2_depthBuffer[y * srcW + x] = static_cast<float>(val);
                }
            }
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(srcW * srcH * 2, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint16_t* out = static_cast<uint16_t*>(buf->data);
                for (int i = 0; i < srcW * srcH; ++i) {
                    out[i] = static_cast<uint16_t>(fn2_depthBuffer[i]);
                }
                TOP_UploadInfo info;
                info.textureDesc.width = srcW;
                info.textureDesc.height = srcH;
                info.textureDesc.texDim = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex = 1;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
        fn2_listener->release(frames);
    }
}

void FreenectTOP::pulsePressed(const char*, void*) {}
