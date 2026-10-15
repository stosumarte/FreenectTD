//
// FreenectTOP.cpp
// FreenectTOP
//
// Created by marte on 01/05/2025.
//

#include "FreenectTOP.h"
#include "USBScan.h"
#include "UpdateCheck.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
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
#include <libfreenect2/logger.h>

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
        (void)info->setAPIVersion(TOPCPlusPlusAPIVersion);
        info->executeMode = TOP_ExecuteMode::CPUMem;
        info->customOPInfo.opType->setString("Freenect");
        info->customOPInfo.opLabel->setString("Freenect");
        info->customOPInfo.opIcon->setString("FNT");
        info->customOPInfo.authorName->setString("Marte Tagliabue");
        info->customOPInfo.authorEmail->setString("ciao@marte.ee");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 0;
        info->customOPInfo.majorVersion = 1;
        info->customOPInfo.minorVersion = 2;
        info->customOPInfo.opHelpURL->setString("https://github.com/stosumarte/FreenectTD");
        
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

    // Separator line above the parameter and Alt+hover help text
    auto layout = [](auto& param, const char* help, bool section) {
        param.help = help;
        param.section = section;
    };

    // Small helpers so every parameter is declared the same way
    auto header = [&](const char* name, const char* label, const char* page, bool section = false,
                      const char* help = nullptr) {
        OP_StringParameter headerParam;
        headerParam.name = name;
        headerParam.label = label;
        headerParam.page = page;
        layout(headerParam, help, section);
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
        // Entries come from the shared USB scan (see buildDynamicMenu); the Kinect also sets the version
        OP_StringParameter deviceParam;
        deviceParam.name = "Device";
        deviceParam.label = "Device";
        deviceParam.page = page0;
        deviceParam.defaultValue = "Auto";
        layout(deviceParam, "Auto opens the first Kinect plugged in. Pick a Kinect to always use that one.", false);
        manager->appendDynamicStringMenu(deviceParam);
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
    // One Depth Format menu drives both: Registered puts depth AND the point cloud in the
    // color camera (aligned to RGB); Raw keeps them in the depth camera.
    {
        const char* names[]  = {"Raw", "Rawundistorted", "Registered"};
        const char* labels[] = {"Raw", "Raw undistorted", "Registered (aligned to RGB)"};
        menu("Depthformat", "Depth Format", "Raw", 3, names, labels, page0,
             "Registered aligns depth and point cloud to the RGB image; Raw keeps them in the depth camera.", true);
    }
    {
        const char* names[]  = {"Normalized", "Millimeters", "Meters"};
        const char* labels[] = {"Normalized (16-bit)", "Millimeters (32-bit float)", "Meters (32-bit float)"};
        menu("Depthoutput", "Depth Output", "Normalized", 3, names, labels, page0,
             "Normalized = 16-bit. Millimeters / Meters = 32-bit float.");
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

    // ----------
    // ABOUT PAGE
    // ----------
    const char* page1 = "About";
    std::string versionLabel = std::string("FreenectTD v") + FREENECTTOP_VERSION + " by @stosumarte";
    header("Version", versionLabel.c_str(), page1);
    // Short labels: header text sets the width of the parameter name column
    header("Contributors", "Contributors", page1, /*section=*/true);
    header("Contrib1", "@dcheesman", page1, false, "Point cloud registration, float depth, POP workflow");
    header("Contrib2", "@gcarizza", page1, false, "Kinect v1 depth stream fix (#21)");
    header("Contrib3", "@fedevoxlive", page1, false, "Kinect v1 IR streaming (#22)");
    auto pulse = [&](const char* name, const char* label, const char* help, bool section = false) {
        OP_NumericParameter pulseParam;
        pulseParam.name = name;
        pulseParam.label = label;
        pulseParam.page = page1;
        layout(pulseParam, help, section);
        manager->appendPulse(pulseParam);
    };
    pulse("Checkupdates", "Check for Updates", "Ask GitHub for the latest release and show the result in a dialog.", true);
    pulse("Openreleases", "Open Releases Page", "Open the FreenectTD releases page in the browser.");
    pulse("Donate", "Support FreenectTD", "Open the FreenectTD donation page in the browser.", true);
}

void FreenectTOP::pulsePressed(const char* name, void*) {
    if (std::strcmp(name, "Checkupdates") == 0) {
        checkForUpdates(FREENECTTOP_VERSION);
    } else if (std::strcmp(name, "Openreleases") == 0) {
        openReleasesPage();
    } else if (std::strcmp(name, "Donate") == 0) {
        openDonationPage();
    }
}

// "v2-One:504521442542" -> "v2 (One) [504521442542]"
static std::string deviceLabel(const std::string& id) {
    const size_t colon = id.find(':');
    return id.find('-') == 2 && colon != std::string::npos
        ? id.substr(0, 2) + " (" + id.substr(3, colon - 3) + ") [" + id.substr(colon + 1) + "]" : id;
}

// Name in status and error messages: "Kinect v2 (One) [504521442542]"
static std::string kinectName(const std::string& id) {
    return "Kinect " + deviceLabel(id);
}

// Version from a Kinect id ("v2-One:..."); None for an empty id
static kinectVersionEnum versionOf(const std::string& id) {
    return id.starts_with("v2-") ? kinectVersionEnum::V2
         : id.starts_with("v1-") ? kinectVersionEnum::V1
                                 : kinectVersionEnum::None;
}

// A Kinect present in the shared USB scan, so a missing or unplugged one shows up right away without opening it
static std::optional<KinectUSB> findKinect(const std::string& id) {
    for (const KinectUSB& kinect : usbKinects()) {
        if (kinect.id() == id && kinect.present) return kinect;
    }
    return std::nullopt;
}

// libfreenect2 crashes the process opening a Kinect v2 there.
// LIBUSB_SPEED_UNKNOWN is let through so an unreported speed doesn't block a working setup.
static bool onUSB2(const KinectUSB& kinect) {
    return kinect.speed != LIBUSB_SPEED_UNKNOWN && kinect.speed < LIBUSB_SPEED_SUPER;
}

// Status shown in the info popup and Info DAT, as of the last cook
std::string FreenectTOP::deviceStatus() const {
    if (!wasActive) return "inactive";
    if (deviceId.empty()) return "no Kinect";
    if (initSuccess.load()) return "streaming";
    return errorString.empty() ? "initializing" : "error";
}

// Info CHOP channels, numbers only: streaming and connected (0/1), version (0 = none, 1, 2), depth_fps,
// and for a Kinect v2 (0 on v1) rgb_fps and process_ms (worker time per frame set)
int32_t FreenectTOP::getNumInfoCHOPChans(void*) {
    return 6;
}

void FreenectTOP::getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void*) {
    switch (index) {
        case 0:
            chan->name->setString("streaming");
            chan->value = (wasActive && initSuccess.load()) ? 1.0f : 0.0f;
            break;
        case 1:
            chan->name->setString("connected");
            chan->value = findKinect(deviceId) ? 1.0f : 0.0f;
            break;
        case 2:
            chan->name->setString("version");
            switch (versionOf(deviceId)) {
                case kinectVersionEnum::None: chan->value = 0.0f; break;
                case kinectVersionEnum::V1:   chan->value = 1.0f; break;
                case kinectVersionEnum::V2:   chan->value = 2.0f; break;
            }
            break;
        case 3:
            chan->name->setString("depth_fps");
            chan->value = depthFps;
            break;
        case 4:
            chan->name->setString("rgb_fps");
            chan->value = rgbFps;
            break;
        case 5:
            chan->name->setString("process_ms");
            chan->value = processMs;
            break;
    }
}

