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
#include <dlfcn.h>
#include <optional>
#include <cmath>
#include <libusb.h>

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

    // Separator line above the parameter and Alt+hover help text (TouchDesigner 2025 API only)
    auto layout = [](auto& param, const char* help, bool section) {
#if TD_VERSION == 2025
        param.help = help;
        param.section = section;
#else
        (void)param; (void)help; (void)section;
#endif
    };

    // Small helpers so every parameter is declared the same way
    auto header = [&](const char* name, const char* label, const char* page, bool section = false) {
        OP_StringParameter headerParam;
        headerParam.name = name;
        headerParam.label = label;
        headerParam.page = page;
        layout(headerParam, nullptr, section);
        manager->appendHeader(headerParam);
    };
    auto toggle = [&](const char* name, const char* label, double defaultValue, const char* page,
                      const char* help = nullptr, bool section = false) {
        OP_NumericParameter toggleParam;
        toggleParam.name = name;
        toggleParam.label = label;
        toggleParam.page = page;
        toggleParam.defaultValues[0] = defaultValue;
        toggleParam.minValues[0] = 0.0;
        toggleParam.maxValues[0] = 1.0;
        toggleParam.minSliders[0] = 0.0;
        toggleParam.maxSliders[0] = 1.0;
        toggleParam.clampMins[0] = true;
        toggleParam.clampMaxes[0] = true;
        layout(toggleParam, help, section);
        manager->appendToggle(toggleParam);
    };
    auto menu = [&](const char* name, const char* label, const char* defaultValue, int count,
                    const char** names, const char** labels, const char* page,
                    const char* help = nullptr, bool section = false) {
        OP_StringParameter menuParam;
        menuParam.name = name;
        menuParam.label = label;
        menuParam.page = page;
        menuParam.defaultValue = defaultValue;
        layout(menuParam, help, section);
        manager->appendMenu(menuParam, count, names, labels);
    };

    // -------------
    // FREENECT PAGE
    // -------------
    const char* page0 = "Freenect";

    // --- Device ---
    toggle("Active", "Active", 1.0, page0);
    {
        const char* names[]  = {"Kinect v1", "Kinect v2"};
        const char* labels[] = {"Kinect v1 (Xbox 360)", "Kinect v2 (Xbox One)"};
        menu("Hardwareversion", "Hardware Version", "Kinect v1", 2, names, labels, page0);
    }
    {
        OP_NumericParameter tiltAngleParam;
        tiltAngleParam.name = "Tilt";
        tiltAngleParam.label = "Tilt Angle";
        tiltAngleParam.page = page0;
        tiltAngleParam.defaultValues[0] = 0.0;
        tiltAngleParam.minValues[0] = -30.0;
        tiltAngleParam.maxValues[0] = 30.0;
        tiltAngleParam.minSliders[0] = -30.0;
        tiltAngleParam.maxSliders[0] = 30.0;
        tiltAngleParam.clampMins[0] = true;
        tiltAngleParam.clampMaxes[0] = true;
        manager->appendFloat(tiltAngleParam);
    }

    // --- Streams ---
    toggle("Enabledepth",      "Depth [1]",              1.0, page0,
           "Number in brackets = Render Select TOP image index. RGB is always on, at index 0 (blank on Kinect v1 while IR is on).", true);
    toggle("Enablepointcloud", "Point Cloud [2]",        0.0, page0);
    toggle("Enableir",         "IR [3]",                 0.0, page0,
           "On Kinect v1, RGB and IR share one stream: turning IR on blanks RGB [0].");
    toggle("Enableregcolor",   "Registered Color [4]",   0.0, page0);
    toggle("Enableuv",         "Depth-to-Color UV [5]",  0.0, page0);

    // --- Depth & point cloud ---
    // One Format menu drives both: Registered puts depth AND the point cloud in the
    // color camera (aligned to RGB); Raw keeps them in the depth camera.
    {
        const char* names[]  = {"Raw", "Rawundistorted", "Registered"};
        const char* labels[] = {"Raw", "Raw undistorted", "Registered (aligned to RGB)"};
        menu("Depthformat", "Format", "Raw", 3, names, labels, page0,
             "Registered aligns depth and point cloud to the RGB image; Raw keeps them in the depth camera.", true);
    }
    {
        const char* names[]  = {"Normalized", "Millimeters", "Meters"};
        const char* labels[] = {"Normalized 16-bit (0-1 across depth range)", "Millimeters (32-bit float)", "Meters (32-bit float)"};
        menu("Depthoutput", "Depth Output", "Normalized", 3, names, labels, page0,
             "Normalized = 16-bit 0-1 across the depth range. Millimeters / Meters = 32-bit float.");
    }
    toggle("Manualdepththresh", "Manual Depth Range", 0.0, page0);
    {
        OP_NumericParameter depthThreshMinParam;
        depthThreshMinParam.name = "Depththreshmin";
        depthThreshMinParam.label = "Depth Range Min (mm)";
        depthThreshMinParam.page = page0;
        depthThreshMinParam.defaultValues[0] = 0.0;
        depthThreshMinParam.minValues[0] = 0.0;
        depthThreshMinParam.maxValues[0] = 8000.0;
        depthThreshMinParam.minSliders[0] = 0.0;
        depthThreshMinParam.maxSliders[0] = 8000.0;
        depthThreshMinParam.clampMins[0] = true;
        manager->appendFloat(depthThreshMinParam);
    }
    {
        OP_NumericParameter depthThreshMaxParam;
        depthThreshMaxParam.name = "Depththreshmax";
        depthThreshMaxParam.label = "Depth Range Max (mm)";
        depthThreshMaxParam.page = page0;
        depthThreshMaxParam.defaultValues[0] = 5000.0;
        depthThreshMaxParam.minValues[0] = 0.0;
        depthThreshMaxParam.maxValues[0] = 8000.0;
        depthThreshMaxParam.minSliders[0] = 0.0;
        depthThreshMaxParam.maxSliders[0] = 8000.0;
        depthThreshMaxParam.clampMins[0] = true;
        manager->appendFloat(depthThreshMaxParam);
    }
    // Sentinels for invalid data. Alpha (point cloud) / 0-masking is still the authoritative validity
    // signal; these only decide what value lands in the dead pixels for pipelines that cannot read alpha.
    {
        OP_NumericParameter unknownDepthParam;
        unknownDepthParam.name = "Unknowndepth";
        unknownDepthParam.label = "Unknown Depth Value";
        unknownDepthParam.page = page0;
        unknownDepthParam.defaultValues[0] = 0.0;
        unknownDepthParam.minSliders[0] = -1.0;
        unknownDepthParam.maxSliders[0] = 10000.0;
        layout(unknownDepthParam, "Written to depth pixels with no reading or outside the depth range.", false);
        manager->appendFloat(unknownDepthParam);
    }

    // --- Point cloud ---
    toggle("Pcflipx", "Point Cloud Flip X", 0.0, page0,
           "Native frame: +Y up, +Z away from the sensor, X follows the mirrored image.", true);
    toggle("Pcflipy", "Point Cloud Flip Y", 0.0, page0);
    toggle("Pcflipz", "Point Cloud Flip Z", 0.0, page0);
    {
        OP_NumericParameter unknownPointParam;
        unknownPointParam.name = "Unknownpoint";
        unknownPointParam.label = "Unknown Point Value";
        unknownPointParam.page = page0;
        for (int i = 0; i < 3; ++i) {
            unknownPointParam.defaultValues[i] = 0.0;
            unknownPointParam.minSliders[i] = -10.0;
            unknownPointParam.maxSliders[i] = 100.0;
        }
        layout(unknownPointParam, "XYZ written to invalid points; their alpha is always 0.", false);
        manager->appendXYZ(unknownPointParam);
    }

    // ---------------
    // RESOLUTION PAGE
    // ---------------
    // Presets only; every size is a downscale of the native frame, the field of view
    // never changes. Depth and point cloud use nearest-neighbour, RGB and IR use vImage
    // high-quality resampling.
    const char* page1 = "Resolution";
    header("Hdrresnote", "Downscale presets. Field of view never changes.", page1);

    header("Kinectv1resolution", "Kinect v1 (native 640x480)", page1);
    {
        const char* names[]  = {"640x480", "320x240", "160x120"};
        menu("V1rgbres",   "RGB Resolution",   "640x480", 3, names, names, page1);
        menu("V1depthres", "Depth Resolution", "640x480", 3, names, names, page1);
    }

    header("Kinectv2resolution", "Kinect v2 (native RGB 1920x1080, depth/IR 512x424)", page1);
    {
        const char* rgbNames[]   = {"1920x1080", "1280x720", "960x540", "640x360"};
        const char* depthNames[] = {"512x424", "256x212", "128x106"};
        menu("V2rgbres",   "RGB Resolution",         "1280x720", 4, rgbNames,   rgbNames,   page1);
        menu("V2depthres", "Depth Resolution",       "512x424",  3, depthNames, depthNames, page1);
        menu("V2pcres",    "Point Cloud Resolution", "512x424",  3, depthNames, depthNames, page1);
        menu("V2irres",    "IR Resolution",          "512x424",  3, depthNames, depthNames, page1);
    }
    header("Hdrresnote2", "Registered depth / point cloud follow the RGB resolution.", page1);

    // ----------
    // ABOUT PAGE
    // ----------
    const char* page2 = "About";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION + " - by @stosumarte";
    header("Version", versionLabel.c_str(), page2);
    header("Hdrcontrib", "Point cloud registration, float depth, POP workflow (v1.1): Dean Cheesman", page2);
    header("Updateheader", "Visit the following URL to check for updates:", page2, /*section=*/true);
    {
        OP_StringParameter updateUrlParam;
        updateUrlParam.name = "Updateurl";
        updateUrlParam.label = "Copy this -> ";
        updateUrlParam.page = page2;
        updateUrlParam.defaultValue = "github.com/stosumarte/FreenectTD/releases/latest";
        manager->appendString(updateUrlParam);
    }
}

