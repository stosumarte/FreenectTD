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

// TouchDesigner Entrypoints
extern "C" {

    using namespace TD;

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
void FreenectTOP::setupParameters(TD::OP_ParameterManager* manager, void*) {
    
    using namespace TD;
    
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
    
    // Device type dropdown
    OP_StringParameter deviceTypeParam;
    deviceTypeParam.name = "Devicetype";
    deviceTypeParam.label = "Device Type";
    deviceTypeParam.defaultValue = "Kinect v1";
    const char* deviceTypeNames[] = {"Kinect v1", "Kinect v2"};
    const char* deviceTypeLabels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
    manager->appendMenu(deviceTypeParam, 2, deviceTypeNames, deviceTypeLabels);
    
    // TO DO - Enable Depth toggle
    /*OP_NumericParameter enableDepthParam;
    enableDepthParam.name = "Enabledepth";
    enableDepthParam.label = "Enable Depth Stream";
    enableDepthParam.defaultValues[0] = 1.0; // Default to enabled
    enableDepthParam.minValues[0] = 0.0;
    enableDepthParam.maxValues[0] = 1.0;
    enableDepthParam.minSliders[0] = 0.0;
    enableDepthParam.maxSliders[0] = 1.0;
    enableDepthParam.clampMins[0] = true;
    enableDepthParam.clampMaxes[0] = true;
    manager->appendToggle(enableDepthParam);*/
    
    // TO DO - Enable IR toggle
    /*OP_NumericParameter enableIRParam;
    enableIRParam.name = "Enableir";
    enableIRParam.label = "Enable IR Stream";
    enableIRParam.defaultValues[0] = 0.0; // Default to disabled
    enableIRParam.minValues[0] = 0.0;
    enableIRParam.maxValues[0] = 1.0;
    enableIRParam.minSliders[0] = 0.0;
    enableIRParam.maxSliders[0] = 1.0;
    enableIRParam.clampMins[0] = true;
    enableIRParam.clampMaxes[0] = true;
    manager->appendToggle(enableIRParam);*/

    // Tilt angle parameter
    OP_NumericParameter tiltAngleParam;
    tiltAngleParam.name = "Tilt";
    tiltAngleParam.label = "Tilt Angle";
    tiltAngleParam.defaultValues[0] = 0.0;
    tiltAngleParam.minValues[0] = -30.0;
    tiltAngleParam.maxValues[0] = 30.0;
    tiltAngleParam.minSliders[0] = -30.0;
    tiltAngleParam.maxSliders[0] = 30.0;
    manager->appendFloat(tiltAngleParam);
    
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
    
    // TO DO - Depth format dropdown (16-bit or 32-bit float)
    OP_StringParameter depthFormatParam;
    depthFormatParam.name = "Depthformat";
    depthFormatParam.label = "Depth Format";
    depthFormatParam.defaultValue = "Raw (16-bit)";
    const char* depthFormatNames[] = {"Raw (16-bit)", "Registered (32-bit float)", "Visualized (8-bit)"};
    const char* depthFormatLabels[] = {"Raw (16-bit)", "Registered (32-bit float)", "Visualized (8-bit)"};
    manager->appendMenu(depthFormatParam, 3, depthFormatNames, depthFormatLabels);
    

}

// TD - Cook every frame
void FreenectTOP::getGeneralInfo(TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::initDeviceV1() {
    LOG("[FreenectTOP] initDeviceV1: start");
    PROFILE("initDeviceV1: start");
    LOG(std::string("[FreenectTOP] initDeviceV1: freenectContext before = ") + std::to_string(reinterpret_cast<uintptr_t>(freenectContext)));
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&freenectContext, nullptr) < 0) {
        PROFILE("initDeviceV1: freenect_init failed");
        LOG("[FreenectTOP] freenect_init failed");
        LOG(std::string("[FreenectTOP] initDeviceV1: freenectContext after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(freenectContext)));
        PROFILE("initDeviceV1: end");
        LOG("[FreenectTOP] initDeviceV1: end (fail)");
        return false;
    }
    freenect_set_log_level(freenectContext, FREENECT_LOG_WARNING);

    int numDevices = freenect_num_devices(freenectContext);
    PROFILE(std::string("initDeviceV1: numDevices=") + std::to_string(numDevices));
    LOG("[FreenectTOP] initDeviceV1: numDevices = " + std::to_string(numDevices));
    if (numDevices <= 0) {
        LOG("[FreenectTOP] No Kinect v1 devices found");
        errorString.clear();
        errorString = "No Kinect v1 devices found";
        freenect_shutdown(freenectContext);
        freenectContext = nullptr;
        LOG(std::string("[FreenectTOP] initDeviceV1: freenectContext shutdown, now = ") + std::to_string(reinterpret_cast<uintptr_t>(freenectContext)));
        PROFILE("initDeviceV1: end");
        LOG("[FreenectTOP] initDeviceV1: end (no devices)");
        return false;
    }

    try {
        firstRGBReady = false;
        firstDepthReady = false;
        LOG(std::string("[FreenectTOP] initDeviceV1: device before = ") + std::to_string(reinterpret_cast<uintptr_t>(device)));
        device = new MyFreenectDevice(freenectContext, 0, firstRGBReady, firstDepthReady);
        PROFILE("initDeviceV1: device created");
        LOG(std::string("[FreenectTOP] initDeviceV1: device after = ") + std::to_string(reinterpret_cast<uintptr_t>(device)));
        device->startVideo();
        device->startDepth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        runV1Events = true;
        LOG("[FreenectTOP] initDeviceV1: runV1Events set to true");
        PROFILE("initDeviceV1: starting eventThreadV1");
        LOG(std::string("[FreenectTOP] initDeviceV1: eventThreadV1 joinable before = ") + std::to_string(eventThreadV1.joinable()));
        eventThreadV1 = std::thread([this]() {
            PROFILE("eventThreadV1: started");
            LOG("[FreenectTOP] eventThreadV1: running");
            while (runV1Events.load()) {
                PROFILE("eventThreadV1: iteration start");
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (!freenectContext) break;
                int err = freenect_process_events(freenectContext);
                if (err < 0) {
                    LOG("[FreenectTOP] Error in freenect_process_events");
                    break;
                }
                PROFILE("eventThreadV1: iteration end");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            PROFILE("eventThreadV1: exiting");
            LOG("[FreenectTOP] eventThreadV1: exiting");
        });
        LOG(std::string("[FreenectTOP] initDeviceV1: eventThreadV1 joinable after = ") + std::to_string(eventThreadV1.joinable()));
        PROFILE("initDeviceV1: eventThreadV1 started");
    } catch (...) {
        LOG("[FreenectTOP] Failed to start device");
        errorString.clear();
        errorString = "Failed to start Kinect v1 device";
        cleanupDeviceV1();
        PROFILE("initDeviceV1: end");
        LOG("[FreenectTOP] initDeviceV1: end (exception)");
        return false;
    }
    PROFILE("initDeviceV1: end");
    LOG("[FreenectTOP] initDeviceV1: end (success)");
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
void FreenectTOP::cleanupDeviceV1() {
    LOG("[FreenectTOP] cleanupDeviceV1: start");
    PROFILE("cleanupDeviceV1: start");
    LOG("[FreenectTOP] cleanupDeviceV1: runV1Events before = " + std::to_string(runV1Events.load()));
    runV1Events = false;
    LOG("[FreenectTOP] cleanupDeviceV1: runV1Events after = " + std::to_string(runV1Events.load()));
    LOG("[FreenectTOP] cleanupDeviceV1: eventThreadV1 joinable = " + std::to_string(eventThreadV1.joinable()));
    if (eventThreadV1.joinable()) {
        PROFILE("cleanupDeviceV1: joining eventThreadV1");
        eventThreadV1.join();
        LOG("[FreenectTOP] eventThreadV1 joined");
    }
    std::lock_guard<std::mutex> lock(freenectMutex);
    LOG(std::string("[FreenectTOP] cleanupDeviceV1: device before = ") + std::to_string(reinterpret_cast<uintptr_t>(device)));
    if (device) {
        device->stopVideo();
        device->stopDepth();
        delete device;
        LOG("[FreenectTOP] device deleted (v1)");
        device = nullptr;
        LOG("[FreenectTOP] device set to nullptr (v1)");
    }
    LOG(std::string("[FreenectTOP] cleanupDeviceV1: freenectContext before = ") + std::to_string(reinterpret_cast<uintptr_t>(freenectContext)));
    if (freenectContext) {
        freenect_shutdown(freenectContext);
        freenectContext = nullptr;
        LOG("[FreenectTOP] freenectContext shutdown (v1)");
        LOG("[FreenectTOP] freenectContext set to nullptr (v1)");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    PROFILE("cleanupDeviceV1: end");
    LOG("[FreenectTOP] cleanupDeviceV1: end");
}

// Add static atomic flag and thread for v2 device search
static std::atomic<bool> v2DeviceAvailable(false);
static std::thread v2EnumThread;
static std::atomic<bool> v2EnumThreadRunning(false);

// Start the background enumeration thread for Kinect v2
void FreenectTOP::startV2EnumThread() {
    LOG("[FreenectTOP] startV2EnumThread: start");
    PROFILE("startV2EnumThread: start");
    LOG("[FreenectTOP] startV2EnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    if (v2EnumThreadRunning.load()) {
        PROFILE("startV2EnumThread: already running");
        LOG("[FreenectTOP] startV2EnumThread: already running");
        PROFILE("startV2EnumThread: end");
        LOG("[FreenectTOP] startV2EnumThread: end (already running)");
        return;
    }
    v2EnumThreadRunning = true;
    LOG("[FreenectTOP] startV2EnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    PROFILE("startV2EnumThread: launching thread");
    v2EnumThread = std::thread([]() {
        while (v2EnumThreadRunning.load()) {
            libfreenect2::Freenect2 ctx;
            v2DeviceAvailable = (ctx.enumerateDevices() > 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    LOG("[FreenectTOP] startV2EnumThread: v2EnumThread joinable after = " + std::to_string(v2EnumThread.joinable()));
    PROFILE("startV2EnumThread: thread launched");
    PROFILE("startV2EnumThread: end");
    LOG("[FreenectTOP] startV2EnumThread: end");
}

// Stop the background enumeration thread for Kinect v2
void FreenectTOP::stopV2EnumThread() {
    LOG("[FreenectTOP] stopV2EnumThread: start");
    PROFILE("stopV2EnumThread: start");
    LOG("[FreenectTOP] stopV2EnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    v2EnumThreadRunning = false;
    LOG("[FreenectTOP] stopV2EnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    LOG("[FreenectTOP] stopV2EnumThread: v2EnumThread joinable = " + std::to_string(v2EnumThread.joinable()));
    if (v2EnumThread.joinable()) {
        PROFILE("stopV2EnumThread: joining thread");
        v2EnumThread.join();
        LOG("[FreenectTOP] stopV2EnumThread: thread joined");
    }
    PROFILE("stopV2EnumThread: end");
    LOG("[FreenectTOP] stopV2EnumThread: end");
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::initDeviceV2() {
    LOG("[FreenectTOP] initDeviceV2: start");
    PROFILE("initDeviceV2: start");
    LOG(std::string("[FreenectTOP] initDeviceV2: fn2_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    std::lock_guard<std::mutex> lock(freenectMutex);
    startV2EnumThread();
    if (!v2DeviceAvailable.load()) {
        PROFILE("initDeviceV2: no device available");
        LOG("[FreenectTOP] initDeviceV2: no device available");
        PROFILE("initDeviceV2: end");
        LOG("[FreenectTOP] initDeviceV2: end (no device)");
        return false;
    }
    if (fn2_ctx) {
        PROFILE("initDeviceV2: already initialized");
        LOG("[FreenectTOP] initDeviceV2: already initialized");
        PROFILE("initDeviceV2: end");
        LOG("[FreenectTOP] initDeviceV2: end (already initialized)");
        return true;
    }
    fn2_ctx = new libfreenect2::Freenect2();
    PROFILE("initDeviceV2: fn2_ctx created");
    LOG(std::string("[FreenectTOP] initDeviceV2: fn2_ctx after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx->enumerateDevices() == 0) {
        LOG("[FreenectTOP] No Kinect v2 devices found");
        errorString.clear();
        errorString = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] initDeviceV2: fn2_ctx deleted and set to nullptr");
        PROFILE("initDeviceV2: end");
        LOG("[FreenectTOP] initDeviceV2: end (no devices)");
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
        LOG("[FreenectTOP] Using CPU pipeline for Kinect v2");
        PROFILE("initDeviceV2: CPU pipeline created");
        LOG(std::string("[FreenectTOP] initDeviceV2: fn2_pipeline after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    } catch (...) {
        errorString.clear();
        errorString = "Couldn't create CPU pipeline for Kinect v2";
        LOG("Couldn't create CPU pipeline for Kinect v2");
        PROFILE("initDeviceV2: CPU pipeline creation failed");
        LOG(std::string("[FreenectTOP] initDeviceV2: fn2_pipeline after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    }
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    LOG(std::string("[FreenectTOP] initDeviceV2: openDevice returned dev = ") + std::to_string(reinterpret_cast<uintptr_t>(dev)));
    if (!dev) {
        LOG("[FreenectTOP] Failed to open Kinect v2 device");
        errorString.clear();
        errorString = "Failed to open Kinect v2 device";
        delete fn2_device;
        LOG("[FreenectTOP] initDeviceV2: fn2_device deleted");
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
            LOG("[FreenectTOP] initDeviceV2: fn2_pipeline deleted and set to nullptr");
        }
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
            LOG("[FreenectTOP] initDeviceV2: fn2_ctx deleted and set to nullptr");
        }
        fn2_device = nullptr;
        LOG("[FreenectTOP] initDeviceV2: fn2_device set to nullptr");
        PROFILE("initDeviceV2: end");
        LOG("[FreenectTOP] initDeviceV2: end (openDevice fail)");
        return false;
    }
    if (!fn2_device) {
        fn2_device = new MyFreenect2Device(dev, fn2_rgbReady, fn2_depthReady);
        PROFILE("initDeviceV2: fn2_device created");
        LOG(std::string("[FreenectTOP] initDeviceV2: fn2_device after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_device)));
    }
    if (!fn2_device->start()) {
        LOG("[FreenectTOP] Failed to start Kinect v2 device");
        errorString.clear();
        errorString = "Failed to start Kinect v2 device";
        delete fn2_device;
        LOG("[FreenectTOP] initDeviceV2: fn2_device deleted");
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
            LOG("[FreenectTOP] initDeviceV2: fn2_pipeline deleted and set to nullptr");
        }
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
            LOG("[FreenectTOP] initDeviceV2: fn2_ctx deleted and set to nullptr");
        }
        fn2_device = nullptr;
        LOG("[FreenectTOP] initDeviceV2: fn2_device set to nullptr");
        PROFILE("initDeviceV2: end");
        LOG("[FreenectTOP] initDeviceV2: end (start fail)");
        return false;
    }
    // **Stop enumeration thread after successful device start**
    stopV2EnumThread();
    PROFILE("initDeviceV2: device started and enum thread stopped");
    LOG("[FreenectTOP] initDeviceV2: device started and enum thread stopped");
    // Start event thread for v2
    LOG("[FreenectTOP] initDeviceV2: eventThreadV2 joinable before = " + std::to_string(eventThreadV2.joinable()));
    if (!eventThreadV2.joinable()) {
        runV2Events = true;
        LOG("[FreenectTOP] initDeviceV2: runV2Events set to true");
        PROFILE("initDeviceV2: starting eventThreadV2");
        eventThreadV2 = std::thread([this]() {
            PROFILE("eventThreadV2: started");
            LOG("[FreenectTOP] eventThreadV2: running");
            while (runV2Events.load()) {
                PROFILE("eventThreadV2: iteration start");
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (fn2_device) {
                    fn2_device->processFrames();
                } else {
                    LOG("[FreenectTOP] fn2_device is null in event thread");
                }
                PROFILE("eventThreadV2: iteration end");
            }
            PROFILE("eventThreadV2: exiting");
            LOG("[FreenectTOP] eventThreadV2: exiting");
        });
        LOG("[FreenectTOP] initDeviceV2: eventThreadV2 joinable after = " + std::to_string(eventThreadV2.joinable()));
        PROFILE("initDeviceV2: eventThreadV2 started");
    }
    // After opening device, create Registration object
    if (fn2_device && !registrationV2) {
        registrationV2 = new libfreenect2::Registration(
            dev->getIrCameraParams(),
            dev->getColorCameraParams()
        );
        LOG("[FreenectTOP] registrationV2 created");
    }
    PROFILE("initDeviceV2: end");
    LOG("[FreenectTOP] initDeviceV2: end (success)");
    return true;
}

// Cleanup for Kinect v2 (libfreenect2)
void FreenectTOP::cleanupDeviceV2() {
    LOG("[FreenectTOP] cleanupDeviceV2: start");
    PROFILE("cleanupDeviceV2: start");
    LOG("[FreenectTOP] cleanupDeviceV2: runV2Events before = " + std::to_string(runV2Events.load()));
    runV2Events = false;
    LOG("[FreenectTOP] cleanupDeviceV2: runV2Events after = " + std::to_string(runV2Events.load()));
    LOG("[FreenectTOP] cleanupDeviceV2: eventThreadV2 joinable = " + std::to_string(eventThreadV2.joinable()));
    LOG("[FreenectTOP] Stopping event thread for v2");
    if (eventThreadV2.joinable()) {
        PROFILE("cleanupDeviceV2: joining eventThreadV2");
        eventThreadV2.join();
        LOG("[FreenectTOP] Event thread for v2 stopped (joinable and joined)");
    }
    std::lock_guard<std::mutex> lock(freenectMutex);
    LOG("[FreenectTOP] cleanupDeviceV2: Locked freenectMutex)");
    stopV2EnumThread();
    LOG(std::string("[FreenectTOP] cleanupDeviceV2: fn2_device before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_device)));
    if (fn2_device) {
        delete fn2_device;
        LOG("[FreenectTOP] fn2_device deleted");
        fn2_device = nullptr;
        LOG("[FreenectTOP] fn2_device set to nullptr");
    }
    LOG(std::string("[FreenectTOP] cleanupDeviceV2: fn2_pipeline before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    if (fn2_pipeline) {
        fn2_pipeline = nullptr;
        LOG("[FreenectTOP] fn2_pipeline set to nullptr");
    }
    LOG(std::string("[FreenectTOP] cleanupDeviceV2: fn2_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx) {
        delete fn2_ctx;
        LOG("[FreenectTOP] fn2_ctx deleted");
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_ctx set to nullptr");
    }
    // Cleanup registrationV2
    if (registrationV2) {
        delete registrationV2;
        registrationV2 = nullptr;
        LOG("[FreenectTOP] registrationV2 deleted");
    }
    PROFILE("cleanupDeviceV2: end");
    LOG("[FreenectTOP] cleanupDeviceV2: end");
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
    fallbackBuffer = nullptr; // Release fallback buffer
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
        TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(colorWidth * colorHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
        TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(depthWidth * depthHeight * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
void FreenectTOP::executeV2(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    int colorWidth = MyFreenect2Device::WIDTH,
        colorHeight = MyFreenect2Device::HEIGHT,
        colorScaledWidth = MyFreenect2Device::SCALED_WIDTH,
        colorScaledHeight = MyFreenect2Device::SCALED_HEIGHT,
        depthWidth = MyFreenect2Device::DEPTH_WIDTH,
        depthHeight = MyFreenect2Device::DEPTH_HEIGHT;
    bool v2InitOk = true;
    if (!fn2_device) {
        LOG("[FreenectTOP] executeV2: fn2_device is null, initializing device");
        v2InitOk = initDeviceV2();
        if (!v2InitOk) {
            LOG("[FreenectTOP] executeV2: initDeviceV2 failed, returning early");
            LOG("[FreenectTOP] Kinect v2 init failed or no device found");
            errorString = "No Kinect v2 devices found";
            uploadFallbackBuffer();
            return;
        }
    }
    bool downscale = (inputs->getParInt("Resolutionlimit") != 0);
    int outW = downscale ? colorScaledWidth : colorWidth;
    int outH = downscale ? colorScaledHeight : colorHeight;
    if (!fn2_device || !v2InitOk) {
        LOG("[FreenectTOP] executeV2: fn2_device is null or v2InitOk is false, returning early");
        errorString = "No Kinect v2 devices found";
        uploadFallbackBuffer();
        return;
    }
    std::vector<uint8_t> colorFrame;
    bool gotColor = fn2_device->getColorFrame(colorFrame, downscale);
    if (gotColor) {
        errorString.clear();
        LOG("[FreenectTOP] executeV2: creating color output buffer");
        TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outW * outH * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV2: copying color frame data to buffer");
            std::memcpy(buf->data, colorFrame.data(), outW * outH * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = outW;
            info.textureDesc.height = outH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            LOG("[FreenectTOP] executeV2: uploading color buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV2: failed to create color output buffer");
        }
    }
    // --- Output depth according to Depthformat parameter ---
    std::vector<uint16_t> depthFrame;
    bool gotDepth = fn2_device->getDepthFrame(depthFrame);
    if (gotDepth) {
        errorString.clear();
        int outDW = depthWidth, outDH = depthHeight;
        std::string depthFormatStr = inputs->getParString("Depthformat") ? inputs->getParString("Depthformat") : "Raw (16-bit)";
        if (depthFormatStr == "Raw (16-bit)") {
            LOG("[FreenectTOP] executeV2: outputting raw depth");
            TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outDW * outDH * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                std::memcpy(buf->data, depthFrame.data(), outDW * outDH * 2);
                TD::TOP_UploadInfo info;
                info.textureDesc.width = outDW;
                info.textureDesc.height = outDH;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex = 1;
                LOG("[FreenectTOP] executeV2: uploading raw depth buffer");
                output->uploadBuffer(&buf, info, nullptr);
            } else {
                LOG("[FreenectTOP] executeV2: failed to create raw depth output buffer");
            }
        } else if (depthFormatStr == "Registered (32-bit float)" && registrationV2) {
            LOG("[FreenectTOP] executeV2: outputting registered depth");
            libfreenect2::Frame rgbFrame(outW, outH, 4, colorFrame.data());
            libfreenect2::Frame depthFrameF(outDW, outDH, 2, reinterpret_cast<uint8_t*>(depthFrame.data()));
            libfreenect2::Frame undistorted(outDW, outDH, 4);
            libfreenect2::Frame registered(outDW, outDH, 4);
            registrationV2->apply(&rgbFrame, &depthFrameF, &undistorted, &registered);
            TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outDW * outDH * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                std::memcpy(buf->data, registered.data, outDW * outDH * 4);
                TD::TOP_UploadInfo info;
                info.textureDesc.width = outDW;
                info.textureDesc.height = outDH;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono32Float;
                info.colorBufferIndex = 1;
                LOG("[FreenectTOP] executeV2: uploading registered depth buffer");
                output->uploadBuffer(&buf, info, nullptr);
            } else {
                LOG("[FreenectTOP] executeV2: failed to create registered depth output buffer");
            }
        } else if (depthFormatStr == "Visualized (8-bit)") {
            LOG("[FreenectTOP] executeV2: outputting visualized depth");
            std::vector<uint8_t> visualized(outDW * outDH);
            for (int i = 0; i < outDW * outDH; ++i) {
                visualized[i] = static_cast<uint8_t>(std::min(255, depthFrame[i] / 8)); // Simple normalization
            }
            TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext ? myContext->createOutputBuffer(outDW * outDH, TD::TOP_BufferFlags::None, nullptr) : nullptr;
            if (buf) {
                std::memcpy(buf->data, visualized.data(), outDW * outDH);
                TD::TOP_UploadInfo info;
                info.textureDesc.width = outDW;
                info.textureDesc.height = outDH;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono8Fixed;
                info.colorBufferIndex = 1;
                LOG("[FreenectTOP] executeV2: uploading visualized depth buffer");
                output->uploadBuffer(&buf, info, nullptr);
            } else {
                LOG("[FreenectTOP] executeV2: failed to create visualized depth output buffer");
            }
        }
    }
}

// Main execution method
void FreenectTOP::execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) {
    myCurrentOutput = output;
    
    if (!inputs) {
        LOG("[FreenectTOP] ERROR: inputs is null!");
        return;
    }
    
    // Get device type parameter
    const char* devTypeCStr = inputs->getParString("Devicetype");
    std::string deviceTypeStr = devTypeCStr ? devTypeCStr : "Kinect v1";
    int newDeviceType = (deviceTypeStr == "Kinect v2") ? 1 : 0;
    deviceType = newDeviceType;
    LOG("[FreenectTOP] execute: switching device type from " + lastDeviceTypeStr + " to " + deviceTypeStr);
    
    // Enable/disable parameters based on device type
    if (deviceType == 0) {
        inputs->enablePar("Tilt", true);                // Enable Tilt for Kinect v1
        inputs->enablePar("Resolutionlimit", false);    // Disable Resolutionlimit for Kinect v1
        inputs->enablePar("Depthformat", false);        // Disable Depthformat for Kinect v1
    } else if (deviceType == 1) {
        inputs->enablePar("Tilt", false);               // Disable Tilt for Kinect v2
        inputs->enablePar("Resolutionlimit", true);     // Enable Resolutionlimit for Kinect v2
        inputs->enablePar("Depthformat", true);         // Enable Depthformat for Kinect v2
    }
    
    // If device type changed, re-init device
    if (deviceTypeStr != lastDeviceTypeStr) {
        LOG("[FreenectTOP] execute: device type changed, cleaning up devices");
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
    if (!fallbackBuffer) {
        std::vector<uint8_t> black(fallbackWidth * fallbackHeight * 4, 0);
        fallbackBuffer = myContext ? myContext->createOutputBuffer(fallbackWidth * fallbackHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (fallbackBuffer) {
            std::memcpy(fallbackBuffer->data, black.data(), fallbackWidth * fallbackHeight * 4);
        }
    }
    if (fallbackBuffer) {
        TD::TOP_UploadInfo info;
        info.textureDesc.width = fallbackWidth;
        info.textureDesc.height = fallbackHeight;
        info.textureDesc.texDim = TD::OP_TexDim::e2D;
        info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
        info.colorBufferIndex = 0; // Always upload to buffer index 0
        myCurrentOutput->uploadBuffer(&fallbackBuffer, info, nullptr);
        LOG("[FreenectTOP] uploadFallbackBuffer: uploaded persistent fallback black buffer");
    }
}
