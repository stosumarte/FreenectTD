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
#include <future>
#include <array>

#ifndef DLLEXPORT
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#ifndef FREENECTTOP_VERSION
#define FREENECTTOP_VERSION "dev"
#endif

#define FNTD_FALLBACK_BUFFER_ENABLED false

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
    
    // Depth visualization dropdown
    /*OP_StringParameter depthVisualizeParam;
    depthVisualizeParam.name = "Depthvisualize";
    depthVisualizeParam.label = "Visualize Depth As";
    depthVisualizeParam.page = "Freenect";
    depthVisualizeParam.defaultValue = "Mono 16-bit";
    const char* depthVisualizeNames[] = {"Mono 16-bit", "RGB 8-bit (Remapped)"};
    const char* depthVisualizeLabels[] = {"Mono 16-bit", "RGB 8-bit (Remapped)"};
    manager->appendMenu(depthVisualizeParam, 2, depthVisualizeNames, depthVisualizeLabels);*/
    
    // Depth format dropdown
    OP_StringParameter depthFormatParam;
    depthFormatParam.name = "Depthformat";
    depthFormatParam.label = "Depth Format";
    depthFormatParam.page = "Freenect";
    depthFormatParam.defaultValue = "Raw";
    const char* depthFormatNames[] = {"Raw", "Registered"};
    const char* depthFormatLabels[] = {"Raw", "Registered"};
    manager->appendMenu(depthFormatParam, 2, depthFormatNames, depthFormatLabels);
    
    // Undistort toggle
    OP_NumericParameter depthUndistortParam;
    depthUndistortParam.name = "Depthundistort";
    depthUndistortParam.label = "Depth Undistortion";
    depthUndistortParam.page = "Freenect";
    depthUndistortParam.defaultValues[0] = 0.0; // Default to disabled
    depthUndistortParam.minValues[0] = 0.0;
    depthUndistortParam.maxValues[0] = 1.0;
    depthUndistortParam.minSliders[0] = 0.0;
    depthUndistortParam.maxSliders[0] = 1.0;
    depthUndistortParam.clampMins[0] = true;
    depthUndistortParam.clampMaxes[0] = true;
    manager->appendToggle(depthUndistortParam);
    
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
    
    // Update URL (needs to be copied manually)
    OP_StringParameter updateURLParam;
    updateURLParam.name = "Updateurl";
    updateURLParam.label = "Copy this → ";
    updateURLParam.page = "About";
    updateURLParam.defaultValue = "github.com/stosumarte/FreenectTD/releases/latest";
    manager->appendString(updateURLParam);

}

// TD - Cook every frame
void FreenectTOP::getGeneralInfo(TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) {
    ginfo->cookEveryFrameIfAsked = true;
}

// TD - Error string handling
void FreenectTOP::getErrorString(TD::OP_String* error, void* reserved1) {
    if (!errorString.empty())
        error->setString(errorString.c_str());
}