// TD - Cook every frame
void FreenectTOP::getGeneralInfo(TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) {
    // Cook every frame, but only while something downstream uses the output (a viewer, a Render
    // Select feeding a displayed chain, a Null TOP with its display flag on). Cooking unconditionally
    // was tried and made whole networks sluggish, so leave the pull model in charge.
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

// Reads td.licenses.isNonCommercial from TouchDesigner's own Python. The C++ SDK has no license query,
// so the CPython functions are looked up at runtime in the TD process: nothing is linked at build time,
// and a TD that ships a different Python version still works. Returns nullopt if anything is missing.
// Must run on TD's main thread (execute does): Python can't be entered from the device threads.
static std::optional<bool> readIsNonCommercial() {
    using Fn_IsInit  = int (*)();
    using Fn_Ensure  = int (*)();
    using Fn_Release = void (*)(int);
    using Fn_Import  = void* (*)(const char*);
    using Fn_GetAttr = void* (*)(void*, const char*);
    using Fn_IsTrue  = int (*)(void*);
    using Fn_DecRef  = void (*)(void*);
    using Fn_ErrClr  = void (*)();
    auto isInit  = reinterpret_cast<Fn_IsInit>(dlsym(RTLD_DEFAULT, "Py_IsInitialized"));
    auto ensure  = reinterpret_cast<Fn_Ensure>(dlsym(RTLD_DEFAULT, "PyGILState_Ensure"));
    auto release = reinterpret_cast<Fn_Release>(dlsym(RTLD_DEFAULT, "PyGILState_Release"));
    auto import  = reinterpret_cast<Fn_Import>(dlsym(RTLD_DEFAULT, "PyImport_ImportModule"));
    auto getAttr = reinterpret_cast<Fn_GetAttr>(dlsym(RTLD_DEFAULT, "PyObject_GetAttrString"));
    auto isTrue  = reinterpret_cast<Fn_IsTrue>(dlsym(RTLD_DEFAULT, "PyObject_IsTrue"));
    auto decRef  = reinterpret_cast<Fn_DecRef>(dlsym(RTLD_DEFAULT, "Py_DecRef"));
    auto errClr  = reinterpret_cast<Fn_ErrClr>(dlsym(RTLD_DEFAULT, "PyErr_Clear"));
    if (!isInit || !ensure || !release || !import || !getAttr || !isTrue || !decRef || !errClr || !isInit()) {
        return std::nullopt;
    }

    std::optional<bool> result;
    const int gil = ensure();
    void* td = import("td");
    void* licenses = td ? getAttr(td, "licenses") : nullptr;
    void* nonCommercial = licenses ? getAttr(licenses, "isNonCommercial") : nullptr;
    if (nonCommercial) {
        const int value = isTrue(nonCommercial); // -1 on error
        if (value >= 0) {
            result = (value == 1);
        }
    }
    errClr();
    if (nonCommercial) decRef(nonCommercial);
    if (licenses) decRef(licenses);
    if (td) decRef(td);
    release(gil);
    return result;
}

// Constructor for FreenectTOP
FreenectTOP::FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context)
    : fntdNodeInfo(info),
      fntdContext(context)
{
    // Do not initialize device here, will be done in execute
}

