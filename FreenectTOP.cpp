//
// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include "ofxKinectExtras.h"
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
    
    // -------------
    // FREENECT PAGE
    // -------------
    
    // Active toggle
    OP_NumericParameter activeParam;
    activeParam.name = "Active";
    activeParam.label = "Active";
    activeParam.page = "Freenect";
    activeParam.defaultValues[0] = 1.0; // Default to enabled
    activeParam.minValues[0] = 0.0;
    activeParam.maxValues[0] = 1.0;
    activeParam.minSliders[0] = 0.0;
    activeParam.maxSliders[0] = 1.0;
    activeParam.clampMins[0] = true;
    activeParam.clampMaxes[0] = true;
    manager->appendToggle(activeParam);
    
    // Hardware version dropdown
    OP_StringParameter deviceTypeParam;
    deviceTypeParam.name = "Hardwareversion";
    deviceTypeParam.label = "Hardware Version";
    deviceTypeParam.page = "Freenect";
    deviceTypeParam.defaultValue = "Kinect v1";
    const char* deviceTypeNames[] = {"Kinect v1", "Kinect v2"};
    const char* deviceTypeLabels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
    manager->appendMenu(deviceTypeParam, 2, deviceTypeNames, deviceTypeLabels);
    
    // Kinect V1 firmware binary file path
    OP_StringParameter fn1FirmwarePathParam;
    fn1FirmwarePathParam.name = "Firmwarepathv1";
    fn1FirmwarePathParam.label = "Firmware Path";
    fn1FirmwarePathParam.page = "Freenect";
    fn1FirmwarePathParam.defaultValue = "";
    manager->appendFile(fn1FirmwarePathParam);
    
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
    tiltAngleParam.page = "Freenect";
    tiltAngleParam.defaultValues[0] = 0.0;
    tiltAngleParam.minValues[0] = -30.0;
    tiltAngleParam.maxValues[0] = 30.0;
    tiltAngleParam.minSliders[0] = -30.0;
    tiltAngleParam.maxSliders[0] = 30.0;
    manager->appendFloat(tiltAngleParam);
    
    // Resolution limiter toggle
    // Limit resolution to 1280x720 for Kinect V2 instead of 1920x1080 (for non-commercial licenses)
    // !!! Look into automating this based on requested output resolution
    OP_NumericParameter resLimitParam;
    resLimitParam.name = "Resolutionlimit";
    resLimitParam.label = "Resolution Limit";
    resLimitParam.page = "Freenect";
    resLimitParam.defaultValues[0] = 1.0; // Default to enabled
    resLimitParam.minValues[0] = 0.0;
    resLimitParam.maxValues[0] = 1.0;
    resLimitParam.minSliders[0] = 0.0;
    resLimitParam.maxSliders[0] = 1.0;
    resLimitParam.clampMins[0] = true;
    resLimitParam.clampMaxes[0] = true;
    manager->appendToggle(resLimitParam);
    
    // WIP - Depth format dropdown
    OP_StringParameter depthFormatParam;
    depthFormatParam.name = "Depthformat";
    depthFormatParam.label = "Depth Format";
    depthFormatParam.page = "Freenect";
    depthFormatParam.defaultValue = "Raw (16-bit)";
    const char* depthFormatNames[] = {"Raw (16-bit)", "Visualized (8-bit)"};
    const char* depthFormatLabels[] = {"Raw (16-bit)", "Visualized (8-bit)"};
    manager->appendMenu(depthFormatParam, 2, depthFormatNames, depthFormatLabels);
    
    // TO DO - Distorted / undistorted depth dropdown
    OP_StringParameter depthDistortParam;
    depthDistortParam.name = "Depthdistortion";
    depthDistortParam.label = "Depth Distortion";
    depthDistortParam.page = "Freenect";
    depthDistortParam.defaultValue = "Distorted";
    const char* depthDistortNames[] = {"Distorted", "Undistorted"};
    const char* depthDistortLabels[] = {"Distorted", "Undistorted"};
    manager->appendMenu(depthDistortParam, 2, depthDistortNames, depthDistortLabels);
    
    // ----------
    // ABOUT PAGE
    // ----------
    
    
    
    // Show version in header
    OP_StringParameter versionHeader;
    versionHeader.name = "Version";
    versionHeader.page = "About";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION + " – by @stosumarte";
    versionHeader.label = versionLabel.c_str();
    manager->appendHeader(versionHeader);
    
    // Empty spacer header
    OP_StringParameter emptyHeader1;
    emptyHeader1.name = "Emptyheader1";
    emptyHeader1.page = "About";
    std::string emptyLabel1 = std::string(" ");
    emptyHeader1.label = emptyLabel1.c_str();
    manager->appendHeader(emptyHeader1);
    
    // Check for updates header
    OP_StringParameter updateHeader;
    updateHeader.name = "Updateheader";
    updateHeader.page = "About";
    std::string updateLabel = std::string("Visit the following URL to check for updates:");
    updateHeader.label = updateLabel.c_str();
    manager->appendHeader(updateHeader);
    
    OP_StringParameter updateURLParam;
    updateURLParam.name = "Updateurl";
    updateURLParam.label = "Copy this → ";
    updateURLParam.page = "About";
    updateURLParam.defaultValue = "github.com/stosumarte/FreenectTD/releases/latest";
    manager->appendString(updateURLParam);

}

