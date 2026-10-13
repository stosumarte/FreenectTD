//
// FreenectTOP.h
// FreenectTOP
//
// Created by marte
//

#pragma once

#include "logger.h"

// Disable warnings from TouchDesigner headers for non-standard offsetof usage
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"

#include <touchdesigner_2025/TOP_CPlusPlusBase.h>

#pragma clang diagnostic pop

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <limits>
#include <functional>
#include <chrono>
#include <map>

#include "FreenectCommon.h"
#include "FreenectV1.h"
#include "FreenectV2.h"

struct libusb_context;

class FreenectTOP : public TD::TOP_CPlusPlusBase {
    
public:
    FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context);
    virtual ~FreenectTOP();

    void getGeneralInfo   (TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) override;
    void execute          (TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) override;
    void setupParameters  (TD::OP_ParameterManager* manager, void*) override;
    void pulsePressed     (const char* name, void*) override;
    void buildDynamicMenu (const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info, void*) override;

private:
    const TD::OP_NodeInfo*                  fntdNodeInfo;
    TD::TOP_Context*                        fntdContext;
    
    // V1 device members
    freenect_context*                       fn1_ctx = nullptr;
    libusb_context*                         fn1_usb = nullptr; // libusb context fn1_ctx runs on, ours (see fn1_initDevice)
    MyFreenectDevice*                       fn1_device = nullptr;
    std::atomic<bool>                       fn1_rgbReady{false};
    std::atomic<bool>                       fn1_depthReady{false};
    std::atomic<bool>                       fn1_runEvents{false};
    std::thread                             fn1_eventThread;

    // V2 device members
    libfreenect2::Freenect2*                fn2_ctx = nullptr;
    MyFreenect2Device*                      fn2_device = nullptr;
    libfreenect2::PacketPipeline*           fn2_pipeline = nullptr;
    std::string                             fn2_openedId; // Kinect fn2_device is open on; device thread only
    std::atomic<bool>                       fn2_rgbReady{false};
    std::atomic<bool>                       fn2_depthReady{false};
    std::atomic<bool>                       fn2_irReady{false};

    // Device open/close, the same for both versions except the library calls in fn1_/fn2_initDevice
    std::atomic<bool> initSuccess{false}; // fn1_device / fn2_device belong to the device thread until it's true
    bool fn1_initDevice(bool ir, const std::string& id);
    void fn1_closeContext();
    bool fn2_initDevice(const std::string& id);
    void startInit(kinectVersionEnum version);
    void cleanupDevice();
    bool deviceReady(kinectVersionEnum version);
    std::mutex freenectMutex;
    std::mutex fn1_eventMutex; // Separate mutex for v1 event thread
    
    // Frames of each version, once deviceReady says the device is open
    void fn1_execute(TD::TOP_Output* output);
    void fn2_execute(TD::TOP_Output* output);
    void uploadBuffer(TD::TOP_Output* output, TD::OP_SmartRef<TD::TOP_Buffer>& buf, int index, int width, int height, TD::OP_PixelFormat format);
    bool uploadFrame(TD::TOP_Output* output, int index, const void* data, int width, int height, TD::OP_PixelFormat format, size_t bytesPerPixel);
    void uploadFallbackBuffer(int targetIndex = -1);
    void uploadDepthFrame(TD::TOP_Output* output, const std::vector<float>& depthMM, int width, int height);
    
    // Each Kinect is open in one FreenectTOP at a time (see claimDevice in FreenectTOP.cpp)
    static std::mutex deviceOwnerMutex;                       // guards deviceOwners and pickedDevices
    static std::map<std::string, FreenectTOP*> deviceOwners;  // Kinect id -> node that has it open
    static std::map<FreenectTOP*, std::string> pickedDevices; // active nodes with a Kinect picked in Device, not Auto
    FreenectTOP* claimDevice(const std::string& id);
    void releaseDevice();
    bool freeForAuto(const std::string& id) const;
    
    // Error/warning string handling
    std::string errorString;
    bool nonCommercial = true;   // TouchDesigner license, read each time the TOP becomes active
    bool wasActive = false;      // Active on the previous cook
    bool licenseKnown = false;   // false if it couldn't be read; nonCommercial is then assumed
    std::string warningString;
    void getErrorString(TD::OP_String* error, void* reserved1) override;
    void getWarningString(TD::OP_String* warning, void* reserved1) override;

    // Info popup, Info DAT and Info CHOP: which Kinect the node is on (also the one Auto picked) and its status
    int32_t getNumInfoCHOPChans(void* reserved1) override;
    void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved1) override;
    bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved1) override;
    void getInfoDATEntries(int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries, void* reserved1) override;
    void getInfoPopupString(TD::OP_String* info, void* reserved1) override;
    std::string deviceStatus() const;
    
    // Current output pointer
    TD::TOP_Output* myCurrentOutput = nullptr;

    static constexpr int NUM_OUTPUTS = 6; // 0 RGB, 1 depth, 2 point cloud, 3 IR, 4 registered color, 5 depth->color UV
    std::array<TD::OP_SmartRef<TD::TOP_Buffer>, NUM_OUTPUTS> fallbackBuffers;

    // Device init and cleanup run as jobs on a background thread so opening or closing the device
    // never stalls the cook. Each instance has its own thread, so deleting a node only waits for its
    // own jobs. deviceIOMutex makes jobs from all instances take turns, so a close finishes before the next open.
    std::thread      deviceThread;
    std::atomic<int> deviceJobsPending{0}; // queued or running jobs; 0 = idle
    static std::mutex deviceIOMutex;
    void runOnDeviceThread(std::function<void()> job);
    std::string initError; // written by init jobs, read by the cook thread only while the device thread is idle
    std::string lastInitError; // cook thread's copy of initError from the last finished init attempt
    int initFailures = 0;      // init attempts in a row that ended with an error; shown from the second one
    
    // Parameters variables
    float fn1_tilt = 0.0f;
    float fn1_lastAppliedTilt = std::numeric_limits<float>::quiet_NaN();
    uint64_t lastDepthSeq = 0;                             // v2 depthSeq seen at lastDepthTime
    std::chrono::steady_clock::time_point lastDepthTime;   // last cook a new depth frame had arrived (watchdog in deviceReady)
    int depthFrames = 0;                                   // depth frames since fpsWindowStart, for depthFps
    float depthFps = 0.0f;                                 // Info CHOP depth_fps, updated about once a second
    std::chrono::steady_clock::time_point fpsWindowStart;
    
    // Output sizes, set each cook (see execute): RGB depends on the license, depth and point cloud on Format
    int fn2_colorW = 0, fn2_colorH = 0;
    int fn2_depthW = 0, fn2_depthH = 0;
    int fn2_pcW = 0, fn2_pcH = 0;
    static constexpr uint64_t NO_POINT_CLOUD = std::numeric_limits<uint64_t>::max();
    uint64_t fn2_lastPointCloudSeq = NO_POINT_CLOUD; // depthSeq of the last uploaded point cloud
    
    bool manualDepthThresh;
    float depthThreshMin, depthThreshMax;
    depthFormatEnum depthFormat = depthFormatEnum::Raw;
    std::string deviceId;     // Kinect this node uses (KinectUSB::id), set each cook from Device; empty = none
    std::string lastDeviceId; // used to tear down the device when deviceId changes
    std::string autoDevice;   // Kinect id Device = Auto is on; kept while that Kinect stays plugged in
    depthOutputEnum depthOutput = depthOutputEnum::Normalized;
    pcSpaceEnum pcSpace = pcSpaceEnum::DepthCamera;
    bool pcFlipX = false, pcFlipY = false, pcFlipZ = false;
    float unknownDepth = 0.0f;                 // written to invalid depth pixels, in output units
    float unknownPoint[3] = {0.0f, 0.0f, 0.0f}; // written to XYZ of invalid points
    
    bool streamEnabledIR;
    bool streamEnabledDepth;
    bool streamEnabledPC;
    bool streamEnabledRegColor = false;
    bool streamEnabledUV = false;
    
};