// Process-wide device ownership: only one FreenectTOP instance may open the Kinect
std::mutex   FreenectTOP::deviceOwnerMutex;
FreenectTOP* FreenectTOP::deviceOwner = nullptr;

std::thread      FreenectTOP::deviceThread;
std::atomic<int> FreenectTOP::deviceJobsPending{0};

// Queue a job behind whatever the device thread is doing; each job's thread joins the previous one first.
// Only called from the cook thread and destructors (TD's main thread), so `deviceThread` itself needs no lock.
void FreenectTOP::runOnDeviceThread(std::function<void()> job) {
    ++deviceJobsPending;
    deviceThread = std::thread([prev = std::move(deviceThread), job = std::move(job)]() mutable {
        if (prev.joinable()) {
            prev.join();
        }
        job();
        --deviceJobsPending;
    });
}

bool FreenectTOP::claimDevice() {
    std::lock_guard<std::mutex> lock(deviceOwnerMutex);
    if (deviceOwner == nullptr) deviceOwner = this;
    return deviceOwner == this;
}

void FreenectTOP::releaseDevice() {
    bool wasOwner = false;
    {
        std::lock_guard<std::mutex> lock(deviceOwnerMutex);
        if (deviceOwner == this) {
            deviceOwner = nullptr;
            wasOwner = true;
        }
    }
    if (wasOwner) {
        // Close the device so the next node that becomes active can open it.
        fn2_cleanupDevice();
        fn1_cleanupDevice();
        lastDeviceType.clear();
    }
}

