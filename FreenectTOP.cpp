// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include "libfreenect.hpp"
#include "libfreenect2.hpp"
#include <sys/time.h>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <thread>
#include <string>

#ifndef DLLEXPORT
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#ifndef FREENECTTOP_VERSION
#define FREENECTTOP_VERSION "dev"
#endif

static constexpr int WIDTH = 640;
static constexpr int HEIGHT = 480;

// MyFreenectDevice ---------------------------------------------------------
MyFreenectDevice::MyFreenectDevice(freenect_context* ctx, int index,
                                   std::atomic<bool>& rgbFlag,
                                   std::atomic<bool>& depthFlag)
  : FreenectDevice(ctx, index)
  , rgbReady(rgbFlag)
  , depthReady(depthFlag)
  , rgbBuffer(WIDTH*HEIGHT*3)
  , depthBuffer(WIDTH*HEIGHT)
  , hasNewRGB(false)
  , hasNewDepth(false)
{
    setVideoFormat(FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_11BIT);
}

void MyFreenectDevice::VideoCallback(void* rgb, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!rgb) return;
    auto ptr = static_cast<uint8_t*>(rgb);
    std::copy(ptr, ptr + rgbBuffer.size(), rgbBuffer.begin());
    hasNewRGB = true;
    rgbReady = true;
}

void MyFreenectDevice::DepthCallback(void* depth, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!depth) return;
    auto ptr = static_cast<uint16_t*>(depth);
    std::copy(ptr, ptr + depthBuffer.size(), depthBuffer.begin());
    hasNewDepth = true;
    depthReady = true;
}

bool MyFreenectDevice::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    out = rgbBuffer;
    hasNewRGB = false;
    return true;
}

bool MyFreenectDevice::getDepth(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    out = depthBuffer;
    hasNewDepth = false;
    return true;
}

// TouchDesigner Entrypoints ------------------------------------------------
extern "C" {
DLLEXPORT void FillTOPPluginInfo(TOP_PluginInfo* info) {
    info->apiVersion       = TOPCPlusPlusAPIVersion;
    info->executeMode      = TOP_ExecuteMode::CPUMem;
    info->customOPInfo.opType->setString("Freenecttop");
    info->customOPInfo.opLabel->setString("FreenectTOP");
    info->customOPInfo.opIcon->setString("KNT");
    info->customOPInfo.authorName->setString("Matteo (Marte) Tagliabue");
    info->customOPInfo.authorEmail->setString("");
    info->customOPInfo.minInputs = 0;
    info->customOPInfo.maxInputs = 0;
}

DLLEXPORT TOP_CPlusPlusBase* CreateTOPInstance(const OP_NodeInfo* info,
                                               TOP_Context*       context) {
    return new FreenectTOP(info, context);
}

DLLEXPORT void DestroyTOPInstance(TOP_CPlusPlusBase* instance,
                                  TOP_Context*       context) {
    delete (FreenectTOP*)instance;
}
} // extern "C"

// FreenectTOP Implementation -----------------------------------------------