// Info DAT rows. id is the Device menu value, so a script can pin the Kinect Auto picked:
// op('freenect1').par.Device = op('info1')['id', 1]
bool FreenectTOP::getInfoDATSize(TD::OP_InfoDATSize* infoSize, void*) {
    infoSize->rows = 4;
    infoSize->cols = 2;
    infoSize->byColumn = false;
    return true;
}

void FreenectTOP::getInfoDATEntries(int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries, void*) {
    // pipeline: where Kinect v2 depth is decoded (Metal, or CPU if Metal isn't usable); empty for v1,
    // which decodes depth itself, and until the v2 is open
    const bool v2Open = initSuccess.load() && versionOf(deviceId) == kinectVersionEnum::V2;
    const std::string rows[4][2] = {
        {"kinect", deviceId.empty() ? "" : deviceLabel(deviceId)},
        {"id", deviceId},
        {"status", deviceStatus()},
        {"pipeline", v2Open ? fn2_depthPipeline : ""},
    };
    if (index < 0 || index >= 4 || nEntries < 2) {
        return;
    }
    entries->values[0]->setString(rows[index][0].c_str());
    entries->values[1]->setString(rows[index][1].c_str());
}

// Middle-click popup on the node
void FreenectTOP::getInfoPopupString(TD::OP_String* info, void*) {
    const std::string text = "Kinect: " + (deviceId.empty() ? std::string("none") : deviceLabel(deviceId))
                           + "\nStatus: " + deviceStatus();
    info->setString(text.c_str());
}

