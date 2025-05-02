// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include <libfreenect.hpp>
#include <sys/time.h>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <thread>

#ifndef DLLEXPORT
#define DLLEXPORT __attribute__((visibility("default")))
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
    OP_NumericParameter np;
    np.name            = "Tilt";
    np.label           = "Tilt Angle";
    np.defaultValues[0] = 0.0;
    np.minValues[0]     = -30.0;
    np.maxValues[0]     = 30.0;
    np.minSliders[0]    = -30.0;
    np.maxSliders[0]    = 30.0;
    manager->appendFloat(np);
    
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
}

void FreenectTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

void FreenectTOP::execute(TOP_Output* output, const OP_Inputs* inputs, void*) {
    bool deviceValid = (device != nullptr);
    if (!device) {
        cleanupDevice();
        if (!initDevice()) {
            // Display a red screen with an error message
            OP_SmartRef<TOP_Buffer> buf = myContext->createOutputBuffer(WIDTH * HEIGHT * 4, TOP_BufferFlags::None, nullptr);
            uint8_t* out = static_cast<uint8_t*>(buf->data);
            for (int i = 0; i < WIDTH * HEIGHT; ++i) {
                out[i * 4 + 0] = 255; // red
                out[i * 4 + 1] = 0;
                out[i * 4 + 2] = 0;
                out[i * 4 + 3] = 255;
            }
            TOP_UploadInfo info;
            info.textureDesc.width       = WIDTH;
            info.textureDesc.height      = HEIGHT;
            info.textureDesc.texDim      = OP_TexDim::e2D;
            info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex        = 0;
            output->uploadBuffer(&buf, info, nullptr);

            OP_SmartRef<TOP_Buffer> depthBuf = myContext->createOutputBuffer(WIDTH * HEIGHT * 2, TOP_BufferFlags::None, nullptr);
            uint16_t* depthOut = static_cast<uint16_t*>(depthBuf->data);
            for (int i = 0; i < WIDTH * HEIGHT; ++i)
                depthOut[i] = 0;
            TOP_UploadInfo depthInfo;
            depthInfo.textureDesc.width       = WIDTH;
            depthInfo.textureDesc.height      = HEIGHT;
            depthInfo.textureDesc.texDim      = OP_TexDim::e2D;
            depthInfo.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
            depthInfo.colorBufferIndex        = 1;
            // Overwrite upper part with white rectangle and write "Kinect Not Connected"
            const char* errorMsg = "KINECT NOT CONNECTED";
            int msgLen = static_cast<int>(strlen(errorMsg));
            int row = 20;
            for (int i = 0; i < msgLen && i < WIDTH - 1; ++i) {
                int px = (i + (WIDTH - msgLen) / 2);
                int py = row;
                int idx = py * WIDTH + px;
                out[idx * 4 + 0] = 255;
                out[idx * 4 + 1] = 255;
                out[idx * 4 + 2] = 255;
                out[idx * 4 + 3] = 255;
            }

            // Overwrite depth top row with a bright band
            for (int i = 0; i < WIDTH; ++i) {
                int idx = 20 * WIDTH + i;
                depthOut[idx] = 65535;
            }
            output->uploadBuffer(&depthBuf, depthInfo, nullptr);
            return;
        }
        deviceValid = true;
    }
    float tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    try {
        device->setTiltDegrees(tilt);
    } catch (const std::exception& e) {
        fprintf(stderr, "[FreenectTOP] Failed to set tilt: %s", e.what());
        cleanupDevice();
        device = nullptr;
        return;
    }

    std::vector<uint8_t> rgb;
    if (device->getRGB(rgb)) {
        lastRGB = rgb;
        OP_SmartRef<TOP_Buffer> buf = myContext->createOutputBuffer(WIDTH*HEIGHT*4, TOP_BufferFlags::None, nullptr);
        uint8_t* out = static_cast<uint8_t*>(buf->data);
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x) {
                int srcY = HEIGHT - 1 - y;
                int iSrc = (srcY * WIDTH + x) * 3;
                int iDst = (y * WIDTH + x) * 4;
                out[iDst+0] = lastRGB[iSrc+0];
                out[iDst+1] = lastRGB[iSrc+1];
                out[iDst+2] = lastRGB[iSrc+2];
                out[iDst+3] = 255;
            }
        TOP_UploadInfo info;
        info.textureDesc.width       = WIDTH;
        info.textureDesc.height      = HEIGHT;
        info.textureDesc.texDim      = OP_TexDim::e2D;
        info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
        info.colorBufferIndex        = 0;
        output->uploadBuffer(&buf, info, nullptr);
    }

    std::vector<uint16_t> depth;
    if (device->getDepth(depth)) {
        lastDepth = depth;
        OP_SmartRef<TOP_Buffer> buf = myContext->createOutputBuffer(WIDTH*HEIGHT*2, TOP_BufferFlags::None, nullptr);
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

void FreenectTOP::pulsePressed(const char*, void*) {}