// TD - Cook every frame
void FreenectTOP::getGeneralInfo(TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs*, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

// TD - Error string handling
void FreenectTOP::getErrorString(TD::OP_String* error, void* reserved1) {
    if (!errorString.empty())
        error->setString(errorString.c_str());
}

// Constructor for FreenectTOP
FreenectTOP::FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context)
    : fntdNodeInfo(info),
      fntdContext(context)
{
    // Do not initialize device here, will be done in execute
}

// Destructor for FreenectTOP
FreenectTOP::~FreenectTOP() {
    LOG("[FreenectTOP] Destructor called, cleaning up devices");
    LOG("[FreenectTOP] Calling fn2_cleanupDevice");
    fn2_cleanupDevice();
    LOG("[FreenectTOP] Calling fn1_cleanupDevice");
    fn1_cleanupDevice();
    fallbackBuffer = nullptr; // Release fallback buffer
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::fn1_initDevice() {
    
    LOG("[FreenectTOP] fn1_initDevice: start");
    PROFILE("fn1_initDevice: start");
    LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
    
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&fn1_ctx, nullptr) < 0) {
        PROFILE("fn1_initDevice: freenect_init failed");
        LOG("[FreenectTOP] freenect_init failed");
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
        PROFILE("fn1_initDevice: end");
        LOG("[FreenectTOP] fn1_initDevice: end (fail)");
        return false;
    }
    
    // Set libfreenect log level based on FNTD_DEBUG macro
    if (FNTD_DEBUG == 1) {
        freenect_set_log_level(fn1_ctx, FREENECT_LOG_WARNING);
    }
    
    // Upload firmware to Kinect v1 if needed (models 1473 and Kinect for Windows)
    freenect_set_fw_address_nui
        (fn1_ctx, ofxKinectExtras::getFWData1473(), ofxKinectExtras::getFWSize1473());
    freenect_set_fw_address_k4w
        (fn1_ctx, ofxKinectExtras::getFWDatak4w(), ofxKinectExtras::getFWSizek4w());

    int numDevices = freenect_num_devices(fn1_ctx);
    
    PROFILE(std::string("fn1_initDevice: numDevices=") + std::to_string(numDevices));
    LOG("[FreenectTOP] fn1_initDevice: numDevices = " + std::to_string(numDevices));
    
    if (numDevices <= 0) {
        LOG("[FreenectTOP] No Kinect v1 devices found");
        errorString.clear();
        errorString = "No Kinect v1 devices found";
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx shutdown, now = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
        PROFILE("fn1_initDevice: end");
        LOG("[FreenectTOP] fn1_initDevice: end (no devices)");
        return false;
    }

    try {
        fn1_rgbReady = false;
        fn1_depthReady = false;
        LOG(std::string("[FreenectTOP] fn1_initDevice: device before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_device)));
        fn1_device = new MyFreenectDevice(fn1_ctx, 0, fn1_rgbReady, fn1_depthReady);
        PROFILE("fn1_initDevice: device created");
        LOG(std::string("[FreenectTOP] fn1_initDevice: device after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_device)));
        fn1_device->startVideo();
        fn1_device->startDepth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        fn1_runEvents = true;
        LOG("[FreenectTOP] fn1_initDevice: fn1_runEvents set to true");
        PROFILE("fn1_initDevice: starting fn1_eventThread");
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_eventThread joinable before = ") + std::to_string(fn1_eventThread.joinable()));
        fn1_eventThread = std::thread([this]() {
            PROFILE("fn1_eventThread: started");
            LOG("[FreenectTOP] fn1_eventThread: running");
            while (fn1_runEvents.load()) {
                PROFILE("fn1_eventThread: iteration start");
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (!fn1_ctx) break;
                int err = freenect_process_events(fn1_ctx);
                if (err < 0) {
                    LOG("[FreenectTOP] Error in freenect_process_events");
                    break;
                }
                PROFILE("fn1_eventThread: iteration end");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            PROFILE("fn1_eventThread: exiting");
            LOG("[FreenectTOP] fn1_eventThread: exiting");
        });
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_eventThread joinable after = ") + std::to_string(fn1_eventThread.joinable()));
        PROFILE("fn1_initDevice: fn1_eventThread started");
    } catch (...) {
        LOG("[FreenectTOP] Failed to start device");
        errorString.clear();
        errorString = "Failed to start Kinect v1 device";
        fn1_cleanupDevice();
        PROFILE("fn1_initDevice: end");
        LOG("[FreenectTOP] fn1_initDevice: end (exception)");
        return false;
    }
    PROFILE("fn1_initDevice: end");
    LOG("[FreenectTOP] fn1_initDevice: end (success)");
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
void FreenectTOP::fn1_cleanupDevice() {
    LOG("[FreenectTOP] fn1_cleanupDevice: start");
    PROFILE("fn1_cleanupDevice: start");
    fn1_runEvents = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (fn1_eventThread.joinable()) {
        PROFILE("fn1_cleanupDevice: joining fn1_eventThread");
        fn1_eventThread.join();
        LOG("[FreenectTOP] fn1_eventThread joined");
    }
    waitV1InitThread(); // Ensure init thread is joined
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (fn1_device) {
        delete fn1_device;
        fn1_device = nullptr;
        LOG("[FreenectTOP] device deleted (v1)");
    }
    if (fn1_ctx) {
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        LOG("[FreenectTOP] fn1_ctx shutdown (v1)");
    }
    fn1InitInProgress = false;
    fn1InitSuccess = false;
    PROFILE("fn1_cleanupDevice: end");
    LOG("[FreenectTOP] fn1_cleanupDevice: end");
}

// Add static atomic flag and thread for v2 device search
static std::atomic<bool> v2DeviceAvailable(false);
static std::thread v2EnumThread;
static std::atomic<bool> v2EnumThreadRunning(false);

// Start the background enumeration thread for Kinect v2
void FreenectTOP::fn2_startEnumThread() {
    LOG("[FreenectTOP] fn2_startEnumThread: start");
    PROFILE("fn2_startEnumThread: start");
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    if (v2EnumThreadRunning.load()) {
        PROFILE("fn2_startEnumThread: already running");
        LOG("[FreenectTOP] fn2_startEnumThread: already running");
        PROFILE("fn2_startEnumThread: end");
        LOG("[FreenectTOP] fn2_startEnumThread: end (already running)");
        return;
    }
    v2EnumThreadRunning = true;
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    PROFILE("fn2_startEnumThread: launching thread");
    v2EnumThread = std::thread([]() {
        while (v2EnumThreadRunning.load()) {
            libfreenect2::Freenect2 ctx;
            v2DeviceAvailable = (ctx.enumerateDevices() > 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThread joinable after = " + std::to_string(v2EnumThread.joinable()));
    PROFILE("fn2_startEnumThread: thread launched");
    PROFILE("fn2_startEnumThread: end");
    LOG("[FreenectTOP] fn2_startEnumThread: end");
}

// Stop the background enumeration thread for Kinect v2
void FreenectTOP::fn2_stopEnumThread() {
    LOG("[FreenectTOP] fn2_stopEnumThread: start");
    PROFILE("fn2_stopEnumThread: start");
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    v2EnumThreadRunning = false;
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThread joinable = " + std::to_string(v2EnumThread.joinable()));
    if (v2EnumThread.joinable()) {
        PROFILE("fn2_stopEnumThread: joining thread");
        v2EnumThread.join();
        LOG("[FreenectTOP] fn2_stopEnumThread: thread joined");
    }
    PROFILE("fn2_stopEnumThread: end");
    LOG("[FreenectTOP] fn2_stopEnumThread: end");
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::fn2_initDevice() {
    LOG("[FreenectTOP] fn2_initDevice: start");
    PROFILE("fn2_initDevice: start");
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    std::lock_guard<std::mutex> lock(freenectMutex);
    fn2_startEnumThread();
    if (!v2DeviceAvailable.load()) {
        PROFILE("fn2_initDevice: no device available");
        LOG("[FreenectTOP] fn2_initDevice: no device available");
        PROFILE("fn2_initDevice: end");
        LOG("[FreenectTOP] fn2_initDevice: end (no device)");
        return false;
    }
    if (fn2_ctx) {
        PROFILE("fn2_initDevice: already initialized");
        LOG("[FreenectTOP] fn2_initDevice: already initialized");
        PROFILE("fn2_initDevice: end");
        LOG("[FreenectTOP] fn2_initDevice: end (already initialized)");
        return true;
    }
    fn2_ctx = new libfreenect2::Freenect2();
    PROFILE("fn2_initDevice: fn2_ctx created");
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx->enumerateDevices() == 0) {
        LOG("[FreenectTOP] No Kinect v2 devices found");
        errorString.clear();
        errorString = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: fn2_ctx deleted and set to nullptr");
        PROFILE("fn2_initDevice: end");
        LOG("[FreenectTOP] fn2_initDevice: end (no devices)");
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
        LOG("[FreenectTOP] Using CPU pipeline for Kinect v2");
        PROFILE("fn2_initDevice: CPU pipeline created");
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_pipeline after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    } catch (...) {
        errorString.clear();
        errorString = "Couldn't create CPU pipeline for Kinect v2";
        LOG("Couldn't create CPU pipeline for Kinect v2");
        PROFILE("fn2_initDevice: CPU pipeline creation failed");
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_pipeline after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    }
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    LOG(std::string("[FreenectTOP] fn2_initDevice: openDevice returned dev = ") + std::to_string(reinterpret_cast<uintptr_t>(dev)));
    if (!dev) {
        LOG("[FreenectTOP] Failed to open Kinect v2 device");
        errorString.clear();
        errorString = "Failed to open Kinect v2 device";
        delete fn2_device;
        LOG("[FreenectTOP] fn2_initDevice: fn2_device deleted");
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
            LOG("[FreenectTOP] fn2_initDevice: fn2_pipeline deleted and set to nullptr");
        }
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
            LOG("[FreenectTOP] fn2_initDevice: fn2_ctx deleted and set to nullptr");
        }
        fn2_device = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: fn2_device set to nullptr");
        PROFILE("fn2_initDevice: end");
        LOG("[FreenectTOP] fn2_initDevice: end (openDevice fail)");
        return false;
    }
    if (!fn2_device) {
        fn2_device = new MyFreenect2Device(dev, fn2_rgbReady, fn2_depthReady);
        PROFILE("fn2_initDevice: fn2_device created");
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_device after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_device)));
    }
    if (!fn2_device->start()) {
        LOG("[FreenectTOP] Failed to start Kinect v2 device");
        errorString.clear();
        errorString = "Failed to start Kinect v2 device";
        delete fn2_device;
        LOG("[FreenectTOP] fn2_initDevice: fn2_device deleted");
        if (fn2_pipeline) {
            delete fn2_pipeline;
            fn2_pipeline = nullptr;
            LOG("[FreenectTOP] fn2_initDevice: fn2_pipeline deleted and set to nullptr");
        }
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
            LOG("[FreenectTOP] fn2_initDevice: fn2_ctx deleted and set to nullptr");
        }
        fn2_device = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: fn2_device set to nullptr");
        PROFILE("fn2_initDevice: end");
        LOG("[FreenectTOP] fn2_initDevice: end (start fail)");
        return false;
    }
    // **Stop enumeration thread after successful device start**
    fn2_stopEnumThread();
    PROFILE("fn2_initDevice: device started and enum thread stopped");
    LOG("[FreenectTOP] fn2_initDevice: device started and enum thread stopped");
    // Start event thread for v2
    LOG("[FreenectTOP] fn2_initDevice: fn2_eventThread joinable before = " + std::to_string(fn2_eventThread.joinable()));
    if (!fn2_eventThread.joinable()) {
        fn2_runEvents = true;
        LOG("[FreenectTOP] fn2_initDevice: fn2_runEvents set to true");
        PROFILE("fn2_initDevice: starting fn2_eventThread");
        fn2_eventThread = std::thread([this]() {
            PROFILE("fn2_eventThread: started");
            LOG("[FreenectTOP] fn2_eventThread: running");
            while (fn2_runEvents.load()) {
                PROFILE("fn2_eventThread: iteration start");
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (fn2_device) {
                    fn2_device->processFrames();
                } else {
                    LOG("[FreenectTOP] fn2_device is null in event thread");
                }
                PROFILE("fn2_eventThread: iteration end");
            }
            PROFILE("fn2_eventThread: exiting");
            LOG("[FreenectTOP] fn2_eventThread: exiting");
        });
        LOG("[FreenectTOP] fn2_initDevice: fn2_eventThread joinable after = " + std::to_string(fn2_eventThread.joinable()));
        PROFILE("fn2_initDevice: fn2_eventThread started");
    }
    PROFILE("fn2_initDevice: end");
    LOG("[FreenectTOP] fn2_initDevice: end (success)");
    return true;
}

// Threaded initialization for Kinect v1
void FreenectTOP::startV1InitThread() {
    if (fn1InitInProgress.load()) return; // Already running
    fn1InitInProgress = true;
    fn1InitThread = std::thread([this]() {
        bool result = this->fn1_initDevice();
        fn1InitSuccess = result;
        fn1InitInProgress = false;
    });
    fn1InitThread.detach(); // Or join later as needed
}

void FreenectTOP::waitV1InitThread() {
    if (fn1InitThread.joinable()) {
        fn1InitThread.join();
    }
}

// Threaded initialization for Kinect v2
void FreenectTOP::fn2_startInitThread() {
    if (fn2_InitInProgress.load()) return; // Already running
    fn2_InitInProgress = true;
    fn2_InitThread = std::thread([this]() {
        bool result = this->fn2_initDevice();
        fn2_InitSuccess = result;
        fn2_InitInProgress = false;
    });
    fn2_InitThread.detach(); // Or join later as needed
}

void FreenectTOP::fn2_waitInitThread() {
    if (fn2_InitThread.joinable()) {
        fn2_InitThread.join();
    }
}

// Cleanup for Kinect v2 (libfreenect2)
void FreenectTOP::fn2_cleanupDevice() {
    LOG("[FreenectTOP] fn2_cleanupDevice: start");
    PROFILE("fn2_cleanupDevice: start");
    fn2_runEvents = false;
    // Wait a short time for the event thread to exit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (fn2_eventThread.joinable()) {
        PROFILE("fn2_cleanupDevice: joining fn2_eventThread");
        fn2_eventThread.join();
        LOG("[FreenectTOP] fn2_eventThread joined");
    }
    fn2_stopEnumThread();
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (fn2_device) {
        delete fn2_device;
        fn2_device = nullptr;
        LOG("[FreenectTOP] fn2_device deleted");
    }
    if (fn2_pipeline) {
        fn2_pipeline = nullptr;
        LOG("[FreenectTOP] fn2_pipeline set to nullptr");
    }
    if (fn2_ctx) {
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_ctx deleted");
    }
    PROFILE("fn2_cleanupDevice: end");
    LOG("[FreenectTOP] fn2_cleanupDevice: end");
}

// Execute method for Kinect v1 (libfreenect)
void FreenectTOP::executeV1(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    int colorWidth = MyFreenectDevice::WIDTH, colorHeight = MyFreenectDevice::HEIGHT, depthWidth = MyFreenectDevice::WIDTH, depthHeight = MyFreenectDevice::HEIGHT;
    if (!fn1_device) {
        LOG("[FreenectTOP] executeV1: device is null, initializing device in thread");
        startV1InitThread();
        if (!fn1InitSuccess.load()) {
            LOG("[FreenectTOP] executeV1: fn1_initDevice not finished or failed, uploading fallback black buffer");
            errorString.clear();
            errorString = "No Kinect v1 devices found";
            uploadFallbackBuffer();
            return;
        }
    }
    if (!fn1_device) {
        LOG("[FreenectTOP] ERROR: device is null after init! Uploading fallback black buffer");
        errorString = "Device is null after initialization";
        uploadFallbackBuffer();
        return;
    }
    // If device is not available, do not proceed further
    if (!fn1_device) {
        LOG("[FreenectTOP] executeV1: device is still null, aborting frame");
        return;
    }
    float tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    try {
        fn1_device->setTiltDegrees(tilt);
    } catch (const std::exception& e) {
        LOG(std::string("[FreenectTOP] Failed to set tilt: ") + e.what());
        errorString = "Failed to set tilt angle: " + std::string(e.what());
        LOG("[FreenectTOP] executeV1: cleaning up device due to tilt error");
        fn1_cleanupDevice();
        fn1_device = nullptr;
        return;
    }
    std::vector<uint8_t> colorFrame;
    if (fn1_device->getColorFrame(colorFrame)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating color output buffer");
        TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(colorWidth * colorHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
    if (fn1_device->getDepthFrame(depthFrame)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating depth output buffer");
        TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(depthWidth * depthHeight * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
        fn2_startInitThread();
        if (!fn2_InitSuccess.load()) {
               LOG("[FreenectTOP] executeV2: fn2_initDevice failed, returning early");
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
        TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(outW * outH * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
    // --- Output depth according to Depthformat and Depthdistortion parameter ---
    std::string depthDistortStr = inputs->getParString("Depthdistortion") ? inputs->getParString("Depthdistortion") : "Distorted";
    std::string depthFormatStr = inputs->getParString("Depthformat") ? inputs->getParString("Depthformat") : "Raw (16-bit)";
    int outDW = depthWidth, outDH = depthHeight;
    std::vector<uint16_t> depthFrame;
    bool gotDepth = false;
    if (depthDistortStr == "Undistorted") {
        // Get undistorted depth using MyFreenect2Device method
        if (fn2_device) {
            gotDepth = fn2_device->getUndistortedDepthFrame(depthFrame);
        }
    } else {
        // Distorted: use raw depth
        gotDepth = fn2_device->getDepthFrame(depthFrame);
    }
    if (gotDepth) {
        errorString.clear();
        if (depthFormatStr == "Raw (16-bit)") {
            LOG("[FreenectTOP] executeV2: outputting raw depth");
            TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(outDW * outDH * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
        } else if (depthFormatStr == "Visualized (8-bit)") {
            LOG("[FreenectTOP] executeV2: outputting visualized depth as RGB");
            std::vector<uint8_t> visualized(outDW * outDH * 4);
            uint16_t minDepth = 65535, maxDepth = 0;
            for (size_t i = 0; i < depthFrame.size(); ++i) {
                uint16_t d = depthFrame[i];
                if (d > 0 && d < minDepth) minDepth = d;
                if (d > maxDepth) maxDepth = d;
            }
            if (minDepth == 65535 || maxDepth == 0 || maxDepth - minDepth < 100) {
                minDepth = 500;
                maxDepth = 4500;
            }
            for (int y = 0; y < outDH; ++y) {
                for (int x = 0; x < outDW; ++x) {
                    int i = (y * outDW + x);
                    uint16_t d = depthFrame[i];
                    float norm = (float)(d - minDepth) / (maxDepth - minDepth);
                    if (norm < 0.0f) norm = 0.0f;
                    if (norm > 1.0f) norm = 1.0f;
                    uint8_t r, g, b;
                    if (norm < 0.5f) {
                        float t = norm / 0.5f;
                        r = 0;
                        g = static_cast<uint8_t>(t * 255);
                        b = static_cast<uint8_t>((1.0f - t) * 255);
                    } else {
                        float t = (norm - 0.5f) / 0.5f;
                        r = static_cast<uint8_t>(t * 255);
                        g = static_cast<uint8_t>((1.0f - t) * 255);
                        b = 0;
                    }
                    int vi = i * 4;
                    visualized[vi + 0] = r;
                    visualized[vi + 1] = g;
                    visualized[vi + 2] = b;
                    visualized[vi + 3] = 255;
                }
            }
            TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext
                ? fntdContext->createOutputBuffer(outDW * outDH * 4, TD::TOP_BufferFlags::None, nullptr)
                : nullptr;
            if (buf) {
                std::memcpy(buf->data, visualized.data(), outDW * outDH * 4);
                TD::TOP_UploadInfo info;
                info.textureDesc.width = outDW;
                info.textureDesc.height = outDH;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
                info.colorBufferIndex = 1;
                LOG("[FreenectTOP] executeV2: uploading visualized depth buffer");
                output->uploadBuffer(&buf, info, nullptr);
            } else {
                LOG("[FreenectTOP] executeV2: failed to create visualized depth buffer");
                errorString.clear();
                errorString = "Failed to create visualized depth buffer";
            }
        }
    }
}

// Enable/disable parameters based on device type
void dynamicParameterEnables(int deviceType, const TD::OP_Inputs* inputs) {
    if (deviceType == 0) {
        inputs->enablePar("Tilt", true);                // Enable Tilt for Kinect v1
        inputs->enablePar("Resolutionlimit", false);    // Disable Resolutionlimit for Kinect v1
        inputs->enablePar("Depthformat", false);        // Disable Depthformat for Kinect v1
        inputs->enablePar("Depthdistortion", false);    // Disable Depthdistortion for Kinect v1
    } else if (deviceType == 1) {
        inputs->enablePar("Tilt", false);               // Disable Tilt for Kinect v2
        inputs->enablePar("Resolutionlimit", true);     // Enable Resolutionlimit for Kinect v2
        inputs->enablePar("Depthformat", true);         // Enable Depthformat for Kinect v2
        inputs->enablePar("Depthdistortion", true);     // Enable Depthdistortion for Kinect v2
    }
}

// Main execution method
void FreenectTOP::execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) {
    myCurrentOutput = output;
    
    // Validate inputs
    if (!inputs) {
        LOG("[FreenectTOP] ERROR: inputs is null");
        errorString = "Inputs is null";
        return;
    }
    
    // Get device type from parameter
    const char* devTypeCStr = inputs->getParString("Hardwareversion");
    std::string deviceTypeStr = devTypeCStr ? devTypeCStr : "Kinect v1";
    int newDeviceType = (deviceTypeStr == "Kinect v2") ? 1 : 0;
    deviceType = newDeviceType;
    LOG("[FreenectTOP] execute: switching device type from " + lastDeviceTypeStr + " to " + deviceTypeStr);
    
    // Check if Active parameter is set
    bool isActive = (inputs && inputs->getParInt("Active") != 0);
    
    // Check if the plugin is active
    if (isActive == false) {
        // If not active, upload a black buffer and return, keep dynamic parameters updated
        LOG("[FreenectTOP] Not active, skipping device initialization and execution");
        uploadFallbackBuffer();
        dynamicParameterEnables(deviceType, inputs);
        errorString.clear();
        return;
    }
    
    else {
        
        dynamicParameterEnables(deviceType, inputs);
        
        
        // If device type changed, re-init device
        if (deviceTypeStr != lastDeviceTypeStr) {
            LOG("[FreenectTOP] execute: device type changed, cleaning up devices");
            fn2_cleanupDevice();
            fn1_cleanupDevice();
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
        
        // Handle Checkforupdates button (uncomment to enable)
        /*if (inputs->getParInt("Checkforupdates") == 1) {
         openWebpage("github.com/stosumarte/FreenectTD/releases");
         LOG("[FreenectTOP] Check for updates button pressed: opened update webpage");
         }*/
    }
}

// Upload a fallback black buffer
void FreenectTOP::uploadFallbackBuffer() {
    if (!myCurrentOutput) {
        LOG("[FreenectTOP] uploadFallbackBuffer: myCurrentOutput is null, cannot upload fallback buffer");
        return;
    }
    int fallbackWidth = 128, fallbackHeight = 128;
    if (!fallbackBuffer) {
        std::vector<uint8_t> black(fallbackWidth * fallbackHeight * 4, 0);
        fallbackBuffer = fntdContext ? fntdContext->createOutputBuffer(fallbackWidth * fallbackHeight * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
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
