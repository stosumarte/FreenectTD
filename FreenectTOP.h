//
// FreenectTOP.h
// FreenectTOP
//
// Created by marte
//

#pragma once

#include "TOP_CPlusPlusBase.h"
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

#include "FreenectV1.h"
#include "FreenectV2.h"

//using namespace TD;

class FreenectTOP : public TD::TOP_CPlusPlusBase {
public:
    FreenectTOP(const TD::OP_NodeInfo* info, TD::TOP_Context* context);
    virtual ~FreenectTOP();

    void getGeneralInfo   (TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs* inputs, void*) override;
    void execute          (TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*) override;
    void setupParameters  (TD::OP_ParameterManager* manager, void*) override;
    void pulsePressed     (const char* name, void*) override;

private:
    const TD::OP_NodeInfo*        myNodeInfo;
    TD::TOP_Context*              myContext;
    
    // V1 device members
    freenect_context*         freenectContext;
    MyFreenectDevice*         device;
    std::atomic<bool>         firstRGBReady{false};
    std::atomic<bool>         firstDepthReady{false};
    std::vector<uint8_t>      lastRGB;
    std::vector<uint16_t>     lastDepth;
    std::atomic<bool>         runV1Events{false};
    std::thread               eventThreadV1;

    // Device type
    // 0 = Kinect v1
    // 1 = Kinect v2
    int deviceType = 0;
    std::string lastDeviceTypeStr;

    // V2 device members
    libfreenect2::Freenect2*                fn2_ctx = nullptr;
    MyFreenect2Device*                      fn2_device = nullptr;
    libfreenect2::PacketPipeline*           fn2_pipeline = nullptr;
    //libfreenect2::SyncMultiFrameListener*   fn2_listener = nullptr;
    std::string                             fn2_serial;
    std::atomic<bool>                       fn2_rgbReady{false};
    std::atomic<bool>                       fn2_depthReady{false};
    std::atomic<bool>                       runV2Events{false};
    std::thread                             eventThreadV2;

    // Device init/cleanup methods
    bool initDeviceV1();
    void cleanupDeviceV1();
    bool initDeviceV2();
    void cleanupDeviceV2();
    std::mutex freenectMutex;
    
    // Execution methods for different device versions
    void executeV1(TD::TOP_Output* output, const TD::OP_Inputs* inputs);
    void executeV2(TD::TOP_Output* output, const TD::OP_Inputs* inputs);

    // Add declarations for v2 enumeration thread helpers
    void startV2EnumThread();
    void stopV2EnumThread();
    
    /*std::string errorString;

    void getErrorString(TD::OP_String *error, void *reserved1) override {
        if (!errorString.empty())
            error->setString(errorString.c_str());
    }*/
};
