//
// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include "logger.h"
#include <atomic>
#include <thread>
#include <iostream>

#ifndef DLLEXPORT
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#ifndef FREENECTTOP_VERSION
#define FREENECTTOP_VERSION "dev"
#endif

using namespace TD;

// TouchDesigner Entrypoints
extern "C" {

        DLLEXPORT void FillTOPPluginInfo(TOP_PluginInfo* info) {
        info->apiVersion = TOPCPlusPlusAPIVersion;
        info->executeMode = TOP_ExecuteMode::CPUMem;
        info->customOPInfo.opType->setString("Freenect");
        info->customOPInfo.opLabel->setString("Freenect");
        info->customOPInfo.opIcon->setString("FNT");
        info->customOPInfo.authorName->setString("Marte Tagliabue");
        info->customOPInfo.authorEmail->setString("ciao@marte.ee");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 0;
    }

    DLLEXPORT TOP_CPlusPlusBase* CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context) {
        return new FreenectTOP(info, context);
    }

    DLLEXPORT void DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context* context) {
        delete static_cast<FreenectTOP*>(instance);
    }
}

// Touchdesigner Parameters
void FreenectTOP::setupParameters(OP_ParameterManager* manager, void*) {
    
    // Show version in header
    OP_StringParameter versionHeader;
    versionHeader.name = "Version";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION + " – by @stosumarte";
    versionHeader.label = versionLabel.c_str();
    manager->appendHeader(versionHeader);
    
    // Check for updates button
    OP_NumericParameter checkUpdateParam;
    checkUpdateParam.name = "Checkforupdates";
    checkUpdateParam.label = "Check for updates";
    checkUpdateParam.defaultValues[0] = 0.0;
    checkUpdateParam.minValues[0] = 0.0;
    checkUpdateParam.maxValues[0] = 1.0;
    checkUpdateParam.minSliders[0] = 0.0;
    checkUpdateParam.maxSliders[0] = 1.0;
    checkUpdateParam.clampMins[0] = true;
    checkUpdateParam.clampMaxes[0] = true;
    manager->appendMomentary(checkUpdateParam);
    

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

    // DEPRECATED - Invert depth map toggle
    /*OP_NumericParameter invertParam;
    invertParam.name = "Invertdepth";
    invertParam.label = "Invert Depth Map";
    invertParam.defaultValues[0] = 0.0;
    invertParam.minValues[0] = 0.0;
    invertParam.maxValues[0] = 1.0;
    invertParam.minSliders[0] = 0.0;
    invertParam.maxSliders[0] = 1.0;
    invertParam.clampMins[0] = true;
    invertParam.clampMaxes[0] = true;
    manager->appendToggle(invertParam);*/

    // Device type dropdown
    OP_StringParameter deviceTypeParam;
    deviceTypeParam.name = "Devicetype";
    deviceTypeParam.label = "Device Type";
    deviceTypeParam.defaultValue = "Kinect v1";
    const char* deviceTypeNames[] = {"Kinect v1", "Kinect v2"};
    const char* deviceTypeLabels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
    manager->appendMenu(deviceTypeParam, 2, deviceTypeNames, deviceTypeLabels);
    
    // Resolution limiter toggle - Limit resolution to 1280x720 for Kinect V2 instead of 1920x1080 (for non-commercial licenses)
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

// TD - Cook every frame
void FreenectTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::initDeviceV1() {
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&freenectContext, nullptr) < 0) {
        //LOG("[FreenectTOP] freenect_init failed");
        return false;
    }
    freenect_set_log_level(freenectContext, FREENECT_LOG_WARNING);

    int numDevices = freenect_num_devices(freenectContext);
    if (numDevices <= 0) {
        LOG("[FreenectTOP] No Kinect v1 devices found");
        errorString.clear();
        errorString = "No Kinect v1 devices found";
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

        runV1Events = true;
        eventThreadV1 = std::thread([this]() {
            while (runV1Events.load()) {
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (!freenectContext) break;
                int err = freenect_process_events(freenectContext);
                if (err < 0) {
                    LOG("[FreenectTOP] Error in freenect_process_events");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    } catch (...) {
        LOG("[FreenectTOP] Failed to start device");
        errorString.clear();
        errorString = "Failed to start Kinect v1 device";
        cleanupDeviceV1();
        return false;
    }
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
void FreenectTOP::cleanupDeviceV1() {
    LOG("[FreenectTOP] cleanupDeviceV1 called");
    runV1Events = false;
    if (eventThreadV1.joinable()) {
        eventThreadV1.join();
        LOG("[FreenectTOP] eventThreadV1 joined");
    }
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (device) {
        device->stopVideo();
        device->stopDepth();
        delete device;
        LOG("[FreenectTOP] device deleted (v1)");
        device = nullptr;
    }
    if (freenectContext) {
        freenect_shutdown(freenectContext);
        freenectContext = nullptr;
        LOG("[FreenectTOP] freenectContext shutdown (v1)");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Add static atomic flag and thread for v2 device search
static std::atomic<bool> v2DeviceAvailable(false);
static std::thread v2EnumThread;
static std::atomic<bool> v2EnumThreadRunning(false);

// Start the background enumeration thread for Kinect v2
void FreenectTOP::startV2EnumThread() {
    if (v2EnumThreadRunning.load()) return;
    v2EnumThreadRunning = true;
    v2EnumThread = std::thread([]() {
        while (v2EnumThreadRunning.load()) {
            libfreenect2::Freenect2 ctx;
            v2DeviceAvailable = (ctx.enumerateDevices() > 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

// Stop the background enumeration thread for Kinect v2
void FreenectTOP::stopV2EnumThread() {
    v2EnumThreadRunning = false;
    if (v2EnumThread.joinable()) v2EnumThread.join();
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::initDeviceV2() {
    
    std::lock_guard<std::mutex> lock(freenectMutex);
    
    startV2EnumThread();
    
    if (!v2DeviceAvailable.load()) return false;
    
    if (fn2_ctx) return true;
    
    fn2_ctx = new libfreenect2::Freenect2();
    
    if (fn2_ctx->enumerateDevices() == 0) {
        LOG("[FreenectTOP] No Kinect v2 devices found");
        errorString.clear();
        errorString = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        return false;
    }
    
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
        LOG("[FreenectTOP] Using CPU pipeline for Kinect v2");
    } catch (...) {
        errorString.clear();
        errorString = "Couldn't create CPU pipeline for Kinect v2";
        LOG("Couldn't create CPU pipeline for Kinect v2");
    }
    
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    
    if (!dev) {
        LOG("[FreenectTOP] Failed to open Kinect v2 device");
        errorString.clear();
        errorString = "Failed to open Kinect v2 device";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
        }
        return false;
    }
    
    if (!fn2_device) {
        fn2_device = new MyFreenect2Device(dev, fn2_rgbReady, fn2_depthReady);
    }
    
    if (!fn2_device->start()) {
        LOG("[FreenectTOP] Failed to start Kinect v2 device");
        errorString.clear();
        errorString = "Failed to start Kinect v2 device";
        delete fn2_device;
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
        }
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
        }
        fn2_device = nullptr;
        return false;
    }
    
    // **Stop enumeration thread after successful device start**
    stopV2EnumThread();

    // Start event thread for v2
    if (!eventThreadV2.joinable()) {
        runV2Events = true;
        eventThreadV2 = std::thread([this]() {
            while (runV2Events.load()) {
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (fn2_device) {
                    fn2_device->processFrames();
                } else {
                    LOG("[FreenectTOP] fn2_device is null in event thread");
                }
            }
        });
    }
    return true;
}

// Cleanup for Kinect v2 (libfreenect2)
void FreenectTOP::cleanupDeviceV2() {
    LOG("[FreenectTOP] cleanupDeviceV2 called");
    runV2Events = false;
    LOG("[FreenectTOP] Stopping event thread for v2");
    if (eventThreadV2.joinable()) {
        eventThreadV2.join();
        LOG("[FreenectTOP] Event thread for v2 stopped (joinable and joined)");
    }
    std::lock_guard<std::mutex> lock(freenectMutex);
    LOG("[FreenectTOP] Locked freenectMutex)");
    stopV2EnumThread();
    if (fn2_device) {
        delete fn2_device;
        LOG("[FreenectTOP] fn2_device deleted");
        fn2_device = nullptr;
        LOG("[FreenectTOP] fn2_device set to nullptr");
    }
    if (fn2_pipeline) {
        fn2_pipeline = nullptr;
        LOG("[FreenectTOP] fn2_pipeline set to nullptr");
    }
    if (fn2_ctx) {
        delete fn2_ctx;
        LOG("[FreenectTOP] fn2_ctx deleted");
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_ctx set to nullptr");
    }
    
    
}

// Constructor for FreenectTOP
FreenectTOP::FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context)
    : myNodeInfo(info),
      myContext(context),
      freenectContext(nullptr),
      device(nullptr),
      firstRGBReady(false),
      firstDepthReady(false)
{
    // Do not initialize device here, will be done in execute
}

// Destructor for FreenectTOP
FreenectTOP::~FreenectTOP() {
    LOG("[FreenectTOP] Destructor called, cleaning up devices");
    cleanupDeviceV1();
    cleanupDeviceV2();
}

// Execute method for Kinect v1 (libfreenect)
void FreenectTOP::executeV1(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    int colorWidth = MyFreenectDevice::WIDTH, colorHeight = MyFreenectDevice::HEIGHT, depthWidth = MyFreenectDevice::WIDTH, depthHeight = MyFreenectDevice::HEIGHT;
    if (!device) {
        LOG("[FreenectTOP] executeV1: device is null, cleaning up and re-initializing");
        cleanupDeviceV1();
        if (!initDeviceV1()) {
            LOG("[FreenectTOP] executeV1: initDeviceV1 failed, uploading fallback black buffer");
            errorString.clear();
            errorString = "No Kinect v1 devices found";
            // Upload a fallback black buffer to TD to avoid Metal crash
            uploadFallbackBuffer();
            return;
        }
    }
    if (!device) {
        LOG("[FreenectTOP] ERROR: device is null after init! Uploading fallback black buffer");
        errorString = "Device is null after initialization";
        // Upload a fallback black buffer to TD to avoid Metal crash
        uploadFallbackBuffer();
        return;
    }
    // If device is not available, do not proceed further
    if (!device) {
        LOG("[FreenectTOP] executeV1: device is still null, aborting frame");
        return;
    }
    float tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    try {
        device->setTiltDegrees(tilt);
    } catch (const std::exception& e) {
        LOG(std::string("[FreenectTOP] Failed to set tilt: ") + e.what());
        errorString = "Failed to set tilt angle: " + std::string(e.what());
        LOG("[FreenectTOP] executeV1: cleaning up device due to tilt error");
        cleanupDeviceV1();
        device = nullptr;
        return;
    }
    std::vector<uint8_t> colorFrame;
    if (device->getColorFrame(colorFrame)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating color output buffer");
        OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(colorWidth * colorHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV1: copying color frame data to buffer");
            std::memcpy(buf->data, colorFrame.data(), colorWidth * colorHeight * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = colorWidth;
            info.textureDesc.height = colorHeight;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            LOG("[FreenectTOP] executeV1: uploading color buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create color output buffer");
        }
    }
    std::vector<uint16_t> depthFrame;
    if (device->getDepthFrame(depthFrame)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating depth output buffer");
        OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(depthWidth * depthHeight * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV1: copying depth frame data to buffer");
            std::memcpy(buf->data, depthFrame.data(), depthWidth * depthHeight * 2);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = depthWidth;
            info.textureDesc.height = depthHeight;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 1;
            LOG("[FreenectTOP] executeV1: uploading depth buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create depth output buffer");
        }
    }
}

// Execute method for Kinect v2 (libfreenect2)
void FreenectTOP::executeV2(TOP_Output* output, const OP_Inputs* inputs) {
    int colorWidth = MyFreenect2Device::WIDTH, colorHeight = MyFreenect2Device::HEIGHT, depthWidth = MyFreenect2Device::DEPTH_WIDTH, depthHeight = MyFreenect2Device::DEPTH_HEIGHT;
    bool v2InitOk = true;
    if (!fn2_device) {
        LOG("[FreenectTOP] executeV2: fn2_device is null, initializing device");
        v2InitOk = initDeviceV2();
        if (!v2InitOk) {
            LOG("[FreenectTOP] executeV2: initDeviceV2 failed, returning early");
            LOG("[FreenectTOP] Kinect v2 init failed or no device found");
            errorString = "No Kinect v2 devices found";
            return;
        }
    }
    bool downscale = (inputs->getParInt("Resolutionlimit") != 0);
    int outW = downscale ? 1280 : colorWidth;
    int outH = downscale ? 720 : colorHeight;
    if (!fn2_device || !v2InitOk) {
        LOG("[FreenectTOP] executeV2: fn2_device is null or v2InitOk is false, returning early");
        errorString = "No Kinect v2 devices found";
        return;
    }
    std::vector<uint8_t> colorFrame;
    bool gotColor = fn2_device->getColorFrame(colorFrame, downscale);
    if (gotColor) {
        errorString.clear();
        LOG("[FreenectTOP] executeV2: creating color output buffer");
        OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outW * outH * 4, TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV2: copying color frame data to buffer");
            std::memcpy(buf->data, colorFrame.data(), outW * outH * 4);
            TOP_UploadInfo info;
            info.textureDesc.width = outW;
            info.textureDesc.height = outH;
            info.textureDesc.texDim = OP_TexDim::e2D;
            info.textureDesc.pixelFormat = OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            LOG("[FreenectTOP] executeV2: uploading color buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV2: failed to create color output buffer");
        }
    }
    std::vector<uint16_t> depthFrame;
    bool gotDepth = fn2_device->getDepthFrame(depthFrame);
    if (gotDepth) {
        errorString.clear();
        int outDW = depthWidth, outDH = depthHeight;
        LOG("[FreenectTOP] executeV2: creating depth output buffer");
        OP_SmartRef<TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outDW * outDH * 2, TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV2: copying depth frame data to buffer");
            std::memcpy(buf->data, depthFrame.data(), outDW * outDH * 2);
            TOP_UploadInfo info;
            info.textureDesc.width = outDW;
            info.textureDesc.height = outDH;
            info.textureDesc.texDim = OP_TexDim::e2D;
            info.textureDesc.pixelFormat = OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 1;
            LOG("[FreenectTOP] executeV2: uploading depth buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV2: failed to create depth output buffer");
        }
    }
}

// Main execution method
void FreenectTOP::execute(TOP_Output* output, const OP_Inputs* inputs, void*) {
    myCurrentOutput = output;
    if (!inputs) {
        LOG("[FreenectTOP] ERROR: inputs is null!");
        return;
    }
    const char* devTypeCStr = inputs->getParString("Devicetype");
    std::string deviceTypeStr = devTypeCStr ? devTypeCStr : "Kinect v1";
    int newDeviceType = (deviceTypeStr == "Kinect v2") ? 1 : 0;
    deviceType = newDeviceType;
    
    // If device type changed, re-init device
    if (deviceTypeStr != lastDeviceTypeStr) {
        cleanupDeviceV1();
        cleanupDeviceV2();
        lastDeviceTypeStr = deviceTypeStr;
    }

    // Device type handling
    if (deviceType == 0) {                  // Kinect v1
        executeV1(output, inputs);
    } else if (deviceType == 1) {           // Kinect v2
        executeV2(output, inputs);
    } else {                                // Invalid device type string (should not happen like EVER)
        errorString.clear();
        errorString = "Couldn't get device type - something went REALLY wrong";
        LOG("ERROR: Couldn't get device type - something went REALLY wrong");
        return;
    }
}

// Pulse handler for FreenectTOP - currently does nothing
void FreenectTOP::pulsePressed(const char*, void*) {}

// Upload a fallback black buffer
void FreenectTOP::uploadFallbackBuffer() {
    if (!myCurrentOutput) {
        LOG("[FreenectTOP] uploadFallbackBuffer: myCurrentOutput is null, cannot upload fallback buffer");
        return;
    }
    int fallbackWidth = 256, fallbackHeight = 256;
    std::vector<uint8_t> black(fallbackWidth * fallbackHeight * 4, 0);
    OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(fallbackWidth * fallbackHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
    if (buf) {
        std::memcpy(buf->data, black.data(), fallbackWidth * fallbackHeight * 4);
        TD::TOP_UploadInfo info;
        info.textureDesc.width = fallbackWidth;
        info.textureDesc.height = fallbackHeight;
        info.textureDesc.texDim = TD::OP_TexDim::e2D;
        info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
        info.colorBufferIndex = 0; // Always upload to buffer index 0
        myCurrentOutput->uploadBuffer(&buf, info, nullptr);
        LOG("[FreenectTOP] uploadFallbackBuffer: uploaded fallback black buffer");
    }
}