// Destructor for FreenectTOP
FreenectTOP::~FreenectTOP() {
    LOG("[FreenectTOP] Destructor called, cleaning up devices");
    fn2_cleanupDevice();
    fn1_cleanupDevice();
    releaseDevice();
    if (deviceThread.joinable()) {
        deviceThread.join();
    }
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::fn1_initDevice(bool ir) {
    // Crucial: Device init start
    LOG("[FreenectTOP] fn1_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    // freenect_init only calls libusb_init and returns its error code
    int res = freenect_init(&fn1_ctx, nullptr);
    if (res < 0) {
        initError = "Couldn't initialize libfreenect (" + std::string(libusb_error_name(res)) + ")";
        LOG("[FreenectTOP] fn1_initDevice: " + initError);
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
        initError = "No Kinect v1 devices found";
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        return false;
    }

    try {
        fn1_rgbReady = false;
        fn1_depthReady = false;
        fn1_device = new MyFreenectDevice(fn1_ctx, 0, fn1_rgbReady, fn1_depthReady, ir);
        fn1_device->start();
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
                fn1_device->applyStreamModes();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            LOG("[FreenectTOP] fn1_eventThread: exiting");
        });
    } catch (...) {
        initError = "Failed to start Kinect v1 device";
        fn1_runEvents = false;
        if (fn1_eventThread.joinable()) {
            fn1_eventThread.join();
        }
        delete fn1_device;
        fn1_device = nullptr;
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
        return false;
    }
    LOG("[FreenectTOP] fn1_initDevice: success");
    return true;
}