// Device menu: Auto, then every Kinect in the shared USB scan (never scans here, TD calls this on the
// main thread). A picked Kinect that's unplugged stays listed, so the menu still shows what the node uses.
// Labels never say whether a Kinect is connected: TD rebuilds entries only when the menu opens, and the
// closed menu keeps the label from then. The node's error says it instead.
void FreenectTOP::buildDynamicMenu(const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info, void*) {
    if (std::strcmp(info->name, "Device") != 0) {
        return;
    }
    info->addMenuEntry("Auto", "Auto");
    const char* c = inputs->getParString("Device");
    const std::string picked = c ? c : "Auto";
    bool pickedListed = (picked == "Auto");
    for (const KinectUSB& kinect : usbKinects()) {
        const std::string id = kinect.id();
        info->addMenuEntry(id.c_str(), deviceLabel(id).c_str());
        pickedListed |= (id == picked);
    }
    if (!pickedListed) {
        info->addMenuEntry(picked.c_str(), deviceLabel(picked).c_str());
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
    startUSBScan();
}

// Process-wide device ownership: each Kinect is open in one FreenectTOP at a time
std::mutex FreenectTOP::deviceOwnerMutex;
std::map<std::string, FreenectTOP*> FreenectTOP::deviceOwners;
std::map<FreenectTOP*, std::string> FreenectTOP::pickedDevices;

std::mutex FreenectTOP::deviceIOMutex;

// Queue a job behind whatever this instance's device thread is doing; each job's thread joins the previous one first.
// Only called from the cook thread and the destructor (TD's main thread), so `deviceThread` itself needs no lock.
void FreenectTOP::runOnDeviceThread(std::function<void()> job) {
    ++deviceJobsPending;
    deviceThread = std::thread([this, prev = std::move(deviceThread), job = std::move(job)]() mutable {
        if (prev.joinable()) {
            prev.join();
        }
        {
            std::lock_guard<std::mutex> lock(deviceIOMutex);
            job();
        }
        --deviceJobsPending;
    });
}

// Claims a Kinect for this node. Returns the node that already has it open, or nullptr if this node has it now.
FreenectTOP* FreenectTOP::claimDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(deviceOwnerMutex);
    FreenectTOP*& owner = deviceOwners[id];
    if (owner == nullptr) owner = this;
    return owner == this ? nullptr : owner;
}

void FreenectTOP::releaseDevice() {
    bool wasOwner = false;
    {
        std::lock_guard<std::mutex> lock(deviceOwnerMutex);
        wasOwner = std::erase_if(deviceOwners, [this](const auto& owner) { return owner.second == this; }) > 0;
    }
    if (wasOwner) {
        // Close the device so the next node that becomes active can open it.
        cleanupDevice();
        lastDeviceId.clear();
    }
}

// Whether Auto may use a Kinect: not open in another node, and not picked in the Device menu of another
// active node (that node gets it; an Auto node already on it moves on). Caller holds deviceOwnerMutex.
bool FreenectTOP::freeForAuto(const std::string& id) const {
    const auto owner = deviceOwners.find(id);
    if (owner != deviceOwners.end() && owner->second != this) {
        return false;
    }
    return std::none_of(pickedDevices.begin(), pickedDevices.end(), [&](const auto& picked) {
        return picked.first != this && picked.second == id;
    });
}

// Destructor for FreenectTOP
FreenectTOP::~FreenectTOP() {
    LOG("[FreenectTOP] Destructor called, cleaning up devices");
    {
        std::lock_guard<std::mutex> lock(deviceOwnerMutex);
        pickedDevices.erase(this);
    }
    releaseDevice(); // closes the device if this instance owns it
    if (deviceThread.joinable()) {
        deviceThread.join();
    }
    stopUSBScan();
}

// Shuts libfreenect down, then the libusb context it ran on: freenect_shutdown leaves a context it was given open
void FreenectTOP::fn1_closeContext() {
    if (fn1_ctx) {
        freenect_shutdown(fn1_ctx);
        fn1_ctx = nullptr;
    }
    if (fn1_usb) {
        libusb_exit(fn1_usb);
        fn1_usb = nullptr;
    }
}