// TD - Warning string handling
void FreenectTOP::getWarningString(TD::OP_String* warning, void* reserved1) {
    if (!warningString.empty())
        warning->setString(warningString.c_str());
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
    LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
    
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&fn1_ctx, nullptr) < 0) {
        LOG("[FreenectTOP] freenect_init failed");
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
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
    
    LOG("[FreenectTOP] fn1_initDevice: numDevices = " + std::to_string(numDevices));
    
    if (numDevices <= 0) {
        LOG("[FreenectTOP] No Kinect v1 devices found");
        errorString.clear();
        errorString = "No Kinect v1 devices found";
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_ctx shutdown, now = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_ctx)));
        LOG("[FreenectTOP] fn1_initDevice: end (no devices)");
        return false;
    }

    try {
        fn1_rgbReady = false;
        fn1_depthReady = false;
        LOG(std::string("[FreenectTOP] fn1_initDevice: device before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_device)));
        fn1_device = new MyFreenectDevice(fn1_ctx, 0, fn1_rgbReady, fn1_depthReady);
        LOG(std::string("[FreenectTOP] fn1_initDevice: device after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn1_device)));
        fn1_device->startVideo();
        fn1_device->startDepth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        fn1_runEvents = true;
        LOG("[FreenectTOP] fn1_initDevice: fn1_runEvents set to true");
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_eventThread joinable before = ") + std::to_string(fn1_eventThread.joinable()));
        fn1_eventThread = std::thread([this]() {
            LOG("[FreenectTOP] fn1_eventThread: running");
            while (fn1_runEvents.load()) {
                std::lock_guard<std::mutex> lock(fn1_eventMutex); // Use event mutex here
                if (!fn1_ctx) break;
                int err = freenect_process_events(fn1_ctx);
                if (err < 0) {
                    LOG("[FreenectTOP] Error in freenect_process_events");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            LOG("[FreenectTOP] fn1_eventThread: exiting");
        });
        LOG(std::string("[FreenectTOP] fn1_initDevice: fn1_eventThread joinable after = ") + std::to_string(fn1_eventThread.joinable()));
    } catch (...) {
        LOG("[FreenectTOP] Failed to start device");
        errorString.clear();
        errorString = "Failed to start Kinect v1 device";
        fn1_cleanupDevice();
        LOG("[FreenectTOP] fn1_initDevice: end (exception)");
        return false;
    }
    LOG("[FreenectTOP] fn1_initDevice: end (success)");
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
void FreenectTOP::fn1_cleanupDevice() {
    LOG("[FreenectTOP] fn1_cleanupDevice: start");
    fn1_runEvents = false;
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Try to join event thread with timeout
    if (fn1_eventThread.joinable()) {
        LOG("[FreenectTOP] fn1_cleanupDevice: attempting to join fn1_eventThread");
        fn1_eventThread.join();
        LOG("[FreenectTOP] fn1_eventThread joined successfully");
    } else {
        LOG("[FreenectTOP] fn1_cleanupDevice: fn1_eventThread not joinable");
    }
    
    // Try to join init thread with timeout
    if (fn1InitThread.joinable()) {
        LOG("[FreenectTOP] fn1_cleanupDevice: attempting to join fn1InitThread with timeout");
        fn1InitThread.join();
        LOG("[FreenectTOP] fn1InitThread joined successfully");
    } else {
        LOG("[FreenectTOP] fn1_cleanupDevice: fn1InitThread not joinable");
    }
    
    // Clean up device and context
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
    LOG("[FreenectTOP] fn1_cleanupDevice: end");
}

// Start the background enumeration thread for Kinect v2
void FreenectTOP::fn2_startEnumThread() {
    LOG("[FreenectTOP] fn2_startEnumThread: start");
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    if (v2EnumThreadRunning.load()) {
        LOG("[FreenectTOP] fn2_startEnumThread: end, already running");
        return;
    }
    v2EnumThreadRunning = true;
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    v2EnumThread = std::thread([this]() {
        while (v2EnumThreadRunning.load()) {
            libfreenect2::Freenect2 ctx;
            v2DeviceAvailable = (ctx.enumerateDevices() > 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    LOG("[FreenectTOP] fn2_startEnumThread: v2EnumThread joinable after = " + std::to_string(v2EnumThread.joinable()));
    LOG("[FreenectTOP] fn2_startEnumThread: end");
}

// Stop the background enumeration thread for Kinect v2
void FreenectTOP::fn2_stopEnumThread() {
    LOG("[FreenectTOP] fn2_stopEnumThread: start");
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThreadRunning before = " + std::to_string(v2EnumThreadRunning.load()));
    v2EnumThreadRunning = false;
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThreadRunning after = " + std::to_string(v2EnumThreadRunning.load()));
    LOG("[FreenectTOP] fn2_stopEnumThread: v2EnumThread joinable = " + std::to_string(v2EnumThread.joinable()));
    
    if (v2EnumThread.joinable()) {
        LOG("[FreenectTOP] fn2_stopEnumThread: attempting to join v2EnumThread");
        v2EnumThread.join();
        LOG("[FreenectTOP] v2EnumThread joined successfully");
    }
    LOG("[FreenectTOP] fn2_stopEnumThread: end");
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::fn2_initDevice() {
    LOG("[FreenectTOP] fn2_initDevice: start");
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx before = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    std::lock_guard<std::mutex> lock(freenectMutex);
    fn2_startEnumThread();
    if (!v2DeviceAvailable.load()) {
        LOG("[FreenectTOP] fn2_initDevice: no device available");
        LOG("[FreenectTOP] fn2_initDevice: end (no device)");
        return false;
    }
    if (fn2_ctx) {
        LOG("[FreenectTOP] fn2_initDevice: already initialized");
        LOG("[FreenectTOP] fn2_initDevice: end (already initialized)");
        return true;
    }
    fn2_ctx = new libfreenect2::Freenect2();
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx->enumerateDevices() == 0) {
        LOG("[FreenectTOP] No Kinect v2 devices found");
        errorString.clear();
        errorString = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: fn2_ctx deleted and set to nullptr");
        LOG("[FreenectTOP] fn2_initDevice: end (no devices)");
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
        LOG("[FreenectTOP] Using CPU pipeline for Kinect v2");
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_pipeline after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    } catch (...) {
        errorString.clear();
        errorString = "Couldn't create CPU pipeline for Kinect v2";
        LOG("Couldn't create CPU pipeline for Kinect v2");
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
        LOG("[FreenectTOP] fn2_initDevice: end (openDevice fail)");
        return false;
    }
    if (!fn2_device) {
        fn2_device = new MyFreenect2Device(dev, fn2_rgbReady, fn2_depthReady, fn2_irReady);
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
        LOG("[FreenectTOP] fn2_initDevice: end (start fail)");
        return false;
    }
    // **Stop enumeration thread after successful device start**
    fn2_stopEnumThread();
    LOG("[FreenectTOP] fn2_initDevice: device started and enum thread stopped");
    // Start event thread for v2
    LOG("[FreenectTOP] fn2_initDevice: fn2_eventThread joinable before = " + std::to_string(fn2_eventThread.joinable()));
    if (!fn2_eventThread.joinable()) {
        fn2_runEvents = true;
        LOG("[FreenectTOP] fn2_initDevice: fn2_runEvents set to true");
        fn2_eventThread = std::thread([this]() {
            LOG("[FreenectTOP] fn2_eventThread: running");
            while (fn2_runEvents.load()) {
                std::lock_guard<std::mutex> lock(freenectMutex);
                if (fn2_device) {
                    fn2_device->processFrames();
                } else {
                    LOG("[FreenectTOP] fn2_device is null in event thread");
                }
            }
            LOG("[FreenectTOP] fn2_eventThread: exiting");
        });
        LOG("[FreenectTOP] fn2_initDevice: fn2_eventThread joinable after = " + std::to_string(fn2_eventThread.joinable()));
    }
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
    //fn1InitThread.detach(); // Or join later as needed
    if (fn1InitThread.joinable()) {
        fn1InitThread.join();
    } else {
        LOG("[FreenectTOP] startV1InitThread: fn1InitThread not joinable after creation");
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
    //fn2_InitThread.detach(); // Or join later as needed
    if (fn2_InitThread.joinable()) {
        fn2_InitThread.join();
    } else {
        LOG("[FreenectTOP] fn2_startInitThread: fn2_InitThread not joinable after creation");
    }
}

// Cleanup for Kinect v2 (libfreenect2)
void FreenectTOP::fn2_cleanupDevice() {
    LOG("[FreenectTOP] fn2_cleanupDevice: start");
    fn2_runEvents = false;
    // Wait a short time for the event thread to exit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Try to join event thread
    if (fn2_eventThread.joinable()) {
        fn2_eventThread.join();
    } else {
        LOG("[FreenectTOP] fn2_cleanupDevice: couldn't join fn2_eventThread");
    }
    
    // Try to join init thread
    if (fn2_InitThread.joinable()) {
        fn2_InitThread.join();
    } else {
        LOG("[FreenectTOP] fn2_cleanupDevice: couldn't join fn2_InitThread");
    }
    
    fn2_stopEnumThread();
    
    // Clean up device and context
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
    fn2_InitInProgress = false;
    fn2_InitSuccess = false;
    LOG("[FreenectTOP] fn2_cleanupDevice: end");
}

// Execute method for Kinect v1 (libfreenect)
void FreenectTOP::executeV1(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    std::string depthFormat = (inputs->getParString("Depthformat"));
    
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
    
    // --- Set tilt angle ---
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
    
    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    int outCW, outCH;
    
    if (fn1_device->getColorFrame(colorFrame, outCW, outCH)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating color output buffer");
        TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(outCW * outCH * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            LOG("[FreenectTOP] executeV1: copying color frame data to buffer");
            std::memcpy(buf->data, colorFrame.data(), outCW * outCH * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = outCW;
            info.textureDesc.height = outCH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            LOG("[FreenectTOP] executeV1: uploading color buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create color output buffer");
        }
    }
    
    // --- Depth frame ---
    std::vector<uint16_t> depthFrame;
    int outDW, outDH;
    fn1_depthType depthType = fn1_depthType::Raw; // Default to Raw
    
    if (depthFormat == "Raw") {
        depthType = fn1_depthType::Raw;
    } else if (depthFormat == "Registered") {
        depthType = fn1_depthType::Registered;
    } else {
        // "undistorted" gets ignored for Kinect v1, default to Raw
        depthType = fn1_depthType::Raw;
    }

    if (fn1_device->getDepthFrame(depthFrame, depthType, outDW, outDH)) {
        errorString.clear();
        LOG("[FreenectTOP] executeV1: creating depth output buffer");

        TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext
            ? fntdContext->createOutputBuffer(outDW * outDH * 2, TD::TOP_BufferFlags::None, nullptr)
            : nullptr;

        if (buf) {
            LOG("[FreenectTOP] executeV1: copying depth frame data to buffer");
            std::memcpy(buf->data, depthFrame.data(), outDW * outDH * 2);

            TD::TOP_UploadInfo info;
            info.textureDesc.width = outDW;
            info.textureDesc.height = outDH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 1;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;

            LOG("[FreenectTOP] executeV1: uploading depth buffer");
            output->uploadBuffer(&buf, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create depth output buffer");
        }
    }
}

// Execute method for Kinect v2 (libfreenect2)
void FreenectTOP::executeV2(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {

    bool downscale = (inputs->getParInt("Resolutionlimit") != 0);

    if (!fn2_device) {
        LOG("[FreenectTOP] executeV2: fn2_device is null, initializing device");
        fn2_startInitThread();
        if (!fn2_InitSuccess.load()) {
            errorString = "No Kinect v2 devices found";
            uploadFallbackBuffer();
            return;
        }
    }

    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    int outCW, outCH;
    
    if (fn2_device->getColorFrame(colorFrame, downscale, outCW, outCH)) {
        errorString.clear();
        TD::OP_SmartRef<TD::TOP_Buffer> buf =
            fntdContext ? fntdContext->createOutputBuffer(outCW * outCH * 4, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            std::memcpy(buf->data, colorFrame.data(), outCW * outCH * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = outCW;
            info.textureDesc.height = outCH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&buf, info, nullptr);
        }
    }

    // --- Depth frame ---
    std::vector<uint16_t> depthFrame;
    int outDW, outDH;
    
    // Map string to DepthType enum
    std::string depthFormat = inputs->getParString("Depthformat");
    bool depthUndistort = (inputs->getParInt("Depthundistort") != 0);
    fn2_depthType depthTypeEnum = fn2_depthType::Raw; // Default
    if (depthFormat == "Raw") {
        depthTypeEnum = depthUndistort ? fn2_depthType::Undistorted : fn2_depthType::Raw;
    } else if (depthFormat == "Registered") {
        depthTypeEnum = fn2_depthType::Registered;
    }
    
    if (fn2_device->getDepthFrame(depthFrame, depthTypeEnum, downscale, outDW, outDH)) {
        errorString.clear();
        TD::OP_SmartRef<TD::TOP_Buffer> buf =
            fntdContext ? fntdContext->createOutputBuffer(outDW * outDH * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            std::memcpy(buf->data, depthFrame.data(), outDW * outDH * 2);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = outDW;
            info.textureDesc.height = outDH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 1;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&buf, info, nullptr);
        }
    }

    // --- IR frame ---
    std::vector<uint16_t> irFrame;
    int outIRW, outIRH;
    
    if (fn2_device->getIRFrame(irFrame, outIRW, outIRH)) {
        errorString.clear();
        TD::OP_SmartRef<TD::TOP_Buffer> buf =
            fntdContext ? fntdContext->createOutputBuffer(outIRW * outIRH * 2, TD::TOP_BufferFlags::None, nullptr) : nullptr;
        if (buf) {
            std::memcpy(buf->data, irFrame.data(), outIRW * outIRH * 2);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = outIRW;
            info.textureDesc.height = outIRH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 2;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&buf, info, nullptr);
        }
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
    
    // Get device type parameter as string
    const char* devTypeCStr = inputs->getParString("Hardwareversion");
    std::string devType = devTypeCStr ? devTypeCStr : "Kinect v1";
    static std::string lastDeviceType = "Kinect v1";
    
    // Get depth format parameter as string
    std::string depthFormat = inputs->getParString("Depthformat");
    
    // Check if Active parameter is set
    bool isActive = (inputs && inputs->getParInt("Active") != 0);
    
    // Enable/disable parameters based on device type string
    if (devType == "Kinect v2") {
        inputs->enablePar("Tilt", false);
        inputs->enablePar("Resolutionlimit", true);
        //inputs->enablePar("Depthvisualize", true);
        inputs->enablePar("Depthformat", true);
    } else if (devType == "Kinect v1") {
        inputs->enablePar("Tilt", true);
        inputs->enablePar("Resolutionlimit", false);
        //inputs->enablePar("Depthvisualize", false);
        inputs->enablePar("Depthformat", true);
    } else {
        // Unknown device type, enable all parameters
        inputs->enablePar("Tilt", true);
        inputs->enablePar("Resolutionlimit", true);
        //inputs->enablePar("Depthvisualize", true);
        inputs->enablePar("Depthformat", true);
    }
    
    // Enable/disable depthUndistort based on device type and depthFormat
    if (devType == "Kinect v2" && depthFormat == "Raw") {
        inputs->enablePar("Depthundistort", true);
    } else {
        inputs->enablePar("Depthundistort", false);
    }
        
    
    // Check if the plugin is active
    if (!isActive) {
        warningString = "FreenectTOP is inactive";
        uploadFallbackBuffer();
        errorString.clear();
        return;
    } else {
        warningString.clear();
    }
    
    // Check if device type changed - only clean up and log if it actually changed
    if (devType != lastDeviceType) {
        LOG("[FreenectTOP] Device type changed from " + lastDeviceType + " to " + devType + ", cleaning up devices");
        fn1_cleanupDevice();
        fn2_cleanupDevice();
        lastDeviceType = devType;
    }
    
    // Execute based on current device type string
    if (devType == "Kinect v2") {
        executeV2(output, inputs);
    } else {
        executeV1(output, inputs);
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

        if (!fallbackBuffer0) fallbackBuffer0 = fallbackBuffer;
        if (!fallbackBuffer1) fallbackBuffer1 = fallbackBuffer;
        if (!fallbackBuffer2) fallbackBuffer2 = fallbackBuffer;

        std::array<TD::OP_SmartRef<TD::TOP_Buffer>, 3> fallbackBuffers = {
            fallbackBuffer0,
            fallbackBuffer1,
            fallbackBuffer2
        };

        for (int i = 0; i < 3; i++) {
            info.colorBufferIndex = i;
            myCurrentOutput->uploadBuffer(&fallbackBuffers[i], info, nullptr);
        }
    }
}


