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

#include "FreenectTypes.h"
#include "FreenectV1.h"
#include "FreenectV2.h"

class FreenectTOP : public TD::TOP_CPlusPlusBase {
    
public:
    FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context);
    virtual ~FreenectTOP();

    void getGeneralInfo   (TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) override;
    void execute          (TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) override;
    void setupParameters  (TD::OP_ParameterManager* manager, void*) override;

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
    std::thread                             fn2_enumThread;
    std::atomic<bool>                       fn2_enumThreadRunning;

    // V2 background init members
    std::atomic<bool>                       fn2_initInProgress{false};
    //std::atomic<bool>                       fn2_initDone{false};
    std::atomic<bool>                       fn2_initSuccess{false};
    std::thread                             fn2_initThread;
    
    // Add declarations for v2 enumeration thread helpers
    void fn2_startEnumThread();
    void fn2_stopEnumThread();

    // Device init/cleanup methods
    bool fn1_initDevice();
    void fn1_cleanupDevice();
    bool fn2_initDevice();
    void fn2_cleanupDevice();
    void fn2_startInitThread();
    void fn2_waitInitThread();
    std::mutex freenectMutex;
    std::mutex fn1_eventMutex; // Separate mutex for v1 event thread
    
    // Execution methods for different device versions
    void fn1_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void fn2_execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void uploadFallbackBuffer(int targetIndex = -1);
    // One active FreenectTOP per process (see claimSensor in FreenectTOP.cpp)
    static std::mutex   sensorOwnerMutex;
    static FreenectTOP* sensorOwner;
    bool claimSensor();
    void releaseSensor();
    static constexpr int NUM_OUTPUTS = 6; // 0 RGB, 1 depth, 2 point cloud, 3 IR, 4 registered color, 5 depth->color UV
    void uploadDepthFrame(TD::TOP_Output* output, const std::vector<float>& depthMM, int width, int height);
    
    // Error/warning string handling
    std::string errorString;
    std::string warningString;
    void getErrorString(TD::OP_String* error, void* reserved1) override;
    void getWarningString(TD::OP_String* warning, void* reserved1) override;
    
    // Current output pointer
    TD::TOP_Output* myCurrentOutput = nullptr;

    std::array<TD::OP_SmartRef<TD::TOP_Buffer>, NUM_OUTPUTS> fallbackBuffers;

    // V1 background init members
    std::atomic<bool> fn1_initInProgress{false};
    std::atomic<bool> fn1_initSuccess{false};
    std::thread fn1_initThread;
    void fn1_startInitThread();
    
    // Parameters variables
    int fn1_colorW, fn1_colorH;
    int fn1_depthW, fn1_depthH;
    int fn1_irW, fn1_irH;
    float fn1_tilt = 0.0f;
    float fn1_lastAppliedTilt = std::numeric_limits<float>::quiet_NaN();
    
    int fn2_colorW, fn2_colorH;
    int fn2_depthW, fn2_depthH;
    int fn2_irW, fn2_irH;
    int fn2_pcW, fn2_pcH;
    
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