// Cleanup for Kinect v1 (libfreenect)
// Called on the cook thread: execute stops using the device right away, the close runs on the device thread.
void FreenectTOP::fn1_cleanupDevice() {
    fn1_initSuccess = false;
    fn1_lastAppliedTilt = std::numeric_limits<float>::quiet_NaN();
    fn1_lastDepthTime = {};
    runOnDeviceThread([this]() {
        LOG("[FreenectTOP] fn1_cleanupDevice: start");
        fn1_runEvents = false;
        if (fn1_eventThread.joinable()) {
            fn1_eventThread.join();
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
        initError.clear();
        LOG("[FreenectTOP] fn1_cleanupDevice: end");
    });
}

// True if a Kinect v2 is attached at less than USB 3 speed; libfreenect2 crashes the process opening it there.
// ponytail: checks every Kinect v2, not just the one libfreenect2 picks; match by serial if multi-Kinect setups matter
static bool fn2_onSlowUSB() {
    libusb_context* usb = nullptr;
    if (libusb_init(&usb) != 0) {
        return false;
    }
    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usb, &list);
    bool slow = false;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        // Same IDs libfreenect2 enumerates: Kinect for Windows v2 and Xbox One Kinect
        bool isKinect2 = desc.idVendor == 0x045E && (desc.idProduct == 0x02C4 || desc.idProduct == 0x02D8);
        int speed = libusb_get_device_speed(list[i]);
        // LIBUSB_SPEED_UNKNOWN is let through so an unreported speed doesn't block a working setup
        if (isKinect2 && speed != LIBUSB_SPEED_UNKNOWN && speed < LIBUSB_SPEED_SUPER) {
            slow = true;
        }
    }
    if (count >= 0) {
        libusb_free_device_list(list, 1);
    }
    libusb_exit(usb);
    return slow;
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
            // libfreenect2 enumeration opens the device, which makes it flicker out of other scans,
            // so it is skipped while the device sits on USB 2 and can't be used anyway
            fn2_slowUSB = fn2_onSlowUSB();
            if (!fn2_slowUSB.load()) {
                libfreenect2::Freenect2 ctx;
                fn2_deviceAvailable = (ctx.enumerateDevices() > 0);
            }
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
    if (fn2_slowUSB.load()) {
        initError = "Kinect v2 is on a USB 2 port, connect it to USB 3";
        LOG("[FreenectTOP] fn2_initDevice: (end) device on USB 2");
        return false;
    }
    if (!fn2_deviceAvailable.load()) {
        LOG("[FreenectTOP] fn2_initDevice: no device available");
        initError = "No Kinect v2 devices found";
        return false;
    }
    if (fn2_ctx) {
        LOG("[FreenectTOP] fn2_initDevice: (end) already initialized");
        return true;
    }
    fn2_ctx = new libfreenect2::Freenect2();
    LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_ctx after = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_ctx)));
    if (fn2_ctx->enumerateDevices() == 0) {
        initError = "No Kinect v2 devices found";
        delete fn2_ctx;
        fn2_ctx = nullptr;
        LOG("[FreenectTOP] fn2_initDevice: (end) no devices - fn2_ctx deleted and set to nullptr");
        return false;
    }
    fn2_serial = fn2_ctx->getDefaultDeviceSerialNumber();
    try {
        fn2_pipeline = new libfreenect2::CpuPacketPipeline();
    } catch (...) {
        initError = "Couldn't create CPU pipeline for Kinect v2";
        LOG(std::string("[FreenectTOP] fn2_initDevice: fn2_pipeline after fail = ") + std::to_string(reinterpret_cast<uintptr_t>(fn2_pipeline)));
    }
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    LOG(std::string("[FreenectTOP] fn2_initDevice: openDevice returned dev = ") + std::to_string(reinterpret_cast<uintptr_t>(dev)));
    if (!dev) {
        initError = "Failed to open Kinect v2 device, is it on a USB 3 port?";
        delete fn2_device;
        // openDevice owns the pipeline and already deleted it on failure; deleting it again crashes TD
        fn2_pipeline = nullptr;
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
        initError = "Failed to start Kinect v2 device";
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
    const bool ir = streamEnabledIR; // open in the requested video mode
    runOnDeviceThread([this, ir]() {
        fn1_initSuccess = fn1_initDevice(ir);
    });
}

// Threaded initialization for Kinect v2
void FreenectTOP::fn2_startInitThread() {
    runOnDeviceThread([this]() {
        fn2_initSuccess = fn2_initDevice();
    });
}

// Cleanup for Kinect v2 (libfreenect2)
void FreenectTOP::fn2_cleanupDevice() {
    fn2_initSuccess = false;
    fn2_lastPointCloudSeq = NO_POINT_CLOUD; // a new device starts counting depth frames from 0 again
    runOnDeviceThread([this]() {
        LOG("[FreenectTOP] fn2_cleanupDevice: start");
        fn2_stopEnumThread();
        std::lock_guard<std::mutex> lock(freenectMutex);
        if (fn2_device) {
            delete fn2_device;
            fn2_device = nullptr;
            LOG("[FreenectTOP] fn2_device deleted");
        }
        fn2_pipeline = nullptr; // owned by the libfreenect2 device
        if (fn2_ctx) {
            delete fn2_ctx;
            fn2_ctx = nullptr;
            LOG("[FreenectTOP] fn2_ctx deleted");
        }
        initError.clear();
        LOG("[FreenectTOP] fn2_cleanupDevice: end");
    });
}

