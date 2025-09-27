//
// FreenectTOP.h
// FreenectTOP
//
// Created by marte
//

#pragma once

#include "logger.h"

#include "TOP_CPlusPlusBase.h"
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

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
    std::atomic<bool>                       fn2_runEvents{false};
    //libfreenect2::Registration*             fn2_registration = nullptr;
    std::thread                             fn2_eventThread;

    // V2 background init members
    std::atomic<bool>                       fn2_InitInProgress{false};
    //std::atomic<bool>                       fn2_InitDone{false};
    std::atomic<bool>                       fn2_InitSuccess{false};
    std::thread                             fn2_InitThread;
    
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
    void executeV1(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void executeV2(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void uploadFallbackBuffer();
    
    // Error string handling
    std::string errorString;
    void getErrorString(TD::OP_String* error, void* reserved1) override;
    
    // Current output pointer
    TD::TOP_Output* myCurrentOutput = nullptr;

    // Persistent fallback buffer for Metal safety
    //TD::OP_SmartRef<TD::TOP_Buffer> fallbackBuffer;

    // V1 background init members
    std::atomic<bool> fn1InitInProgress{false};
    std::atomic<bool> fn1InitSuccess{false};
    std::thread fn1InitThread;
    void startV1InitThread();
    void waitV1InitThread();
};