// Init for Kinect v1 (libfreenect)
bool FreenectTOP::fn1_initDevice(bool ir, const std::string& id) {
    LOG("[FreenectTOP] fn1_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    initError.clear(); // deviceReady reports a missing device itself, from the USB scan
    const std::optional<KinectUSB> kinect = findKinect(id);
    if (!kinect) {
        LOG("[FreenectTOP] fn1_initDevice: no device available");
        return false;
    }
    // Undoes a partial init
    auto fail = [&](const std::string& reason) {
        initError = "Couldn't open " + kinectName(id) + ": " + reason;
        LOG("[FreenectTOP] fn1_initDevice: " + initError);
        fn1_runEvents = false;
        if (fn1_eventThread.joinable()) {
            fn1_eventThread.join();
        }
        delete fn1_device;
        fn1_device = nullptr;
        fn1_closeContext();
        return false;
    };
    // libfreenect runs on a libusb context of ours, so the camera's index is counted in the same
    // device list libfreenect opens it from
    int res = libusb_init(&fn1_usb);
    if (res == 0 && FNTD_DEBUG) {
        // libusb's warnings and errors for this context, sent to our log (its DEBUG level logs every transfer)
        libusb_set_log_cb(fn1_usb, [](libusb_context*, enum libusb_log_level, const char* message) {
            LOG(std::string("[libusb] ") + message);
        }, LIBUSB_LOG_CB_CONTEXT);
        libusb_set_option(fn1_usb, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    }
    if (res == 0) {
        res = freenect_init(&fn1_ctx, fn1_usb);
    }
    if (res < 0) {
        return fail(libusb_error_name(res));
    }
    
    // libfreenect's log, sent to ours (a Debug build writes it to /tmp/FreenectTOP_*.log). DEBUG covers
    // opening the camera, motor and audio parts, without the per-packet messages of the levels above it.
    freenect_set_log_level(fn1_ctx, FNTD_DEBUG ? FREENECT_LOG_DEBUG : FREENECT_LOG_WARNING);
    freenect_set_log_callback(fn1_ctx, [](freenect_context*, freenect_loglevel, const char* message) {
        LOG(std::string("[libfreenect] ") + message);
    });
    
    // Upload firmware to Kinect v1 if needed (models 1473 and Kinect for Windows)
    freenect_set_fw_address_nui
        (fn1_ctx, ofxKinectExtras::getFWData1473(), ofxKinectExtras::getFWSize1473());
    freenect_set_fw_address_k4w
        (fn1_ctx, ofxKinectExtras::getFWDatak4w(), ofxKinectExtras::getFWSizek4w());

    const int index = kinect1Index(fn1_usb, *kinect);
    if (index < 0) {
        return fail("libfreenect can't find it");
    }

    try {
        fn1_rgbReady = false;
        fn1_depthReady = false;
        // By index, not freenect_open_device_by_camera_serial: that opens every Kinect v1 to read serials,
        // including ones other nodes are streaming, and then opens by index anyway
        fn1_device = new MyFreenectDevice(fn1_ctx, index, fn1_rgbReady, fn1_depthReady, ir);
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
        return fail("couldn't start its streams");
    }
    LOG("[FreenectTOP] fn1_initDevice: success");
    return true;
}

// libfreenect2's log, sent to ours. It's also the only way to tell whether MetalPacketPipeline fell back
// to the CPU: the depth processor it picked is an internal class.
namespace {
thread_local bool fn2_metalFellBack = false; // set while a pipeline is created on this thread

class Freenect2Logger : public libfreenect2::Logger {
public:
    Freenect2Logger() { level_ = Info; }
    void log(Level, const std::string& message) override {
        // NOTE: the warning MetalPacketPipeline logs in our libfreenect2 fork (src/packet_pipeline.cpp)
        if (message.find("using the CPU instead") != std::string::npos) {
            fn2_metalFellBack = true;
        }
        LOG("[libfreenect2] " + message);
    }
};
}

// Init for Kinect v2 (libfreenect2)
bool FreenectTOP::fn2_initDevice(const std::string& id) {
    // setGlobalLogger deletes the logger it replaces, so set ours once
    static std::once_flag loggerSet;
    std::call_once(loggerSet, [] { libfreenect2::setGlobalLogger(new Freenect2Logger()); });

    LOG("[FreenectTOP] fn2_initDevice: starting");
    std::lock_guard<std::mutex> lock(freenectMutex);
    initError.clear(); // deviceReady reports a missing or USB 2 device itself, from the USB scan
    const std::optional<KinectUSB> kinect = findKinect(id);
    if (!kinect || onUSB2(*kinect)) {
        LOG("[FreenectTOP] fn2_initDevice: (end) no device available, or on USB 2");
        return false;
    }
    if (fn2_ctx) {
        LOG("[FreenectTOP] fn2_initDevice: (end) already initialized");
        return true;
    }
    // Undoes a partial init. The pipeline is never deleted here: openDevice deletes it when it fails,
    // and once opened the libfreenect2 device owns it and deletes it with fn2_ctx.
    auto fail = [&](const std::string& reason) {
        initError = "Couldn't open " + kinectName(id) + ": " + reason;
        LOG("[FreenectTOP] fn2_initDevice: (end) " + initError);
        if (fn2_device) {
            expectReboot(fn2_openedId); // closing it reboots it
        }
        delete fn2_device;
        fn2_device = nullptr;
        fn2_pipeline = nullptr;
        delete fn2_ctx;
        fn2_ctx = nullptr;
        return false;
    };
    if (kinect->serial.empty()) {
        return fail("no serial number, unplug it and plug it back in");
    }
    fn2_ctx = new libfreenect2::Freenect2();
    if (fn2_ctx->enumerateDevices() == 0) {
        return fail("libfreenect2 can't find it");
    }
    try {
        // Depth decoding on the GPU; libfreenect2 falls back to the CPU if Metal isn't usable
        fn2_metalFellBack = false;
        fn2_pipeline = new libfreenect2::MetalPacketPipeline();
        fn2_depthPipeline = fn2_metalFellBack ? "CPU" : "Metal";
    } catch (...) {
        return fail("couldn't create the depth pipeline");
    }
    libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(kinect->serial, fn2_pipeline);
    if (!dev) {
        return fail("libfreenect2 can't open it");
    }
    fn2_device = new MyFreenect2Device(dev, fntdContext);
    fn2_openedId = id;
    if (!fn2_device->start()) {
        return fail("couldn't start its streams");
    }

    LOG("[FreenectTOP] fn2_initDevice: end (success)");
    return true;
}

// Opens the node's Kinect on the device thread; a v1 opens in the requested video mode
void FreenectTOP::startInit(kinectVersionEnum version) {
    runOnDeviceThread([this, version, ir = streamEnabledIR, id = deviceId]() {
        initSuccess = (version == kinectVersionEnum::V2) ? fn2_initDevice(id) : fn1_initDevice(ir, id);
    });
}

// Closes whichever Kinect is open. Called on the cook thread: execute stops using the device right away,
// the close runs on the device thread.
void FreenectTOP::cleanupDevice() {
    initSuccess = false;
    fn1_lastAppliedTilt = std::numeric_limits<float>::quiet_NaN();
    lastDepthTime = {};
    lastDepthSeq = 0; // a new device starts counting depth frames from 0 again
    depthFrames = 0;
    depthFps = 0.0f;
    rgbFps = 0.0f;
    processMs = 0.0f;
    fpsWindowStart = {};
    runOnDeviceThread([this]() {
        LOG("[FreenectTOP] cleanupDevice: start");
        initSuccess = false; // an init job queued before this one may have set it back to true
        fn1_runEvents = false;
        if (fn1_eventThread.joinable()) {
            fn1_eventThread.join();
        }
        std::lock_guard<std::mutex> lock(freenectMutex);
        delete fn1_device;
        fn1_device = nullptr;
        fn1_closeContext();
        if (fn2_device) {
            expectReboot(fn2_openedId); // closing it reboots it
            delete fn2_device;
            fn2_device = nullptr;
        }
        fn2_pipeline = nullptr; // owned by the libfreenect2 device
        delete fn2_ctx;
        fn2_ctx = nullptr;
        initError.clear();
        LOG("[FreenectTOP] cleanupDevice: end");
    });
}

// Before any frames are read, the same for both versions: opens the Kinect in the background, says why
// it isn't streaming yet, and closes it when it's unplugged or stops sending depth. True once it's open.
bool FreenectTOP::deviceReady(kinectVersionEnum version) {
    const std::optional<KinectUSB> kinect = findKinect(deviceId);
    if (!initSuccess.load()) {
        if (deviceJobsPending.load() == 0) {
            lastInitError = initError; // result of the last attempt, empty before the first
            initFailures = initError.empty() ? 0 : initFailures + 1;
            LOG("[FreenectTOP] deviceReady: device not ready, queueing init");
            startInit(version);
        }
        // The USB scan says right away whether the Kinect is there; init results can lag behind a close
        // (~4 s for a v2 on macOS)
        if (!kinect) {
            errorString = kinectName(deviceId) + " is not connected";
        } else if (version == kinectVersionEnum::V2 && onUSB2(*kinect)) {
            errorString = kinectName(deviceId) + " is on a USB 2 port, connect it to USB 3";
        } else if (initFailures >= 2) {
            // A single failure is shown as still initializing: right after one Kinect v2 closes, opening
            // another one can fail once in libfreenect2 and work on the next try
            errorString = lastInitError;
        } else {
            errorString.clear();
            warningString = "Initializing " + kinectName(deviceId);
        }
        uploadFallbackBuffer();
        return false;
    }

    if (!kinect) {
        LOG("[FreenectTOP] deviceReady: device unplugged, closing it");
        cleanupDevice();
        uploadFallbackBuffer();
        return false;
    }

    // An open Kinect can stop sending frames without an error (a stalled v1; a v2 once after a replug),
    // so treat 3 s without depth as a disconnect and reopen it. The depth stream always runs, even when
    // its output is off.
    // A v2 counts its depth frames; a v1 only flags that one arrived since the last cook
    int newFrames = 0;
    if (version == kinectVersionEnum::V2) {
        const uint64_t seq = fn2_device->getDepthSeq();
        newFrames = (seq > lastDepthSeq) ? static_cast<int>(seq - lastDepthSeq) : 0;
        lastDepthSeq = seq;
    } else {
        newFrames = fn1_depthReady.exchange(false) ? 1 : 0;
    }
    const bool newDepth = (newFrames > 0);
    const auto now = std::chrono::steady_clock::now();

    // Info CHOP depth_fps, over windows of about a second
    depthFrames += newFrames;
    const uint64_t colorFrames = (version == kinectVersionEnum::V2) ? fn2_device->getColorFrames() : 0;
    if (fpsWindowStart == std::chrono::steady_clock::time_point{}) {
        fpsWindowStart = now;
        colorFramesAtWindow = colorFrames;
    } else if (now - fpsWindowStart >= std::chrono::seconds(1)) {
        const float seconds = std::chrono::duration<float>(now - fpsWindowStart).count();
        depthFps = depthFrames / seconds;
        rgbFps = (colorFrames - colorFramesAtWindow) / seconds;
        processMs = (version == kinectVersionEnum::V2) ? fn2_device->getProcessMs() : 0.0f;
        depthFrames = 0;
        colorFramesAtWindow = colorFrames;
        fpsWindowStart = now;
    }

    if (newDepth || lastDepthTime == std::chrono::steady_clock::time_point{}) {
        lastDepthTime = now;
    } else if (now - lastDepthTime > std::chrono::seconds(3)) {
        LOG("[FreenectTOP] deviceReady: no depth frames for 3 s, closing the device");
        cleanupDevice();
        uploadFallbackBuffer();
        return false;
    }
    return true;
}

// Kinect v1 frames (libfreenect)
void FreenectTOP::fn1_execute(TD::TOP_Output* output) {
    constexpr int W = MyFreenectDevice::WIDTH, H = MyFreenectDevice::HEIGHT;

    // Only touch the motor when the value actually changes: setting tilt every
    // cook stalls the v1 depth stream (see #21).
    if (std::isnan(fn1_lastAppliedTilt) || std::fabs(fn1_tilt - fn1_lastAppliedTilt) > 0.01f) {
        try {
            fn1_device->setTiltDegrees(fn1_tilt);
            fn1_lastAppliedTilt = fn1_tilt;
        } catch (const std::exception& e) {
            errorString = "Couldn't tilt " + kinectName(deviceId) + ": " + e.what();
            cleanupDevice();
            return;
        }
    }

    // RGB and IR share the v1 video stream, so RGB is blank while IR is on
    fn1_device->setIR(streamEnabledIR);

    std::vector<uint8_t> colorFrame;
    if (streamEnabledIR) {
        uploadFallbackBuffer(0);
    } else if (fn1_device->getColorFrame(colorFrame, fn1_colorType::RGB)) {
        uploadFrame(output, 0, colorFrame.data(), W, H, TD::OP_PixelFormat::RGBA8Fixed, 4);
    }

    std::vector<float> depthFrame; // millimetres, 0 = invalid
    if (!streamEnabledDepth) {
        uploadFallbackBuffer(1);
    } else if (fn1_device->getDepthFrame(depthFrame, depthFormat, depthThreshMin, depthThreshMax)) {
        uploadDepthFrame(output, depthFrame, W, H);
    }

    std::vector<uint16_t> irFrame;
    if (!streamEnabledIR) {
        uploadFallbackBuffer(3);
    } else if (fn1_device->getIRFrame(irFrame)) {
        uploadFrame(output, 3, irFrame.data(), W, H, TD::OP_PixelFormat::Mono16Fixed, 2);
    }
}

// Kinect v2 frames (libfreenect2)
void FreenectTOP::fn2_execute(TD::TOP_Output* output) {
    // The device's worker thread makes the outputs from each frame set; the cook only uploads them.
    // An output that isn't uploaded keeps its previous texture.
    V2Settings settings;
    settings.colorW = fn2_colorW;
    settings.colorH = fn2_colorH;
    settings.depthFormat = depthFormat;
    settings.depthOutput = depthOutput;
    settings.threshMin = depthThreshMin;
    settings.threshMax = depthThreshMax;
    settings.unknownDepth = unknownDepth;
    std::copy(std::begin(unknownPoint), std::end(unknownPoint), settings.unknownPoint);
    settings.depth = streamEnabledDepth;
    settings.pointCloud = streamEnabledPC;
    settings.ir = streamEnabledIR;
    settings.regColor = streamEnabledRegColor;
    settings.uv = streamEnabledUV;
    settings.flipX = pcFlipX;
    settings.flipY = pcFlipY;
    settings.flipZ = pcFlipZ;
    fn2_device->setSettings(settings);

    MyFreenect2Device::Images images;
    fn2_device->takeImages(images);
    const bool enabled[MyFreenect2Device::NUM_OUTPUTS] = {
        true, streamEnabledDepth, streamEnabledPC, streamEnabledIR, streamEnabledRegColor, streamEnabledUV
    };
    for (int i = 0; i < MyFreenect2Device::NUM_OUTPUTS; ++i) {
        if (!enabled[i]) {
            uploadFallbackBuffer(i); // also drops an image made just before the output was turned off
        } else if (images[i].buf) {
            uploadBuffer(output, images[i].buf, i, images[i].width, images[i].height, images[i].format);
        }
    }
}

// Main execution method
void FreenectTOP::execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) {
    myCurrentOutput = output;
    
    // If errorString is set, LOG it
    if ( FNTD_DEBUG == 1 && !errorString.empty() && errorString != "No Kinect found" && !errorString.ends_with(" is not connected")) {
        LOG("[FreenectTOP] ERROR: " + errorString);
    }
    
    // Validate inputs
    if (!inputs) {
        errorString = "Inputs is null";
        return;
    }
    
    // Get parameters
    bool isActive = (inputs && inputs->getParInt("Active") != 0);

    // The Kinect this node uses, which also sets the version: the one picked in Device, or for Auto the one
    // it's already on while that stays listed (also while a v2 reboots after a close) and free for it, else
    // the first free one plugged in (see freeForAuto). Empty = none.
    const char* deviceCStr = inputs->getParString("Device");
    const std::string picked = deviceCStr ? deviceCStr : "Auto";
    const std::vector<KinectUSB> kinects = usbKinects();
    {
        std::lock_guard<std::mutex> lock(deviceOwnerMutex);
        if (isActive && picked != "Auto") {
            pickedDevices[this] = picked;
        } else {
            pickedDevices.erase(this);
        }
        if (picked == "Auto") {
            const bool listed = std::any_of(kinects.begin(), kinects.end(), [&](const KinectUSB& k) { return k.id() == autoDevice; });
            if (!listed || !freeForAuto(autoDevice)) {
                const auto first = std::find_if(kinects.begin(), kinects.end(), [&](const KinectUSB& k) {
                    return k.present && freeForAuto(k.id());
                });
                autoDevice = first != kinects.end() ? first->id() : "";
            }
        }
    }
    deviceId = (picked == "Auto") ? autoDevice : picked;
    const kinectVersionEnum version = versionOf(deviceId);

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
        else if (depthFormatStr == "Rawundistorted" && version == kinectVersionEnum::V2) depthFormat = depthFormatEnum::RawUndistorted;
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
        pcFlipX = (inputs->getParInt("Pcflipx") != 0);
        pcFlipY = (inputs->getParInt("Pcflipy") != 0);
        pcFlipZ = (inputs->getParInt("Pcflipz") != 0);
    }
    
    fn1_tilt = static_cast<float>(inputs->getParDouble("Tilt"));
    
    // Kinect v2 RGB is native 1920x1080, except on Non-Commercial TouchDesigner: it's limited to 1280x1280
    // and doesn't scale a C++ TOP's larger outputs itself, so RGB is 1280x720 there. Registered depth and
    // the color-space point cloud are pixel-aligned with RGB and follow it; every other output is native.
    fn2_colorW = nonCommercial ? MyFreenect2Device::SCALED_WIDTH : MyFreenect2Device::RGB_WIDTH;
    fn2_colorH = nonCommercial ? MyFreenect2Device::SCALED_HEIGHT : MyFreenect2Device::RGB_HEIGHT;
    
    // Version-specific parameters; greyed out while the version is unknown (Auto with no Kinect)
    inputs->enablePar("Tilt", version == kinectVersionEnum::V1);
    for (const char* name : {"Enablepointcloud", "Enableregcolor", "Enableuv", "Pcflipx", "Pcflipy", "Pcflipz", "Unknownpoint"}) {
        inputs->enablePar(name, version == kinectVersionEnum::V2);
    }
    
    // Enable/disable depthThreshMin/Max based on manualDepthThresh
    if (!manualDepthThresh) {
        inputs->enablePar("Depththreshmin", false);
        inputs->enablePar("Depththreshmax", false);
    } else {
        inputs->enablePar("Depththreshmin", true);
        inputs->enablePar("Depththreshmax", true);
    }
    
    if (!manualDepthThresh && version != kinectVersionEnum::None) {
        depthThreshMin = (version == kinectVersionEnum::V1) ? 400.0f : 100.0f;
        depthThreshMax = 4500.0f;
    }

    // Check if the plugin is active
    if (!isActive) {
        warningString = "Freenect TOP is inactive";
        uploadFallbackBuffer();
        errorString.clear();
        releaseDevice(); // let another FreenectTOP take the device
        return;
    } else if (const char* format = inputs->getParString("Depthformat");
               version == kinectVersionEnum::V1 && format && std::string(format) == "Rawundistorted") {
        // A single menu entry can't be disabled, so say what happens instead
        warningString = "Raw undistorted is Kinect v2 only; Kinect v1 uses Raw";
    } else if (version == kinectVersionEnum::V2 && nonCommercial) {
        warningString = licenseKnown ? "Non-Commercial license: Kinect v2 RGB is limited to 1280x720"
                                     : "Couldn't detect the TouchDesigner license: Kinect v2 RGB is limited to 1280x720";
    } else {
        warningString.clear();
    }

    // Each Kinect is open in one FreenectTOP at a time: a second node would fight the first for the USB
    // device and both would stall, so it stays idle with an error. When the Kinect changes (another one
    // picked, or Auto's unplugged or picked by another node), give up (and close) the old one first, so
    // it's free for another node even if this one can't claim the new one.
    if (deviceId != lastDeviceId) {
        releaseDevice();
        lastDeviceId = deviceId;
    }
    if (version == kinectVersionEnum::None) {
        const bool anyPlugged = std::any_of(kinects.begin(), kinects.end(), [](const KinectUSB& k) { return k.present; });
        errorString = anyPlugged ? "Every Kinect is in use by another Freenect TOP" : "No Kinect found";
        uploadFallbackBuffer();
        return;
    }
    if (const FreenectTOP* owner = claimDevice(deviceId)) {
        errorString = kinectName(deviceId) + " is in use by " + owner->fntdNodeInfo->opPath;
        uploadFallbackBuffer();
        return;
    }

    if (!deviceReady(version)) {
        return;
    }
    errorString.clear(); // streaming; the frame code sets an error if something fails
    switch (version) {
        case kinectVersionEnum::V1:   fn1_execute(output); break;
        case kinectVersionEnum::V2:   fn2_execute(output); break;
        case kinectVersionEnum::None: break; // handled above
    }
}