// Execute method for Kinect v1 (libfreenect)
void FreenectTOP::fn1_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    // fn1_device belongs to the device thread until an init succeeds
    if (!fn1_initSuccess.load()) {
        if (deviceJobsPending.load() == 0) {
            errorString = initError; // result of the last attempt, empty before the first
            LOG("[FreenectTOP] executeV1: device not ready, queueing init");
            fn1_startInitThread();
        }
        uploadFallbackBuffer();
        return;
    }
    
    // An unplugged v1 just stops sending frames, so treat 2 s without depth as a disconnect.
    // The depth stream always runs, even when its output is off.
    const auto now = std::chrono::steady_clock::now();
    if (fn1_depthReady.exchange(false) || fn1_lastDepthTime == std::chrono::steady_clock::time_point{}) {
        fn1_lastDepthTime = now;
    } else if (now - fn1_lastDepthTime > std::chrono::seconds(2)) {
        LOG("[FreenectTOP] executeV1: no depth frames for 2 s, closing the device");
        fn1_cleanupDevice();
        uploadFallbackBuffer();
        return;
    }
    
    fn1_device->setResolutions(fn1_colorW, fn1_colorH, fn1_depthW, fn1_depthH, fn1_irW, fn1_irH);
    
    // Only touch the motor when the value actually changes: setting tilt every
    // cook stalls the v1 depth stream (see #21).
    if (std::isnan(fn1_lastAppliedTilt) || std::fabs(fn1_tilt - fn1_lastAppliedTilt) > 0.01f) {
        try {
            fn1_device->setTiltDegrees(fn1_tilt);
            fn1_lastAppliedTilt = fn1_tilt;
        } catch (const std::exception& e) {
            errorString = "Failed to set tilt angle: " + std::string(e.what());
            fn1_cleanupDevice();
            return;
        }
    }
    
    // Set color type based on parameter (not implemented yet, default to RGB)
    fn1_colorType colorType = fn1_colorType::RGB; // Default to RGB
    
    // RGB and IR share the v1 video stream, so RGB is blank while IR is on
    fn1_device->setIR(streamEnabledIR);
    
    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext && !streamEnabledIR ? fntdContext->createOutputBuffer(fn1_colorW * fn1_colorH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    
    // --- Color frame ---
    std::vector<uint8_t> colorFrame;
    if (streamEnabledIR) {
        uploadFallbackBuffer(0);
    } else if (colorFrameBuffer && fn1_device->getColorFrame(colorFrame, colorType)) {
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
    
    // --- IR frame ---
    if (streamEnabledIR) {
        std::vector<uint16_t> irFrame;
        if (fntdContext && fn1_device->getIRFrame(irFrame)) {
            TD::OP_SmartRef<TD::TOP_Buffer> irFrameBuffer = fntdContext->createOutputBuffer(irFrame.size() * sizeof(uint16_t), TD::TOP_BufferFlags::None, nullptr);
            if (irFrameBuffer) {
                errorString.clear();
                std::memcpy(irFrameBuffer->data, irFrame.data(), irFrame.size() * sizeof(uint16_t));
                TD::TOP_UploadInfo info;
                info.textureDesc.width = MyFreenectDevice::WIDTH;
                info.textureDesc.height = MyFreenectDevice::HEIGHT;
                info.textureDesc.texDim = TD::OP_TexDim::e2D;
                info.textureDesc.pixelFormat = TD::OP_PixelFormat::Mono16Fixed;
                info.colorBufferIndex = 3;
                info.firstPixel = TD::TOP_FirstPixel::TopLeft;
                output->uploadBuffer(&irFrameBuffer, info, nullptr);
            }
        }
    } else {
        uploadFallbackBuffer(3);
    }
}
    
// Execute method for Kinect v2 (libfreenect2)
void FreenectTOP::fn2_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs) {
    // fn2_device belongs to the device thread until an init succeeds
    if (!fn2_initSuccess.load()) {
        if (deviceJobsPending.load() == 0) {
            errorString = initError; // result of the last attempt, empty before the first
            LOG("[FreenectTOP] executeV2: device not ready, queueing init");
            fn2_startInitThread();
        }
        uploadFallbackBuffer();
        return;
    }

    // Check if device was disconnected
    if (!fn2_deviceAvailable.load()) {
        fn2_cleanupDevice();
        uploadFallbackBuffer();
        return;
    }

    fn2_device->setResolutions(fn2_colorW, fn2_colorH, fn2_depthW, fn2_depthH, fn2_pcW, fn2_pcH, fn2_irW, fn2_irH);
    // libfreenect2 clips depth to 4.5 m by default; use the TOP's depth range instead so v2 can see up to ~8 m
    fn2_device->setDepthRange(depthThreshMin, depthThreshMax);

    // Create output buffers
    TD::OP_SmartRef<TD::TOP_Buffer> colorFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_colorW * fn2_colorH * 4, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
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
    // Only rebuilt and uploaded when a new depth frame has arrived: TD cooks faster than the
    // Kinect delivers depth, and an output that isn't uploaded keeps its previous texture.
    const uint64_t depthSeq = fn2_device->getDepthSeq();
    if (streamEnabledPC && depthSeq != fn2_lastPointCloudSeq) {
        TD::OP_SmartRef<TD::TOP_Buffer> pointCloudFrameBuffer = fntdContext ? fntdContext->createOutputBuffer(fn2_pcW * fn2_pcH * 4 * sizeof(float), TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
        std::vector<float> pointCloudFrame;
        if (pointCloudFrameBuffer && fn2_device->getPointCloudFrame(pointCloudFrame, pcSpace, depthThreshMin, depthThreshMax, pcFlipX, pcFlipY, pcFlipZ, unknownPoint)) {
            errorString.clear();
            fn2_lastPointCloudSeq = depthSeq;
            std::memcpy(pointCloudFrameBuffer->data, pointCloudFrame.data(), fn2_pcW * fn2_pcH * 4 * sizeof(float));
            TD::TOP_UploadInfo info;
            info.textureDesc.width = fn2_pcW;
            info.textureDesc.height = fn2_pcH;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA32Float;
            info.colorBufferIndex = 2;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            output->uploadBuffer(&pointCloudFrameBuffer, info, nullptr);
        } else {
            errorString = "Failed to get point cloud frame from Kinect v2";
        }
    } else if (!streamEnabledPC) {
        uploadFallbackBuffer(2);
        fn2_lastPointCloudSeq = NO_POINT_CLOUD; // re-enabling uploads straight away instead of waiting for the next frame
    }

    // --- Registered color (index 4) and depth-to-color UV map (index 5) ---
    if (streamEnabledRegColor || streamEnabledUV) {
        const int regWidth = MyFreenect2Device::DEPTH_WIDTH;
        const int regHeight = MyFreenect2Device::DEPTH_HEIGHT;
        std::vector<uint8_t> regColor;
        std::vector<float> regUV;
        if (fntdContext && fn2_device->getRegisteredColorFrame(regColor, regUV)) {
            TD::TOP_UploadInfo info;
            info.textureDesc.width = regWidth;
            info.textureDesc.height = regHeight;
            info.textureDesc.texDim = TD::OP_TexDim::e2D;
            info.firstPixel = TD::TOP_FirstPixel::TopLeft;
            if (streamEnabledRegColor) {
                TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext->createOutputBuffer(regWidth * regHeight * 4, TD::TOP_BufferFlags::None, nullptr);
                if (buf) {
                    std::memcpy(buf->data, regColor.data(), regWidth * regHeight * 4);
                    info.textureDesc.pixelFormat = TD::OP_PixelFormat::RGBA8Fixed;
                    info.colorBufferIndex = 4;
                    output->uploadBuffer(&buf, info, nullptr);
                }
            }
            if (streamEnabledUV) {
                TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext->createOutputBuffer(regWidth * regHeight * 4 * sizeof(float), TD::TOP_BufferFlags::None, nullptr);
                if (buf) {
                    std::memcpy(buf->data, regUV.data(), regWidth * regHeight * 4 * sizeof(float));
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

    // Read the license whenever the TOP becomes active (including the first cook), so a key installed
    // while TD runs takes effect by toggling Active. If it can't be read, assume Non-Commercial:
    // a too-large output corrupts there, while limiting a commercial license only costs resolution.
    if (isActive && !wasActive) {
        const std::optional<bool> isNonCommercial = readIsNonCommercial();
        licenseKnown = isNonCommercial.has_value();
        nonCommercial = isNonCommercial.value_or(true);
        LOG(std::string("[FreenectTOP] license: ") + (licenseKnown ? (nonCommercial ? "Non-Commercial" : "Commercial or Pro") : "unknown"));
    }
    wasActive = isActive;
    
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
    unknownDepth = static_cast<float>(inputs->getParDouble("Unknowndepth"));
    for (int i = 0; i < 3; ++i) unknownPoint[i] = static_cast<float>(inputs->getParDouble("Unknownpoint", i));
    
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
    auto parseRes = [&](const char* parName, int& width, int& height, int defaultWidth, int defaultHeight) {
        const char* preset = inputs->getParString(parName);
        int parsedWidth = 0, parsedHeight = 0;
        if (preset && std::sscanf(preset, "%dx%d", &parsedWidth, &parsedHeight) == 2 && parsedWidth > 0 && parsedHeight > 0) {
            width = parsedWidth;
            height = parsedHeight;
        } else {
            width = defaultWidth;
            height = defaultHeight;
        }
    };
    parseRes("V1rgbres",   fn1_colorW, fn1_colorH, MyFreenectDevice::WIDTH, MyFreenectDevice::HEIGHT);
    parseRes("V1depthres", fn1_depthW, fn1_depthH, MyFreenectDevice::WIDTH, MyFreenectDevice::HEIGHT);
    parseRes("V2rgbres",   fn2_colorW, fn2_colorH, MyFreenect2Device::SCALED_WIDTH, MyFreenect2Device::SCALED_HEIGHT);
    parseRes("V2depthres", fn2_depthW, fn2_depthH, MyFreenect2Device::DEPTH_WIDTH, MyFreenect2Device::DEPTH_HEIGHT);
    parseRes("V2pcres",    fn2_pcW,    fn2_pcH,    MyFreenect2Device::DEPTH_WIDTH, MyFreenect2Device::DEPTH_HEIGHT);
    parseRes("V2irres",    fn2_irW,    fn2_irH,    MyFreenect2Device::IR_WIDTH,    MyFreenect2Device::IR_HEIGHT);
    // Non-Commercial TouchDesigner is limited to 1280x1280 and doesn't scale a C++ TOP's larger outputs
    // itself, so cap the only stream that exceeds it (Registered depth and point cloud follow RGB below)
    fn2_rgbLimited = nonCommercial && (fn2_colorW > 1280 || fn2_colorH > 1280);
    if (fn2_rgbLimited) {
        fn2_colorW = MyFreenect2Device::SCALED_WIDTH;
        fn2_colorH = MyFreenect2Device::SCALED_HEIGHT;
    }
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
    dynamicParameterEnable("Unknownpoint", false, true);
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
        releaseDevice(); // let another FreenectTOP take the device
        return;
    } else if (const char* format = inputs->getParString("Depthformat");
               devType == "Kinect v1" && format && std::string(format) == "Rawundistorted") {
        // A single menu entry can't be disabled, so say what happens instead
        warningString = "Raw undistorted is Kinect v2 only; Kinect v1 uses Raw";
    } else if (devType == "Kinect v2" && fn2_rgbLimited) {
        warningString = licenseKnown ? "Non-Commercial license: Kinect v2 RGB is limited to 1280x720"
                                     : "Couldn't detect the TouchDesigner license: Kinect v2 RGB is limited to 1280x720";
    } else {
        warningString.clear();
    }

    // Only one FreenectTOP per process may talk to the device. A second active node would
    // fight the first for the USB device and both would stall, so it stays idle with an error.
    if (!claimDevice()) {
        errorString = "Another FreenectTOP is already active. Only one can run at a time; turn Active off on the other node first.";
        uploadFallbackBuffer();
        return;
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
            if (d <= 0.0f) {
                dst[i] = static_cast<uint16_t>(std::clamp(unknownDepth, 0.0f, 1.0f) * 65535.0f + 0.5f);
            } else {
                const float normalized = std::clamp((d - depthThreshMin) / denom, 0.0f, 1.0f);
                dst[i] = static_cast<uint16_t>(normalized * 65535.0f + 0.5f);
            }
        }
    } else {
        float* dst = static_cast<float*>(buf->data);
        const float scale = (depthOutput == depthOutputEnum::Meters) ? 0.001f : 1.0f;
        #pragma omp parallel for if(pixelCount > 100000)
        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i] = (depthMM[i] > 0.0f) ? depthMM[i] * scale : unknownDepth;
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
    for (int i = 0; i < NUM_OUTPUTS; ++i) {
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

    if (targetIndex >= 0 && targetIndex < NUM_OUTPUTS) {
        info.colorBufferIndex = targetIndex;
        myCurrentOutput->uploadBuffer(&fallbackBuffers[targetIndex], info, nullptr);
    } else {
        for (int i = 0; i < NUM_OUTPUTS; ++i) {
            info.colorBufferIndex = i;
            myCurrentOutput->uploadBuffer(&fallbackBuffers[i], info, nullptr);
        }
    }
}
