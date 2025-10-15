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

// TouchDesigner Entrypoints
extern "C" {

    using namespace TD;

    DLLEXPORT void FillTOPPluginInfo(TOP_PluginInfo* info) {
        #if TD_VERSION != 2025
            info->apiVersion = TOPCPlusPlusAPIVersion;
        #elif TD_VERSION == 2025
            (void)info->setAPIVersion(TOPCPlusPlusAPIVersion);
        #endif
        info->executeMode = TOP_ExecuteMode::CPUMem;
        info->customOPInfo.opType->setString("Freenect");
        info->customOPInfo.opLabel->setString("Freenect");
        info->customOPInfo.opIcon->setString("FNT");
        info->customOPInfo.authorName->setString("Marte Tagliabue");
        info->customOPInfo.authorEmail->setString("ciao@marte.ee");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 0;
        info->customOPInfo.majorVersion = 1;
        info->customOPInfo.minorVersion = 0;
        #if TD_VERSION == 2025
            info->customOPInfo.opHelpURL->setString("https://github.com/stosumarte/FreenectTD");
        #endif
        
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
    enableDepthParam.label = "Enable Depth";
    enableDepthParam.page = "Freenect";
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
    enableIRParam.label = "Enable IR";
    enableIRParam.page = "Freenect";
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
    /*OP_NumericParameter resLimitParam;
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
    manager->appendToggle(resLimitParam);*/
    
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
    
    // ---------------
    // RESOLUTION PAGE
    // ---------------
    
    // V1 header
    OP_StringParameter fn1_resHeader;
    fn1_resHeader.name = "Kinectv1resolution";
    fn1_resHeader.page = "Resolution";
    fn1_resHeader.label = "Kinect V1";
    manager->appendHeader(fn1_resHeader);
    
    // V1 RGB resolution
    OP_NumericParameter fn1_rgbResParam;
    fn1_rgbResParam.name = "V1rgbresolution";
    fn1_rgbResParam.label = "RGB Resolution";
    fn1_rgbResParam.page = "Resolution";
    fn1_rgbResParam.defaultValues[0] = MyFreenectDevice::WIDTH;
    fn1_rgbResParam.defaultValues[1] = MyFreenectDevice::HEIGHT;
    fn1_rgbResParam.minValues[0] = fn1_rgbResParam.minSliders[0] = 1.0;
    fn1_rgbResParam.maxValues[0] = fn1_rgbResParam.maxSliders[0] = MyFreenectDevice::WIDTH;
    fn1_rgbResParam.clampMins[0] = fn1_rgbResParam.clampMaxes[0] = true;
    fn1_rgbResParam.minValues[1] = fn1_rgbResParam.minSliders[1] = 1.0;
    fn1_rgbResParam.maxValues[1] = fn1_rgbResParam.maxSliders[1] = MyFreenectDevice::HEIGHT;
    fn1_rgbResParam.clampMins[1] = fn1_rgbResParam.clampMaxes[1] = true;
    manager->appendXY(fn1_rgbResParam);
    
    // V1 depth resolution
    OP_NumericParameter fn1_depthResParam;
    fn1_depthResParam.name = "V1depthresolution";
    fn1_depthResParam.label = "Depth Resolution";
    fn1_depthResParam.page = "Resolution";
    fn1_depthResParam.defaultValues[0] = MyFreenectDevice::WIDTH;
    fn1_depthResParam.defaultValues[1] = MyFreenectDevice::HEIGHT;
    fn1_depthResParam.minValues[0] = fn1_depthResParam.minSliders[0] = 1.0;
    fn1_depthResParam.maxValues[0] = fn1_depthResParam.maxSliders[0] = MyFreenectDevice::WIDTH;
    fn1_depthResParam.clampMins[0] = fn1_depthResParam.clampMaxes[0] = true;
    fn1_depthResParam.minValues[1] = fn1_depthResParam.minSliders[1] = 1.0;
    fn1_depthResParam.maxValues[1] = fn1_depthResParam.maxSliders[1] = MyFreenectDevice::HEIGHT;
    fn1_depthResParam.clampMins[1] = fn1_depthResParam.clampMaxes[1] = true;
    manager->appendXY(fn1_depthResParam);
    
    // V1 IR resolution
    OP_NumericParameter fn1_irResParam;
    fn1_irResParam.name = "V1irresolution";
    fn1_irResParam.label = "IR Resolution";
    fn1_irResParam.page = "Resolution";
    fn1_irResParam.defaultValues[0] = MyFreenectDevice::WIDTH;
    fn1_irResParam.defaultValues[1] = MyFreenectDevice::HEIGHT;
    fn1_irResParam.minValues[0] = fn1_irResParam.minSliders[0] = 1.0;
    fn1_irResParam.maxValues[0] = fn1_irResParam.maxSliders[0] = MyFreenectDevice::WIDTH;
    fn1_irResParam.clampMins[0] = fn1_irResParam.clampMaxes[0] = true;
    fn1_irResParam.minValues[1] = fn1_irResParam.minSliders[1] = 1.0;
    fn1_irResParam.maxValues[1] = fn1_irResParam.maxSliders[1] = MyFreenectDevice::HEIGHT;
    fn1_irResParam.clampMins[1] = fn1_irResParam.clampMaxes[1] = true;
    manager->appendXY(fn1_irResParam);
    
    // V2 header
    OP_StringParameter fn2_resHeader;
    fn2_resHeader.name = "Kinectv2resolution";
    fn2_resHeader.page = "Resolution";
    fn2_resHeader.label = "Kinect V2";
    manager->appendHeader(fn2_resHeader);
    
    // V2 RGB resolution
    OP_NumericParameter fn2_rgbResParam;
    fn2_rgbResParam.name = "V2rgbresolution";
    fn2_rgbResParam.label = "RGB Resolution";
    fn2_rgbResParam.page = "Resolution";
    fn2_rgbResParam.defaultValues[0] = 1280.0;
    fn2_rgbResParam.defaultValues[1] = 720.0;
    fn2_rgbResParam.minValues[0] = fn2_rgbResParam.minSliders[0] = 1.0;
    fn2_rgbResParam.maxValues[0] = fn2_rgbResParam.maxSliders[0] = MyFreenect2Device::RGB_WIDTH;
    fn2_rgbResParam.clampMins[0] = fn2_rgbResParam.clampMaxes[0] = true;
    fn2_rgbResParam.minValues[1] = fn2_rgbResParam.minSliders[1] = 1.0;
    fn2_rgbResParam.maxValues[1] = fn2_rgbResParam.maxSliders[1] = MyFreenect2Device::RGB_HEIGHT;
    fn2_rgbResParam.clampMins[1] = fn2_rgbResParam.clampMaxes[1] = true;
    manager->appendXY(fn2_rgbResParam);
    
    // V2 Depth resolution
    OP_NumericParameter fn2_depthResParam;
    fn2_depthResParam.name = "V2depthresolution";
    fn2_depthResParam.label = "Depth Resolution";
    fn2_depthResParam.page = "Resolution";
    fn2_depthResParam.defaultValues[0] = MyFreenect2Device::DEPTH_WIDTH;
    fn2_depthResParam.defaultValues[1] = MyFreenect2Device::DEPTH_HEIGHT;
    fn2_depthResParam.minValues[0] = fn2_depthResParam.minSliders[0] = 1.0;
    fn2_depthResParam.maxValues[0] = fn2_depthResParam.maxSliders[0] = MyFreenect2Device::DEPTH_WIDTH;
    fn2_depthResParam.clampMins[0] = fn2_depthResParam.clampMaxes[0] = true;
    fn2_depthResParam.minValues[1] = fn2_depthResParam.minSliders[1] = 1.0;
    fn2_depthResParam.maxValues[1] = fn2_depthResParam.maxSliders[1] = MyFreenect2Device::DEPTH_HEIGHT;
    fn2_depthResParam.clampMins[1] = fn2_depthResParam.clampMaxes[1] = true;
    manager->appendXY(fn2_depthResParam);
    
    // V2 IR resolution
    OP_NumericParameter fn2_irResParam;
    fn2_irResParam.name = "V2irresolution";
    fn2_irResParam.label = "IR Resolution";
    fn2_irResParam.page = "Resolution";
    fn2_irResParam.defaultValues[0] = MyFreenect2Device::IR_WIDTH;
    fn2_irResParam.defaultValues[1] = MyFreenect2Device::IR_HEIGHT;
    fn2_irResParam.minValues[0] = fn2_irResParam.minSliders[0] = 1.0;
    fn2_irResParam.maxValues[0] = fn2_irResParam.maxSliders[0] = MyFreenect2Device::IR_WIDTH;
    fn2_irResParam.clampMins[0] = fn2_irResParam.clampMaxes[0] = true;
    fn2_irResParam.minValues[1] = fn2_irResParam.minSliders[1] = 1.0;
    fn2_irResParam.maxValues[1] = fn2_irResParam.maxSliders[1] = MyFreenect2Device::IR_HEIGHT;
    fn2_irResParam.clampMins[1] = fn2_irResParam.clampMaxes[1] = true;
    manager->appendXY(fn2_irResParam);
    
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
    fn2_cleanupDevice();
    fn1_cleanupDevice();
    fallbackBuffer.release(); // Release fallback buffer
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::fn1_initDevice() {
    // Crucial: Device init start
    LOG("[FreenectTOP] fn1_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    if (freenect_init(&fn1_ctx, nullptr) < 0) {
        LOG("[FreenectTOP] fn1_initDevice: freenect_init failed");
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
    
    if (numDevices <= 0) {
        errorString.clear();
        errorString = "No Kinect v1 devices found";
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        return false;
    }

    try {
        fn1_rgbReady = false;
        fn1_depthReady = false;
        fn1_device = new MyFreenectDevice(fn1_ctx, 0, fn1_rgbReady, fn1_depthReady);
        fn1_device->startVideo();
        fn1_device->startDepth();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        fn1_runEvents = true;
        fn1_eventThread = std::thread([this]() {
            LOG("[FreenectTOP] fn1_eventThread: running");
            while (fn1_runEvents.load()) {
                std::lock_guard<std::mutex> lock(fn1_eventMutex);
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
    } catch (...) {
        errorString.clear();
        errorString = "Failed to start Kinect v1 device";
        fn1_cleanupDevice();
        return false;
    }
    LOG("[FreenectTOP] fn1_initDevice: success");
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
void FreenectTOP::fn1_cleanupDevice() {
    LOG("[FreenectTOP] fn1_cleanupDevice: start");
    fn1_runEvents = false;
    if (fn1_eventThread.joinable()) {
        fn1_eventThread.join();
    }
    if (fn1_InitThread.joinable()) {
        fn1_InitThread.join();
    }
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
    LOG("[FreenectTOP] fn2_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    fn2_startEnumThread();
    if (!v2DeviceAvailable.load()) {
        LOG("[FreenectTOP] fn2_initDevice: no device available");
        return false;
    }
    if (fn2_ctx) {
        LOG("[FreenectTOP] fn2_initDevice: (end) already initialized");
        return true;
    }
    fn2_ctx = new libfreenect2::Freenect2();
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx->enumerateDevices() == 0) {
        errorString.clear();
        errorString = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: (end) no devices - fn2_ctx deleted and set to nullptr");
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
    } catch (...) {
        errorString.clear();
        errorString = "Couldn't create CPU pipeline for Kinect v2";
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_pipeline after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    }
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    LOG(std::string("[FreenectTOP] fn2_initDevice: openDevice returned dev = ") + std::to_string(reinterpret_cast<uintptr_t>(dev)));
    if (!dev) {
        errorString.clear();
        errorString = "Failed to open Kinect v2 device";
        delete fn2_device;
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
    // Stop enumeration thread after successful device start**
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
    fn1_InitThread = std::thread([this]() {
        bool result = this->fn1_initDevice();
        fn1InitSuccess = result;
        fn1InitInProgress = false;
    });
    if (fn1_InitThread.joinable()) {
        fn1_InitThread.join();
    } else {
        LOG("[FreenectTOP] startV1InitThread: fn1_InitThread not joinable after creation");
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
    if (!fn1_device) {
        LOG("[FreenectTOP] executeV1: device is null, initializing device in thread");
        startV1InitThread();
        if (!fn1InitSuccess.load()) {
            errorString.clear();
            errorString = "No Kinect v1 devices found";
            uploadFallbackBuffer();
            return;
        }
    }
    
    if (!fn1_device) {
        errorString = "Device is null after initialization";
        uploadFallbackBuffer();
        return;
    }
    
    std::string depthFormat = (inputs->getParString("Depthformat"));
    
    // Parameters to variables
    
    // V1 resolution values
    int fn1_colorW  = static_cast<int>(inputs->getParDouble("V1rgbresolution", 0));
    int fn1_colorH  = static_cast<int>(inputs->getParDouble("V1rgbresolution", 1));
    int fn1_depthW  = static_cast<int>(inputs->getParDouble("V1depthresolution", 0));
    int fn1_depthH  = static_cast<int>(inputs->getParDouble("V1depthresolution", 1));
    int fn1_irW     = static_cast<int>(inputs->getParDouble("V1irresolution", 0));
    int fn1_irH     = static_cast<int>(inputs->getParDouble("V1irresolution", 1));
    
    // Tilt angle
    float tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    
    if(fn1_device) {
        fn1_device->setResolutions(fn1_colorW, fn1_colorH, fn1_depthW, fn1_depthH, fn1_irW, fn1_irH);
    }
    
    // Set tilt angle
    try {
        fn1_device->setTiltDegrees(tilt);
    } catch (const std::exception& e) {
        errorString = "Failed to set tilt angle: " + std::string(e.what());
        fn1_cleanupDevice();
        fn1_device = nullptr;
        return;
    }
    
    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn1_colorW * fn1_colorH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    TD::OP_SmartRef<TD::TOP_Buffer> depthFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn1_depthW * fn1_depthH * 2, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    
    // Set color type based on parameter (not implemented yet, default to RGB)
    fn1_colorType colorType = fn1_colorType::RGB; // Default to RGB
    
    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    if (fn1_device->getColorFrame(colorFrame, colorType)) {
        errorString.clear();
        if (colorFrameBuffer) {
            std::memcpy(colorFrameBuffer->data, colorFrame.data(), fn1_colorW * fn1_colorH * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn1_colorW;
            info.textureDesc.height = fn1_colorH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&colorFrameBuffer, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create color output buffer");
        }
    }
    
    // Set depth type based on parameter
    fn1_depthType depthType = fn1_depthType::Raw; // Default to Raw
    if (depthFormat == "Raw") {
        depthType = fn1_depthType::Raw;
    } else if (depthFormat == "Registered") {
        depthType = fn1_depthType::Registered;
    }
    
    // --- Depth frame ---
    std::vector<uint16_t> depthFrame;
    if (fn1_device->getDepthFrame(depthFrame, depthType)) {
        errorString.clear();
        if (depthFrameBuffer) {
            std::memcpy(depthFrameBuffer->data, depthFrame.data(), fn1_depthW * fn1_depthH * 2);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn1_depthW;
            info.textureDesc.height = fn1_depthH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 1;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&depthFrameBuffer, info, nullptr);
        } else {
            LOG("[FreenectTOP] executeV1: failed to create depth output buffer");
        }
    }
}

// Execute method for Kinect v2 (libfreenect2)
void FreenectTOP::executeV2(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    //bool streamEnabledIR = (inputs->getParInt("Enableir") != 0);
    //bool streamEnabledDepth = (inputs->getParInt("Enabledepth") != 0);
    
    bool streamEnabledIR = true;
    bool streamEnabledDepth = true;

    if (!fn2_device) {
        LOG("[FreenectTOP] executeV2: fn2_device is null, initializing device");
        fn2_startInitThread();
        if (!fn2_InitSuccess.load()) {
            errorString = "No Kinect v2 devices found";
            uploadFallbackBuffer();
            return;
        }
    }
    
    // V2 resolution values
    int fn2_rgbW = static_cast<int>(inputs->getParDouble("V2rgbresolution", 0));
    int fn2_rgbH = static_cast<int>(inputs->getParDouble("V2rgbresolution", 1));
    int fn2_depthW = static_cast<int>(inputs->getParDouble("V2depthresolution", 0));
    int fn2_depthH = static_cast<int>(inputs->getParDouble("V2depthresolution", 1));
    int fn2_irW = static_cast<int>(inputs->getParDouble("V2irresolution", 0));
    int fn2_irH = static_cast<int>(inputs->getParDouble("V2irresolution", 1));
    int fn2_bigdepthW = fn2_rgbW;
    int fn2_bigdepthH = fn2_rgbH;
    
    if (fn2_device) {
        fn2_device->setResolutions(fn2_rgbW, fn2_rgbH, fn2_depthW, fn2_depthH, fn2_irW, fn2_irH, fn2_bigdepthW, fn2_bigdepthH);
    }
    
    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_rgbW * fn2_rgbH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();

    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    
    if (fn2_device->getColorFrame(colorFrame)) {
        errorString.clear();
        if (colorFrameBuffer) {
            std::memcpy(colorFrameBuffer->data, colorFrame.data(), fn2_rgbW * fn2_rgbH * 4);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn2_rgbW;
            info.textureDesc.height = fn2_rgbH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
            info.colorBufferIndex = 0;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&colorFrameBuffer, info, nullptr);
        }
    }

    // --- Depth frame ---
    std::vector<uint16_t> depthFrame;
    
    // Map string to DepthType enum
    if (streamEnabledDepth) {
        std::string depthFormat = inputs->getParString("Depthformat");
        bool depthUndistort = (inputs->getParInt("Depthundistort") != 0);
        fn2_depthType depthTypeEnum = fn2_depthType::Raw; // Default
        if (depthFormat == "Raw") {
            depthTypeEnum = depthUndistort ? fn2_depthType::Undistorted : fn2_depthType::Raw;
        } else if (depthFormat == "Registered") {
            depthTypeEnum = fn2_depthType::Registered;
        }
        
        if (depthFormat == "Raw") {
            if (fn2_device->getDepthFrame(depthFrame, depthTypeEnum)) {
                errorString.clear();
                TD::OP_SmartRef<TD::TOP_Buffer> buf =
                fntdContext ? fntdContext->createOutputBuffer(fn2_depthW * fn2_depthH * 2, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
                if (buf) {
                    std::memcpy(buf->data, depthFrame.data(), fn2_depthW * fn2_depthH * 2);
                    TD::TOP_UploadInfo info;
                    info.textureDesc.width = fn2_depthW;
                    info.textureDesc.height = fn2_depthH;
                    info.textureDesc.texDim = TD::OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
                    info.colorBufferIndex = 1;
                    info.firstPixel = TD::TOP_FirstPixel::TopLeft;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
        } else if (depthFormat == "Registered") {
            if (fn2_device->getDepthFrame(depthFrame, depthTypeEnum)) {
                errorString.clear();
                TD::OP_SmartRef<TD::TOP_Buffer> buf =
                fntdContext ? fntdContext->createOutputBuffer(fn2_bigdepthW * fn2_bigdepthH * 2, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
                if (buf) {
                    std::memcpy(buf->data, depthFrame.data(), fn2_bigdepthW * fn2_bigdepthH * 2);
                    TD::TOP_UploadInfo info;
                    info.textureDesc.width = fn2_bigdepthW;
                    info.textureDesc.height = fn2_bigdepthH;
                    info.textureDesc.texDim = TD::OP_TexDim::e2D;
                    info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
                    info.colorBufferIndex = 1;
                    info.firstPixel = TD::TOP_FirstPixel::TopLeft;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
        }
    } else {
        uploadFallbackBuffer(1);
        //fntdContext->returnBuffer(&buf)
    }

    // --- IR frame ---
    if (streamEnabledIR) {
        std::vector<uint16_t> irFrame;
        if (fn2_device->getIRFrame(irFrame)) {
            errorString.clear();
            TD::OP_SmartRef<TD::TOP_Buffer> buf =
            fntdContext ? fntdContext->createOutputBuffer(fn2_irW * fn2_irH * 2, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
            if (buf) {
                std::memcpy(buf->data, irFrame.data(), fn2_irW * fn2_irH * 2);
                TD::TOP_UploadInfo info;
                info.textureDesc.width = fn2_irW;
                info.textureDesc.height = fn2_irH;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex = 2;
                info.firstPixel = TD::TOP_FirstPixel::TopLeft;
                output->uploadBuffer(&buf, info, nullptr);
            }
        }
    } else {
        uploadFallbackBuffer(2);
    }
}


// Main execution method
void FreenectTOP::execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) {
    myCurrentOutput = output;
    
    // If errorString is set, LOG it
    if ( FNTD_DEBUG == 1 && !errorString.empty() && errorString != "No Kinect v1 devices found" && errorString != "No Kinect v2 devices found") {
        LOG("[FreenectTOP] ERROR: " + errorString);
    }
    
    // Validate inputs
    if (!inputs) {
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
    
    // Enable/disable parameters based on device type
    auto dynamicParameterEnable = [&](const char* name, bool v1, bool v2, bool other = true) {
        bool enabled = (devType == "Kinect v1") ? v1 : (devType == "Kinect v2") ? v2 : other;
        inputs->enablePar(name, enabled);
    };
    
    // V1 only parameters
    dynamicParameterEnable("Tilt", true, false);
    dynamicParameterEnable("V1rgbresolution", true, false);
    dynamicParameterEnable("V1irresolution", true, false);
    
    // V2 only parameters
    dynamicParameterEnable("V2rgbresolution", false, true);
    dynamicParameterEnable("V2irresolution", false, true);
    
    // Enable/disable depthUndistort based on device type and depthFormat
    if (devType == "Kinect v2" && depthFormat == "Raw") {
        inputs->enablePar("Depthundistort", true);
    } else {
        inputs->enablePar("Depthundistort", false);
    }
    
    // Enable/disable depthResolution based on depthFormat
    if (depthFormat == "Registered") {
        dynamicParameterEnable("V1depthresolution", false, false);
        dynamicParameterEnable("V2depthresolution", false, false);
    } else {
        dynamicParameterEnable("V1depthresolution", true, false);
        dynamicParameterEnable("V2depthresolution", false, true);
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
void FreenectTOP::uploadFallbackBuffer(int targetIndex) {
    if (!myCurrentOutput) {
        LOG("[FreenectTOP] uploadFallbackBuffer: myCurrentOutput is null");
        return;
    }

    const int fallbackWidth = 128, fallbackHeight = 128;

    if (!fallbackBuffer) {
        std::vector<uint8_t> black(fallbackWidth * fallbackHeight * 4, 0);
        fallbackBuffer = fntdContext ? fntdContext->createOutputBuffer(
            fallbackWidth * fallbackHeight * 4,
            TD::TOP_BufferFlags::None,
            nullptr
        ) : TD::OP_SmartRef<TD::TOP_Buffer>();

        if (fallbackBuffer) {
            std::memcpy(fallbackBuffer->data, black.data(), fallbackWidth * fallbackHeight * 4);
        }
    }

    if (!fallbackBuffer) return;

    TD::TOP_UploadInfo info;
    info.textureDesc.width = fallbackWidth;
    info.textureDesc.height = fallbackHeight;
    info.textureDesc.texDim = TD::OP_TexDim::e2D;
    info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;

    if (!fallbackBuffer0) fallbackBuffer0 = fallbackBuffer;
    if (!fallbackBuffer1) fallbackBuffer1 = fallbackBuffer;
    if (!fallbackBuffer2) fallbackBuffer2 = fallbackBuffer;

    std::array<TD::OP_SmartRef<TD::TOP_Buffer>, 3> fallbackBuffers = {
        fallbackBuffer0, fallbackBuffer1, fallbackBuffer2
    };

    if (targetIndex >= 0 && targetIndex < 3) {
        info.colorBufferIndex = targetIndex;
        myCurrentOutput->uploadBuffer(&fallbackBuffers[targetIndex], info, nullptr);
    } else {
        for (int i = 0; i < 3; ++i) {
            info.colorBufferIndex = i;
            myCurrentOutput->uploadBuffer(&fallbackBuffers[i], info, nullptr);
        }
    }
}