// Uploads an output buffer as image `index` (the Render Select TOP index)
void FreenectTOP::uploadBuffer(TD::TOP_Output* output, TD::OP_SmartRef<TD::TOP_Buffer>& buf, int index, int width, int height, TD::OP_PixelFormat format) {
    TD::TOP_UploadInfo info;
    info.textureDesc.width = width;
    info.textureDesc.height = height;
    info.textureDesc.texDim = TD::OP_TexDim::e2D;
    info.textureDesc.pixelFormat = format;
    info.colorBufferIndex = index;
    info.firstPixel = TD::TOP_FirstPixel::TopLeft;
    output->uploadBuffer(&buf, info, nullptr);
}

// Copies a frame into a new output buffer and uploads it; false if the buffer couldn't be created
bool FreenectTOP::uploadFrame(TD::TOP_Output* output, int index, const void* data, int width, int height, TD::OP_PixelFormat format, size_t bytesPerPixel) {
    const size_t bytes = static_cast<size_t>(width) * height * bytesPerPixel;
    TD::OP_SmartRef<TD::TOP_Buffer> buf = fntdContext ? fntdContext->createOutputBuffer(bytes, TD::TOP_BufferFlags::None, nullptr) : TD::OP_SmartRef<TD::TOP_Buffer>();
    if (!buf) {
        LOG("[FreenectTOP] uploadFrame: failed to create output buffer " + std::to_string(index));
        return false;
    }
    std::memcpy(buf->data, data, bytes);
    uploadBuffer(output, buf, index, width, height, format);
    return true;
}

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

    packDepth(depthMM.data(), pixelCount, buf->data, depthOutput, depthThreshMin, depthThreshMax, unknownDepth);
    uploadBuffer(output, buf, 1, width, height, packed16 ? TD::OP_PixelFormat::Mono16Fixed : TD::OP_PixelFormat::Mono32Float);
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

    for (int i = 0; i < NUM_OUTPUTS; ++i) {
        if (targetIndex < 0 || targetIndex == i) {
            uploadBuffer(myCurrentOutput, fallbackBuffers[i], i, fallbackWidth, fallbackHeight, TD::OP_PixelFormat::RGBA8Fixed);
        }
    }
}
