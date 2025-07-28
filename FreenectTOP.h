//
// FreenectTOP.h
// FreenectTOP
//
// Created by marte
//

#pragma once

#include "TOP_CPlusPlusBase.h"
#include "libfreenect.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

#include "FreenectV1.h"
#include "FreenectV2.h"

using namespace TD;

class FreenectTOP : public TOP_CPlusPlusBase {
public:
    FreenectTOP(const OP_NodeInfo* info, TOP_Context* context);
    virtual ~FreenectTOP();

    void getGeneralInfo   (TOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void*) override;
    void execute          (TOP_Output* output, const OP_Inputs* inputs, void*) override;
    void setupParameters  (OP_ParameterManager* manager, void*) override;
    void pulsePressed     (const char* name, void*) override;

private:
    const OP_NodeInfo*        myNodeInfo;
    TOP_Context*              myContext;
    
    // V1 device members
    freenect_context*         freenectContext;
    MyFreenectDevice*         device;
    std::atomic<bool>         firstRGBReady{false};
    std::atomic<bool>         firstDepthReady{false};
    std::vector<uint8_t>      lastRGB;
    std::vector<uint16_t>     lastDepth;
    std::atomic<bool>         runEvents{false};
    std::thread               eventThread;

    // Device type
    // 0 = Kinect v1
    // 1 = Kinect v2
    int deviceType = 0;
    std::string lastDeviceTypeStr;

    // V2 device members
    libfreenect2::Freenect2*       fn2_ctx = nullptr;
    MyFreenect2Device*             fn2_device = nullptr;
    libfreenect2::PacketPipeline*  fn2_pipeline = nullptr;
    std::string                    fn2_serial;
    std::atomic<bool>              fn2_rgbReady{false};
    std::atomic<bool>              fn2_depthReady{false};

    // Device init/cleanup methods
    bool initDeviceV1();
    void cleanupDeviceV1();
    bool initDeviceV2();
    void cleanupDeviceV2();
    std::mutex freenectMutex;
    
    // Execution methods for different device versions
    void executeV1(TOP_Output* output, const OP_Inputs* inputs);
    void executeV2(TOP_Output* output, const OP_Inputs* inputs);
};
