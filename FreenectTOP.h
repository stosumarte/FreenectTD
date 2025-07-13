#pragma once

#include "TOP_CPlusPlusBase.h"
#include "libfreenect.hpp"
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>

// v2 (libfreenect2) members
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/registration.h>

using namespace TD;

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    MyFreenectDevice(freenect_context* ctx, int index,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag);
    void VideoCallback(void* rgb, uint32_t) override;
    void DepthCallback(void* depth, uint32_t) override;
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<uint16_t>& out);
private:
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<uint16_t> depthBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
};

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
    freenect_context*         freenectContext;
    MyFreenectDevice*         device;

    std::atomic<bool>         firstRGBReady{false};
    std::atomic<bool>         firstDepthReady{false};

    std::vector<uint8_t>      lastRGB;
    std::vector<uint16_t>     lastDepth;

    // background event thread control
    std::atomic<bool>         runEvents{false};
    std::thread               eventThread;

    // Device type: 0 = Kinect v1, 1 = Kinect v2
    int deviceType = 0;
    std::string lastDeviceTypeStr;

    // v2 (libfreenect2) members
    libfreenect2::Freenect2*           fn2_ctx = nullptr;
    libfreenect2::Freenect2Device*     fn2_dev = nullptr;
    libfreenect2::SyncMultiFrameListener* fn2_listener = nullptr;
    libfreenect2::PacketPipeline*      fn2_pipeline = nullptr;
    std::string                        fn2_serial;
    bool                               fn2_started = false;
    std::vector<uint8_t>               fn2_lastRGB;
    std::vector<float>                 fn2_lastDepth;

    // Add flag for deferred Kinect v2 initialization
    bool needInitV2;

    // Kinect v2 polling thread and control
    std::thread fn2_eventThread;
    std::atomic<bool> fn2_runEvents{false};

    bool initDevice();
    void cleanupDevice();
    bool initDeviceV2();
    void cleanupDeviceV2();
    
    std::mutex freenectMutex;
    
};