void FreenectTOP::cleanupDevice() {
    runEvents = false;
    if (eventThread.joinable()) {
        eventThread.join();
    }
    cleanupDeviceV2(); // Clean up Kinect v2 if needed
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
    if (fn2_ctx) return true; // Already initialized
    fn2_ctx = new libfreenect2::Freenect2();
    if (fn2_ctx->enumerateDevices() == 0) {
        fprintf(stderr, "[FreenectTOP] No Kinect v2 devices found\n");
        delete fn2_ctx; fn2_ctx = nullptr;
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    fn2_pipeline = new libfreenect2::CpuPacketPipeline();
    fn2_dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    if (!fn2_dev) {
        fprintf(stderr, "[FreenectTOP] Failed to open Kinect v2 device\n");
        delete fn2_ctx; fn2_ctx = nullptr;
        delete fn2_pipeline; fn2_pipeline = nullptr;
        return false;
    }
    int types = libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth;
    fn2_listener = new libfreenect2::SyncMultiFrameListener(types);
    fn2_dev->setColorFrameListener(fn2_listener);
    fn2_dev->setIrAndDepthFrameListener(fn2_listener);
    if (!fn2_dev->start()) {
        fprintf(stderr, "[FreenectTOP] Failed to start Kinect v2 streams\n");
        fn2_dev->close();
        delete fn2_dev; fn2_dev = nullptr;
        delete fn2_ctx; fn2_ctx = nullptr;
        delete fn2_pipeline; fn2_pipeline = nullptr;
        delete fn2_listener; fn2_listener = nullptr;
        return false;
    }
    fn2_started = true;
    fn2_lastRGB.resize(1920 * 1080 * 4, 0); // RGBA
    fn2_lastDepth.resize(512 * 424, 0.0f);
    return true;
}

void FreenectTOP::cleanupDeviceV2() {
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
    fn2_lastRGB.clear();
    fn2_lastDepth.clear();
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
    : myNodeInfo(info), myContext(context), freenectContext(nullptr), device(nullptr),
      firstRGBReady(false), firstDepthReady(false),
      lastRGB(WIDTH*HEIGHT*3, 0), lastDepth(WIDTH*HEIGHT, 0) {
    initDevice();
}

FreenectTOP::~FreenectTOP() {
    cleanupDevice();
}

void FreenectTOP::setupParameters(OP_ParameterManager* manager, void*) {
    // Show version in header
    OP_StringParameter versionHeader;
    versionHeader.name  = "Version";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION;
    versionHeader.label = versionLabel.c_str();
    manager->appendHeader(versionHeader);
    
    // Tilt angle parameter
    OP_NumericParameter np;
    np.name            = "Tilt";
    np.label           = "Tilt Angle";
    np.defaultValues[0] = 0.0;
    np.minValues[0]     = -30.0;
    np.maxValues[0]     = 30.0;
    np.minSliders[0]    = -30.0;
    np.maxSliders[0]    = 30.0;
    manager->appendFloat(np);
    
    // Invert depth map toggle
    OP_NumericParameter invertParam;
    invertParam.name            = "Invertdepth";
    invertParam.label           = "Invert Depth Map";
    invertParam.defaultValues[0] = 0.0;
    invertParam.minValues[0]     = 0.0;
    invertParam.maxValues[0]     = 1.0;
    invertParam.minSliders[0]    = 0.0;
    invertParam.maxSliders[0]    = 1.0;
    invertParam.clampMins[0]     = true;
    invertParam.clampMaxes[0]    = true;
    manager->appendToggle(invertParam);
    
    // Device type dropdown
    OP_StringParameter deviceTypeParam;
    deviceTypeParam.name = "Devicetype";
    deviceTypeParam.label = "Device Type";
    deviceTypeParam.defaultValue = "Kinect v1";
    const char* deviceTypeNames[] = {"Kinect v1", "Kinect v2"};
    const char* deviceTypeLabels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
    manager->appendMenu(deviceTypeParam, 2, deviceTypeNames, deviceTypeLabels);
}

void FreenectTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

void FreenectTOP::execute(TOP_Output* output, const OP_Inputs* inputs, void*) {
    // Defensive: check for null pointers and valid parameter
    if (!inputs) {
        fprintf(stderr, "[FreenectTOP] ERROR: inputs is null!\n");
        return;
    }
    const char* devTypeCStr = inputs->getParString("Devicetype");
    std::string deviceTypeStr = devTypeCStr ? devTypeCStr : "Kinect v1";
    int newDeviceType = (deviceTypeStr == "Kinect v2") ? 1 : 0;
    deviceType = newDeviceType;

    // If device type changed, re-init device
    if (deviceTypeStr != lastDeviceTypeStr) {
        cleanupDevice();
        lastDeviceTypeStr = deviceTypeStr;
    }

    if (deviceType == 0) {
        // Kinect v1 (libfreenect)
        bool deviceValid = (device != nullptr);
        if (!device) {
            cleanupDevice();
            if (!initDevice()) {
                // Display a red screen with a thick white X (cross), as in the example
                OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr) : nullptr;
                if (buf) {
                    uint8_t* out = static_cast<uint8_t*>(buf->data);
                    // Fill the entire image with solid red
                    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
                        out[i * 4 + 0] = 255; // red
                        out[i * 4 + 1] = 0;
                        out[i * 4 + 2] = 0;
                        out[i * 4 + 3] = 255;
                    }
                    // Draw a thick centred 'X' (45° lines)
                    const int thickness = 31;
                    const float slope   = static_cast<float>(HEIGHT) / WIDTH;
                    const float halfT   = thickness * 0.5f;
                    for (int y = 0; y < HEIGHT; ++y) {
                        for (int x = 0; x < WIDTH; ++x) {
                            float d1 = std::fabs(y - slope * x);
                            float d2 = std::fabs(y - ((HEIGHT - 1) - slope * x));
                            if (d1 <= halfT || d2 <= halfT) {
                                int idx = y * WIDTH + x;
                                out[idx * 4 + 0] = 255;
                                out[idx * 4 + 1] = 255;
                                out[idx * 4 + 2] = 255;
                                out[idx * 4 + 3] = 255;
                            }
                        }
                    }
                    TOP_UploadInfo info;
                    info.textureDesc.width       = WIDTH;
                    info.textureDesc.height      = HEIGHT;
                    info.textureDesc.texDim      = OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex        = 0;
                    output->uploadBuffer(&buf, info, nullptr);
                }
                // Depth buffer: fill with black and draw a white band at the top
                OP_SmartRef<TOP_Buffer> depthBuf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 2, TOP_BufferFlags::None, nullptr) : nullptr;
                if (depthBuf) {
                    uint16_t* depthOut = static_cast<uint16_t*>(depthBuf->data);
                    for (int i = 0; i < WIDTH * HEIGHT; ++i)
                        depthOut[i] = 0;
                    for (int x = 0; x < WIDTH; ++x) {
                        int idx = (HEIGHT - 1 - 20) * WIDTH + x;
                        depthOut[idx] = 65535;
                    }
                    TOP_UploadInfo depthInfo;
                    depthInfo.textureDesc.width       = WIDTH;
                    depthInfo.textureDesc.height      = HEIGHT;
                    depthInfo.textureDesc.texDim      = OP_TexDim::e2D;
                    depthInfo.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                    depthInfo.colorBufferIndex        = 1;
                    output->uploadBuffer(&depthBuf, depthInfo, nullptr);
                }
                return;
            }
            deviceValid = true;
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
        std::vector<uint8_t> rgb;
        if (device->getRGB(rgb)) {
            lastRGB = rgb;
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint8_t* out = static_cast<uint8_t*>(buf->data);
                for (int y = 0; y < HEIGHT; ++y)
                    for (int x = 0; x < WIDTH; ++x) {
                        int srcY = HEIGHT - 1 - y;
                        int iSrc = (srcY * WIDTH + x) * 3;
                        int iDst = (y * WIDTH + x) * 4;
                        out[iDst + 0] = lastRGB[iSrc + 0];
                        out[iDst + 1] = lastRGB[iSrc + 1];
                        out[iDst + 2] = lastRGB[iSrc + 2];
                        out[iDst + 3] = 255;
                    }
                TOP_UploadInfo info;
                info.textureDesc.width       = WIDTH;
                info.textureDesc.height      = HEIGHT;
                info.textureDesc.texDim      = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                info.colorBufferIndex        = 0;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
        std::vector<uint16_t> depth;
        if (device->getDepth(depth)) {
            lastDepth = depth;
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 2, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint16_t* out = static_cast<uint16_t*>(buf->data);
                for (int y = 0; y < HEIGHT; ++y)
                    for (int x = 0; x < WIDTH; ++x) {
                        int idx = (HEIGHT - 1 - y) * WIDTH + x;
                        uint16_t raw = lastDepth[idx];
                        bool invert = inputs->getParInt("Invertdepth") != 0;
                        if (raw > 0 && raw < 2047) {
                            uint16_t scaled = raw << 5;
                            out[y * WIDTH + x] = invert ? 65535 - scaled : scaled;
                        } else {
                            out[y * WIDTH + x] = 0;
                        }
                    }
                TOP_UploadInfo info;
                info.textureDesc.width       = WIDTH;
                info.textureDesc.height      = HEIGHT;
                info.textureDesc.texDim      = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex        = 1;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
    } else {
        // Kinect v2 (libfreenect2)
        bool deviceValid = (fn2_ctx && fn2_dev && fn2_started);
        if (!deviceValid) {
            cleanupDeviceV2();
            if (!initDeviceV2()) {
                // Show blue screen if v2 not available
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
                    info.textureDesc.width       = WIDTH;
                    info.textureDesc.height      = HEIGHT;
                    info.textureDesc.texDim      = OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex        = 0;
                    output->uploadBuffer(&buf, info, nullptr);
                }
                return;
            }
        }
        // Wait for new frame
        libfreenect2::FrameMap frames;
        if (!fn2_listener->waitForNewFrame(frames, 1000)) {
            fprintf(stderr, "[FreenectTOP] Kinect v2: timeout waiting for frame\n");
            return;
        }
        libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
        libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
        // Color: convert 1920x1080 BGRA to 640x480 RGBA (simple nearest)
        if (rgb && rgb->data) {
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint8_t* out = static_cast<uint8_t*>(buf->data);
                int srcW = 1920, srcH = 1080;
                for (int y = 0; y < HEIGHT; ++y) {
                    int srcY = y * srcH / HEIGHT;
                    for (int x = 0; x < WIDTH; ++x) {
                        int srcX = x * srcW / WIDTH;
                        int srcIdx = (srcY * srcW + srcX) * 4;
                        int dstIdx = (y * WIDTH + x) * 4;
                        out[dstIdx + 0] = rgb->data[srcIdx + 2]; // R
                        out[dstIdx + 1] = rgb->data[srcIdx + 1]; // G
                        out[dstIdx + 2] = rgb->data[srcIdx + 0]; // B
                        out[dstIdx + 3] = 255;
                    }
                }
                TOP_UploadInfo info;
                info.textureDesc.width       = WIDTH;
                info.textureDesc.height      = HEIGHT;
                info.textureDesc.texDim      = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
                info.colorBufferIndex        = 0;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
        // Depth: 512x424 float -> 640x480 uint16 (simple nearest, scale to 0-65535 for 0-4.5m)
        if (depth && depth->data) {
            OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(WIDTH * HEIGHT * 2, TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                uint16_t* out = static_cast<uint16_t*>(buf->data);
                int srcW = 512, srcH = 424;
                bool invert = inputs->getParInt("Invertdepth") != 0;
                for (int y = 0; y < HEIGHT; ++y) {
                    int srcY = y * srcH / HEIGHT;
                    for (int x = 0; x < WIDTH; ++x) {
                        int srcX = x * srcW / WIDTH;
                        int srcIdx = srcY * srcW + srcX;
                        float d = reinterpret_cast<float*>(depth->data)[srcIdx];
                        uint16_t val = 0;
                        if (d > 0.1f && d < 4.5f) {
                            val = static_cast<uint16_t>(d / 4.5f * 65535.0f);
                            if (invert) val = 65535 - val;
                        }
                        out[y * WIDTH + x] = val;
                    }
                }
                TOP_UploadInfo info;
                info.textureDesc.width       = WIDTH;
                info.textureDesc.height      = HEIGHT;
                info.textureDesc.texDim      = OP_TexDim::e2D;
                info.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex        = 1;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
        fn2_listener->release(frames);
    }
}

void FreenectTOP::pulsePressed(const char*, void*) {}
