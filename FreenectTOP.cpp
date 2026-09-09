//
// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include <algorithm>
#include <cstdio>
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
        info->customOPInfo.minorVersion = 1;
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

    // Small helpers so every parameter is declared the same way
    auto header = [&](const char* name, const char* label, const char* page) {
        OP_StringParameter h; h.name = name; h.label = label; h.page = page;
        manager->appendHeader(h);
    };
    auto toggle = [&](const char* name, const char* label, double def, const char* page) {
        OP_NumericParameter t; t.name = name; t.label = label; t.page = page; t.defaultValues[0] = def;
        t.minValues[0] = t.minSliders[0] = 0.0; t.maxValues[0] = t.maxSliders[0] = 1.0;
        t.clampMins[0] = t.clampMaxes[0] = true;
        manager->appendToggle(t);
    };
    auto menu = [&](const char* name, const char* label, const char* def, int n, const char** names, const char** labels, const char* page) {
        OP_StringParameter m; m.name = name; m.label = label; m.page = page; m.defaultValue = def;
        manager->appendMenu(m, n, names, labels);
    };

    // =====================================================================
    // FREENECT PAGE
    // =====================================================================
    const char* PG = "Freenect";

    // --- Device ---
    header("Hdrdevice", "Device", PG);
    toggle("Active", "Active", 1.0, PG);
    {
        const char* names[]  = {"Kinect v1", "Kinect v2"};
        const char* labels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
        menu("Hardwareversion", "Hardware Version", "Kinect v1", 2, names, labels, PG);
    }
    {
        OP_NumericParameter t; t.name = "Tilt"; t.label = "Tilt Angle (deg)"; t.page = PG;
        t.defaultValues[0] = 0.0; t.minValues[0] = t.minSliders[0] = -30.0; t.maxValues[0] = t.maxSliders[0] = 30.0;
        t.clampMins[0] = t.clampMaxes[0] = true;
        manager->appendFloat(t);
    }

    // --- Streams. The number is the Render Select TOP image index; 0 (RGB) is always on. ---
    header("Hdrstreams", "Streams (number = Render Select index, 0 = RGB always on)", PG);
    toggle("Enabledepth",      "1  Depth",              1.0, PG);
    toggle("Enablepointcloud", "2  Point Cloud",        0.0, PG);
    toggle("Enableir",         "3  IR",                 0.0, PG);
    toggle("Enableregcolor",   "4  Registered Color",   0.0, PG);
    toggle("Enableuv",         "5  Depth-to-Color UV",  0.0, PG);

    // --- Depth & point cloud. One Format menu drives both: Registered puts depth AND the point
    //     cloud in the color camera (aligned to RGB); Raw keeps them in the depth camera. ---
    header("Hdrdepth", "Depth & Point Cloud", PG);
    {
        const char* names[]  = {"Raw", "Rawundistorted", "Registered"};
        const char* labels[] = {"Raw (depth camera)", "Raw, Undistorted (depth camera)", "Registered (aligned to RGB)"};
        menu("Depthformat", "Format", "Raw", 3, names, labels, PG);
    }
    {
        const char* names[]  = {"Normalized", "Millimeters", "Meters"};
        const char* labels[] = {"Normalized 16-bit (0-1 across depth range)", "Millimeters (32-bit float)", "Meters (32-bit float)"};
        menu("Depthoutput", "Depth Output", "Normalized", 3, names, labels, PG);
    }
    toggle("Manualdepththresh", "Manual Depth Range", 0.0, PG);
    {
        OP_NumericParameter t; t.name = "Depththreshmin"; t.label = "Depth Range Min (mm)"; t.page = PG;
        t.defaultValues[0] = 0.0; t.minValues[0] = t.minSliders[0] = 0.0; t.maxValues[0] = t.maxSliders[0] = 5000.0; t.clampMins[0] = true;
        manager->appendFloat(t);
    }
    {
        OP_NumericParameter t; t.name = "Depththreshmax"; t.label = "Depth Range Max (mm)"; t.page = PG;
        t.defaultValues[0] = 5000.0; t.minValues[0] = t.minSliders[0] = 0.0; t.maxValues[0] = t.maxSliders[0] = 5000.0; t.clampMins[0] = true;
        manager->appendFloat(t);
    }
    // Point cloud native frame: +Y up, +Z away from the sensor, X follows the mirrored image.
    toggle("Pcflipx", "Point Cloud Flip X", 0.0, PG);
    toggle("Pcflipy", "Point Cloud Flip Y", 0.0, PG);
    toggle("Pcflipz", "Point Cloud Flip Z (+Z toward viewer)", 0.0, PG);

    // =====================================================================
    // RESOLUTION PAGE - presets only; every size is a nearest-neighbour
    // downscale of the native frame, the field of view never changes.
    // =====================================================================
    const char* PR = "Resolution";
    header("Hdrresnote", "Downscale presets. Field of view never changes.", PR);

    header("Kinectv1resolution", "Kinect v1 (native 640x480)", PR);
    {
        const char* names[]  = {"640x480", "320x240", "160x120"};
        menu("V1rgbres",   "RGB Resolution",   "640x480", 3, names, names, PR);
        menu("V1depthres", "Depth Resolution", "640x480", 3, names, names, PR);
    }

    header("Kinectv2resolution", "Kinect v2 (native RGB 1920x1080, depth/IR 512x424)", PR);
    {
        const char* rgbNames[]   = {"1920x1080", "1280x720", "960x540", "640x360"};
        const char* depthNames[] = {"512x424", "256x212", "128x106"};
        menu("V2rgbres",   "RGB Resolution",         "1280x720", 4, rgbNames,   rgbNames,   PR);
        menu("V2depthres", "Depth Resolution",       "512x424",  3, depthNames, depthNames, PR);
        menu("V2pcres",    "Point Cloud Resolution", "512x424",  3, depthNames, depthNames, PR);
        menu("V2irres",    "IR Resolution",          "512x424",  3, depthNames, depthNames, PR);
    }
    header("Hdrresnote2", "Registered depth / point cloud follow the RGB resolution.", PR);

    // =====================================================================
    // ABOUT PAGE
    // =====================================================================
    const char* PA = "About";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION + " - by @stosumarte";
    header("Version", versionLabel.c_str(), PA);
    header("Hdrcontrib", "Point cloud registration, float depth, POP workflow (v1.1): Dean Cheesman", PA);
    header("Emptyheader1", " ", PA);
    header("Hdroutputs",  "Outputs via Render Select TOP (Image index):", PA);
    header("Hdroutputs0", "0 RGB   1 Depth   2 Point Cloud   3 IR", PA);
    header("Hdroutputs1", "4 Registered Color   5 Depth-to-Color UV", PA);
    header("Emptyheader2", " ", PA);
    header("Updateheader", "Visit the following URL to check for updates:", PA);
    {
        OP_StringParameter u; u.name = "Updateurl"; u.label = "Copy this -> "; u.page = PA;
        u.defaultValue = "github.com/stosumarte/FreenectTD/releases/latest";
        manager->appendString(u);
    }
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
    //fallbackBuffer.release(); // Release fallback buffer
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
    LOG("[FreenectTOP] fn2_startEnumThread: fn2_enumThreadRunning before = " + std::to_string(fn2_enumThreadRunning.load()));
    if (fn2_enumThreadRunning.load()) {
        LOG("[FreenectTOP] fn2_startEnumThread: end, already running");
        return;
    }
    fn2_enumThreadRunning = true;
    LOG("[FreenectTOP] fn2_startEnumThread: fn2_enumThreadRunning after = " + std::to_string(fn2_enumThreadRunning.load()));
    fn2_enumThread = std::thread([this]() {
        while (fn2_enumThreadRunning.load()) {
            libfreenect2::Freenect2 ctx;
            fn2_deviceAvailable = (ctx.enumerateDevices() > 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    LOG("[FreenectTOP] fn2_startEnumThread: fn2_enumThread joinable after = " + std::to_string(fn2_enumThread.joinable()));
}

// Stop the background enumeration thread for Kinect v2
void FreenectTOP::fn2_stopEnumThread() {
    LOG("[FreenectTOP] fn2_stopEnumThread: start");
    LOG("[FreenectTOP] fn2_stopEnumThread: fn2_enumThreadRunning before = " + std::to_string(fn2_enumThreadRunning.load()));
    fn2_enumThreadRunning = false;
    LOG("[FreenectTOP] fn2_stopEnumThread: fn2_enumThreadRunning after = " + std::to_string(fn2_enumThreadRunning.load()));
    LOG("[FreenectTOP] fn2_stopEnumThread: fn2_enumThread joinable = " + std::to_string(fn2_enumThread.joinable()));
    
    if (fn2_enumThread.joinable()) {
        LOG("[FreenectTOP] fn2_stopEnumThread: attempting to join fn2_enumThread");
        fn2_enumThread.join();
        LOG("[FreenectTOP] fn2_enumThread joined successfully");
    }
    LOG("[FreenectTOP] fn2_stopEnumThread: end");
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::fn2_initDevice() {
    LOG("[FreenectTOP] fn2_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    fn2_startEnumThread();
    if (!fn2_deviceAvailable.load()) {
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
    
    // Stop enumeration thread after successful device start
    //fn2_stopEnumThread();
    LOG("[FreenectTOP] fn2_initDevice: device started and enum thread stopped");
    LOG("[FreenectTOP] fn2_initDevice: end (success)");
    return true;
}

// Threaded initialization for Kinect v1
void FreenectTOP::fn1_startInitThread() {
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
        LOG("[FreenectTOP] fn1_startInitThread: fn1_InitThread not joinable after creation");
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

    if (fn2_InitThread.joinable()) {
        fn2_InitThread.join();
    } else {
        LOG("[FreenectTOP] fn2_cleanupDevice: couldn't join fn2_InitThread");
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
void FreenectTOP::fn1_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    if (!fn1_device) {
        LOG("[FreenectTOP] executeV1: device is null, initializing device in thread");
        fn1_startInitThread();
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
    
    if(fn1_device) {
        fn1_device->setResolutions(fn1_colorW, fn1_colorH, fn1_depthW, fn1_depthH, fn1_irW, fn1_irH);
    }
    
    // Set tilt angle
    try {
        fn1_device->setTiltDegrees(fn1_tilt);
    } catch (const std::exception& e) {
        errorString = "Failed to set tilt angle: " + std::string(e.what());
        fn1_cleanupDevice();
        fn1_device = nullptr;
        return;
    }
    
    // Set color type based on parameter (not implemented yet, default to RGB)
    fn1_colorType colorType = fn1_colorType::RGB; // Default to RGB
    
    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn1_colorW * fn1_colorH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    
    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    if (colorFrameBuffer && fn1_device->getColorFrame(colorFrame, colorType)) {
        errorString.clear();
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
    
    // --- Depth frame ---
    if (streamEnabledDepth) {
        std::vector<float> depthFrame; // millimetres, 0 = invalid
        if (fn1_device->getDepthFrame(depthFrame, depthFormat, depthThreshMin, depthThreshMax)) {
            errorString.clear();
            uploadDepthFrame(output, depthFrame, fn1_depthW, fn1_depthH);
        }
    } else {
        uploadFallbackBuffer(1);
    }
    
}
    
// Execute method for Kinect v2 (libfreenect2)
void FreenectTOP::fn2_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    // Check if device was disconnected
    if (!fn2_deviceAvailable.load() && fn2_device) {
        fn2_cleanupDevice();
        uploadFallbackBuffer();
        return;
    }

    // Always attempt initialization if device is null
    if (!fn2_device) {
        LOG("[FreenectTOP] executeV2: fn2_device is null, attempting initialization");
        fn2_startInitThread();
        if (!fn2_InitSuccess.load()) {
            errorString = "No Kinect v2 devices found";
            uploadFallbackBuffer();
            return;
        }
    }

    if (fn2_device) {
        fn2_device->setResolutions(fn2_colorW, fn2_colorH, fn2_depthW, fn2_depthH, fn2_pcW, fn2_pcH, fn2_irW, fn2_irH);
    }

    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_colorW * fn2_colorH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    TD::OP_SmartRef<TD::TOP_Buffer> pointCloudFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_pcW * fn2_pcH * 4 * sizeof(float), TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    TD::OP_SmartRef<TD::TOP_Buffer> irFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_irW * fn2_irH * 2, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();

    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    if (colorFrameBuffer && fn2_device->getColorFrame(colorFrame)) {
        errorString.clear();
        std::memcpy(colorFrameBuffer->data, colorFrame.data(), fn2_colorW * fn2_colorH * 4);
        TD::TOP_UploadInfo info;
        info.textureDesc.width = fn2_colorW;
        info.textureDesc.height = fn2_colorH;
        info.textureDesc.texDim = TD::OP_TexDim::e2D;
        info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
        info.colorBufferIndex = 0;
        info.firstPixel = TD::TOP_FirstPixel::TopLeft;
        output->uploadBuffer(&colorFrameBuffer, info, nullptr);
    }
    
    // --- Depth frame ---
    if (streamEnabledDepth) {
        std::vector<float> depthFrame; // millimetres, 0 = invalid
        if (fn2_device->getDepthFrame(depthFrame, depthFormat, depthThreshMin, depthThreshMax)) {
            errorString.clear();
            uploadDepthFrame(output, depthFrame, fn2_depthW, fn2_depthH);
        }
    } else {
        uploadFallbackBuffer(1);
    }
    
    // --- Point Cloud frame ---
    if (streamEnabledPC) {
        std::vector<float> pointCloudFrame;
        if (pointCloudFrameBuffer && fn2_device->getPointCloudFrame(pointCloudFrame, pcSpace, depthThreshMin, depthThreshMax, pcFlipX, pcFlipY, pcFlipZ)) {
            errorString.clear();
            std::memcpy(pointCloudFrameBuffer->data, pointCloudFrame.data(), fn2_pcW * fn2_pcH * 4 * sizeof(float));
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn2_pcW;
            info.textureDesc.height = fn2_pcH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA32Float;
            info.colorBufferIndex = 2;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&pointCloudFrameBuffer, info, nullptr);
        } else {errorString = "Failed to get point cloud frame from Kinect v2";}
    } else {
        uploadFallbackBuffer(2);
    }

    // --- Registered color (index 4) and depth-to-color UV map (index 5) ---
    if (streamEnabledRegColor || streamEnabledUV) {
        const int rw = MyFreenect2Device::DEPTH_WIDTH;
        const int rh = MyFreenect2Device::DEPTH_HEIGHT;
        std::vector<uint8_t> regColor;
        std::vector<float> regUV;
        if (fntdContext && fn2_device->getRegisteredColorFrame(regColor, regUV)) {
            TD::TOP_UploadInfo info;
            info.textureDesc.width = rw;
            info.textureDesc.height = rh;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            if (streamEnabledRegColor) {
                TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext->createOutputBuffer(rw * rh * 4, TD::TOP_BufferFlags::None, nullptr);
                if (buf) {
                    std::memcpy(buf->data, regColor.data(), rw * rh * 4);
                    info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex = 4;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
            if (streamEnabledUV) {
                TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext->createOutputBuffer(rw * rh * 4 * sizeof(float), TD::TOP_BufferFlags::None, nullptr);
                if (buf) {
                    std::memcpy(buf->data, regUV.data(), rw * rh * 4 * sizeof(float));
                    info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA32Float;
                    info.colorBufferIndex = 5;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
        }
    }
    if (!streamEnabledRegColor) uploadFallbackBuffer(4);
    if (!streamEnabledUV) uploadFallbackBuffer(5);

    // --- IR frame ---
    if (streamEnabledIR) {
        std::vector<uint16_t> irFrame;
        if (irFrameBuffer && fn2_device->getIRFrame(irFrame)) {
            errorString.clear();
            std::memcpy(irFrameBuffer->data, irFrame.data(), fn2_irW * fn2_irH * 2);
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn2_irW;
            info.textureDesc.height = fn2_irH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
            info.colorBufferIndex = 3;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&irFrameBuffer, info, nullptr);
        }
    } else {
        uploadFallbackBuffer(3);
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
    
    // Get parameters
    bool isActive = (inputs && inputs->getParInt("Active") != 0);
    const char* devTypeCStr = inputs->getParString("Hardwareversion");
    std::string devType = devTypeCStr ? devTypeCStr : "Kinect v1";
    static std::string lastDeviceType = "Kinect v1";
    
    // Set depthFormat from parameters
    {
        const char* c = inputs->getParString("Depthformat");
        std::string depthFormatStr = c ? c : "";
        if (depthFormatStr == "Registered") depthFormat = depthFormatEnum::Registered;
        else if (depthFormatStr == "Rawundistorted" && devType == "Kinect v2") depthFormat = depthFormatEnum::RawUndistorted;
        else depthFormat = depthFormatEnum::Raw;
    }
    
    manualDepthThresh = (inputs->getParInt("Manualdepththresh") != 0);
    depthThreshMin = static_cast<float>(inputs->getParDouble("Depththreshmin"));
    depthThreshMax = static_cast<float>(inputs->getParDouble("Depththreshmax"));
    
    streamEnabledIR = (inputs->getParInt("Enableir") != 0);
    streamEnabledDepth = (inputs->getParInt("Enabledepth") != 0);
    streamEnabledPC = (inputs->getParInt("Enablepointcloud") != 0);
    streamEnabledRegColor = (inputs->getParInt("Enableregcolor") != 0);
    streamEnabledUV = (inputs->getParInt("Enableuv") != 0);
    {
        const char* c = inputs->getParString("Depthoutput");
        std::string depthOutputStr = c ? c : "";
        depthOutput = (depthOutputStr == "Millimeters") ? depthOutputEnum::Millimeters
                    : (depthOutputStr == "Meters")      ? depthOutputEnum::Meters
                                                        : depthOutputEnum::Normalized;
        pcSpace = (depthFormat == depthFormatEnum::Registered) ? pcSpaceEnum::ColorCamera : pcSpaceEnum::DepthCamera;
        pcFlipX = (inputs->getParInt("Pcflipx") != 0);
        pcFlipY = (inputs->getParInt("Pcflipy") != 0);
        pcFlipZ = (inputs->getParInt("Pcflipz") != 0);
    }
    
    fn1_tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    
    // Resolution presets ("WxH" menu strings)
    auto parseRes = [&](const char* parName, int& w, int& h, int defW, int defH) {
        const char* c = inputs->getParString(parName);
        int pw = 0, ph = 0;
        if (c && std::sscanf(c, "%dx%d", &pw, &ph) == 2 && pw > 0 && ph > 0) { w = pw; h = ph; }
        else { w = defW; h = defH; }
    };
    parseRes("V1rgbres",   fn1_colorW, fn1_colorH, MyFreenectDevice::WIDTH, MyFreenectDevice::HEIGHT);
    parseRes("V1depthres", fn1_depthW, fn1_depthH, MyFreenectDevice::WIDTH, MyFreenectDevice::HEIGHT);
    parseRes("V2rgbres",   fn2_colorW, fn2_colorH, MyFreenect2Device::SCALED_WIDTH, MyFreenect2Device::SCALED_HEIGHT);
    parseRes("V2depthres", fn2_depthW, fn2_depthH, MyFreenect2Device::DEPTH_WIDTH, MyFreenect2Device::DEPTH_HEIGHT);
    parseRes("V2pcres",    fn2_pcW,    fn2_pcH,    MyFreenect2Device::DEPTH_WIDTH, MyFreenect2Device::DEPTH_HEIGHT);
    parseRes("V2irres",    fn2_irW,    fn2_irH,    MyFreenect2Device::IR_WIDTH,    MyFreenect2Device::IR_HEIGHT);
    if (devType == "Kinect v2" && depthFormat == depthFormatEnum::Registered) {
        fn2_depthW = fn2_colorW;
        fn2_depthH = fn2_colorH;
    }
    if (devType == "Kinect v2" && pcSpace == pcSpaceEnum::ColorCamera) {
        // Color-space point cloud is pixel-aligned with the RGB output
        fn2_pcW = fn2_colorW;
        fn2_pcH = fn2_colorH;
    }
    
    // Enable/disable parameters based on device type
    auto dynamicParameterEnable = [&](const char* name, bool v1, bool v2, bool other = true) {
        bool enabled = (devType == "Kinect v1") ? v1 : (devType == "Kinect v2") ? v2 : other;
        inputs->enablePar(name, enabled);
    };
    
    // Device-specific parameters
    dynamicParameterEnable("Tilt", true, false);
    dynamicParameterEnable("Enableir", false, true);
    dynamicParameterEnable("Enablepointcloud", false, true);
    dynamicParameterEnable("V1rgbres", true, false);
    dynamicParameterEnable("V2rgbres", false, true);
    dynamicParameterEnable("V2irres", false, true);
    dynamicParameterEnable("V2pcres", false, true);
    dynamicParameterEnable("Enableregcolor", false, true);
    dynamicParameterEnable("Enableuv", false, true);
    dynamicParameterEnable("Pcflipx", false, true);
    dynamicParameterEnable("Pcflipy", false, true);
    dynamicParameterEnable("Pcflipz", false, true);
    dynamicParameterEnable("Enableregcolor", false, true);
    if (devType == "Kinect v2" && pcSpace == pcSpaceEnum::ColorCamera) {
        inputs->enablePar("V2pcres", false);
    }
    
    
    // Enable/disable depthResolution based on depthFormat
    if (depthFormat == depthFormatEnum::Registered) {
        dynamicParameterEnable("V1depthres", false, false);
        dynamicParameterEnable("V2depthres", false, false);
    } else {
        dynamicParameterEnable("V1depthres", true, false);
        dynamicParameterEnable("V2depthres", false, true);
    }
    
    // Enable/disable depthThreshMin/Max based on manualDepthThresh
    if (!manualDepthThresh) {
        inputs->enablePar("Depththreshmin", false);
        inputs->enablePar("Depththreshmax", false);
    } else {
        inputs->enablePar("Depththreshmin", true);
        inputs->enablePar("Depththreshmax", true);
    }
    
    if (!manualDepthThresh && devType == "Kinect v1") {
        depthThreshMin = 400.0f;
        depthThreshMax = 4500.0f;
    }
    
    if (!manualDepthThresh && devType == "Kinect v2") {
        depthThreshMin = 100.0f;
        depthThreshMax = 4500.0f;
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
        fn2_execute(output, inputs);
    } else {
        fn1_execute(output, inputs);
    }
}

// Upload a fallback black buffer
// Packs a millimetre depth map into the output texture at index 1 according to the Depthoutput parameter.
void FreenectTOP::uploadDepthFrame(TD::TOP_Output* output, const std::vector<float>& depthMM, int width, int height) {
    if (!output || !fntdContext || width <= 0 || height <= 0) return;
    const size_t pixelCount = static_cast<size_t>(width) * height;
    if (depthMM.size() < pixelCount) {
        LOG("[FreenectTOP] uploadDepthFrame: depth buffer smaller than requested size");
        return;
    }

    const bool packed16 = (depthOutput == depthOutputEnum::Normalized);
    const size_t bytes = pixelCount * (packed16 ? sizeof(uint16_t) : sizeof(float));
    TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext->createOutputBuffer(bytes, TD::TOP_BufferFlags::None, nullptr);
    if (!buf) {
        LOG("[FreenectTOP] uploadDepthFrame: failed to create depth output buffer");
        return;
    }

    if (packed16) {
        // Legacy behaviour: 0..1 across the threshold window, 0 = invalid
        uint16_t* dst = static_cast<uint16_t*>(buf->data);
        const float denom = std::max(depthThreshMax - depthThreshMin, 1.0f);
        #pragma omp parallel for if(pixelCount > 100000)
        for (size_t i = 0; i < pixelCount; ++i) {
            const float d = depthMM[i];
            if (d <= 0.0f) { dst[i] = 0; continue; }
            const float n = std::clamp((d - depthThreshMin) / denom, 0.0f, 1.0f);
            dst[i] = static_cast<uint16_t>(n * 65535.0f + 0.5f);
        }
    } else {
        float* dst = static_cast<float*>(buf->data);
        const float scale = (depthOutput == depthOutputEnum::Meters) ? 0.001f : 1.0f;
        #pragma omp parallel for if(pixelCount > 100000)
        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i] = depthMM[i] * scale;
        }
    }

    TD::TOP_UploadInfo info;
    info.textureDesc.width = width;
    info.textureDesc.height = height;
    info.textureDesc.texDim = TD::OP_TexDim::e2D;
    info.textureDesc.pixelFormat = packed16 ? TD::OP_PixelFormat::Mono16Fixed : TD::OP_PixelFormat::Mono32Float;
    info.colorBufferIndex = 1;
    info.firstPixel = TD::TOP_FirstPixel::TopLeft;
    output->uploadBuffer(&buf, info, nullptr);
}

void FreenectTOP::uploadFallbackBuffer(int targetIndex) {
    if (!myCurrentOutput) {
        LOG("[FreenectTOP] uploadFallbackBuffer: myCurrentOutput is null");
        return;
    }

    const int fallbackWidth = 128, fallbackHeight = 128;
    const size_t fallbackSize = fallbackWidth * fallbackHeight * 4;
    std::vector<uint8_t> black(fallbackSize, 0);

    // Allocate and initialize each fallback buffer if not already
    for (int i = 0; i < kNumOutputs; ++i) {
        if (!fallbackBuffers[i]) {
            fallbackBuffers[i] = fntdContext ? fntdContext->createOutputBuffer(
                fallbackSize,
                TD::TOP_BufferFlags::None,
                nullptr
            ) : TD::OP_SmartRef<TD::TOP_Buffer>();
            if (fallbackBuffers[i]) {
                std::memcpy(fallbackBuffers[i]->data, black.data(), fallbackSize);
            }
        }
    }

    TD::TOP_UploadInfo info;
    info.textureDesc.width = fallbackWidth;
    info.textureDesc.height = fallbackHeight;
    info.textureDesc.texDim = TD::OP_TexDim::e2D;
    info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;

    if (targetIndex >= 0 && targetIndex < kNumOutputs) {
        info.colorBufferIndex = targetIndex;
        myCurrentOutput->uploadBuffer(&fallbackBuffers[targetIndex], info, nullptr);
    } else {
        for (int i = 0; i < kNumOutputs; ++i) {
            info.colorBufferIndex = i;
            myCurrentOutput->uploadBuffer(&fallbackBuffers[i], info, nullptr);
        }
    }
}
