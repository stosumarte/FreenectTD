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

// Include the correct TouchDesigner headers based on version
#if TD_VERSION == 2023
#include <touchdesigner_2023/TOP_CPlusPlusBase.h>
#elif TD_VERSION == 2025
#include <touchdesigner_2025/TOP_CPlusPlusBase.h>
#else
#include <touchdesigner_2023/TOP_CPlusPlusBase.h>
#endif

#pragma clang diagnostic pop

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <limits>
#include <functional>
#include <chrono>

#include "FreenectCommon.h"
#include "FreenectV1.h"
#include "FreenectV2.h"

class FreenectTOP : public TD::TOP_CPlusPlusBase {
    
public:
    FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context);
    virtual ~FreenectTOP();

    void getGeneralInfo   (TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) override;
    void execute          (TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) override;
    void setupParameters  (TD::OP_ParameterManager* manager, void*) override;
    void pulsePressed     (const char* name, void*) override;

private:
    const TD::OP_NodeInfo*                  fntdNodeInfo;
    TD::TOP_Context*                        fntdContext;
    
    // V1 device members
    freenect_context*                       fn1_ctx = nullptr;
    MyFreenectDevice*                       fn1_device = nullptr;
    std::atomic<bool>                       fn1_rgbReady{false};
    std::atomic<bool>                       fn1_depthReady{false};
    std::atomic<bool>                       fn1_runEvents{false};
    std::thread                             fn1_eventThread;

    // V2 device members
    libfreenect2::Freenect2*                fn2_ctx = nullptr;
    MyFreenect2Device*                      fn2_device = nullptr;
    libfreenect2::PacketPipeline*           fn2_pipeline = nullptr;
    std::string                             fn2_serial;
    std::atomic<bool>                       fn2_rgbReady{false};
    std::atomic<bool>                       fn2_depthReady{false};
    std::atomic<bool>                       fn2_irReady{false};
    std::atomic<bool>                       fn2_runEvents{false};
    std::thread                             fn2_eventThread;
    
    std::atomic<bool>                       fn2_deviceAvailable{false};
    std::atomic<bool>                       fn2_slowUSB{false};

    // V2 background init members
    std::atomic<bool>                       fn2_initSuccess{false};
    
    // Background USB scan: whether each Kinect version is plugged in, without opening it
    std::atomic<bool>                       fn1_deviceAvailable{false};
    std::thread                             usbScanThread;
    std::atomic<bool>                       usbScanRunning{false};
    void startUSBScanThread();
    void stopUSBScanThread();

    // Device init/cleanup methods
    bool fn1_initDevice(bool ir);
    void fn1_cleanupDevice();
    bool fn2_initDevice();
    void fn2_cleanupDevice();
    void fn2_startInitThread();
    std::mutex freenectMutex;
    std::mutex fn1_eventMutex; // Separate mutex for v1 event thread
    
    // Execution methods for different device versions
    void fn1_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void fn2_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void uploadFallbackBuffer(int targetIndex = -1);
    void uploadDepthFrame(TD::TOP_Output* output, const std::vector<float>& depthMM, int width, int height);
    
    // One active FreenectTOP per Kinect version (see claimDevice in FreenectTOP.cpp); index 0 = v1, 1 = v2
    static std::mutex   deviceOwnerMutex;
    static FreenectTOP* deviceOwner[2];
    bool claimDevice(bool v2);
    void releaseDevice();
    
    // Error/warning string handling
    std::string errorString;
    bool nonCommercial = true;   // TouchDesigner license, read each time the TOP becomes active
    bool wasActive = false;      // Active on the previous cook
    bool licenseKnown = false;   // false if it couldn't be read; nonCommercial is then assumed
    std::string warningString;
    void getErrorString(TD::OP_String* error, void* reserved1) override;
    void getWarningString(TD::OP_String* warning, void* reserved1) override;
    
    // Current output pointer
    TD::TOP_Output* myCurrentOutput = nullptr;

    static constexpr int NUM_OUTPUTS = 6; // 0 RGB, 1 depth, 2 point cloud, 3 IR, 4 registered color, 5 depth->color UV
    std::array<TD::OP_SmartRef<TD::TOP_Buffer>, NUM_OUTPUTS> fallbackBuffers;

    // V1 background init members
    std::atomic<bool> fn1_initSuccess{false};
    void fn1_startInitThread();

    // Device init and cleanup run as jobs on a background thread so opening or closing the device
    // never stalls the cook. Each instance has its own thread, so deleting a node only waits for its
    // own jobs. deviceIOMutex makes jobs from all instances take turns, so a close finishes before the next open.
    std::thread      deviceThread;
    std::atomic<int> deviceJobsPending{0}; // queued or running jobs; 0 = idle
    static std::mutex deviceIOMutex;
    void runOnDeviceThread(std::function<void()> job);
    std::string initError; // written by init jobs, read by the cook thread only while the device thread is idle
    std::string lastInitError; // cook thread's copy of initError from the last finished init attempt
    
    // Parameters variables
    float fn1_tilt = 0.0f;
    float fn1_lastAppliedTilt = std::numeric_limits<float>::quiet_NaN();
    std::chrono::steady_clock::time_point fn1_lastDepthTime; // last cook a depth frame had arrived
    
    // Output sizes, set each cook (see execute): RGB depends on the license, depth and point cloud on Format
    int fn2_colorW = 0, fn2_colorH = 0;
    int fn2_depthW = 0, fn2_depthH = 0;
    int fn2_pcW = 0, fn2_pcH = 0;
    static constexpr uint64_t NO_POINT_CLOUD = std::numeric_limits<uint64_t>::max();
    uint64_t fn2_lastPointCloudSeq = NO_POINT_CLOUD; // depthSeq of the last uploaded point cloud
    
    bool manualDepthThresh;
    float depthThreshMin, depthThreshMax;
    depthFormatEnum depthFormat = depthFormatEnum::Raw;
    std::string lastDeviceType = "Kinect v1"; // per instance; used to tear down devices when Hardware Version changes
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
