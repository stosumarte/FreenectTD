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

// V1 device class
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

// V2 device class - follows the same pattern as MyFreenectDevice
class MyFreenect2Device {
public:
    MyFreenect2Device(libfreenect2::Freenect2Device* device,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag);
    ~MyFreenect2Device();
    
    bool start();
    void stop();
    
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<float>& out);
    
    // Process any pending frames - similar to freenect_process_events
    void processFrames();
    
private:
    libfreenect2::Freenect2Device* device;
    libfreenect2::SyncMultiFrameListener* listener;
    
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<float>    depthBuffer;
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
    
    // V1 device members
    freenect_context*         freenectContext;
    MyFreenectDevice*         device;
    std::atomic<bool>         firstRGBReady{false};
    std::atomic<bool>         firstDepthReady{false};
    std::vector<uint8_t>      lastRGB;
    std::vector<uint16_t>     lastDepth;
    std::atomic<bool>         runEvents{false};
    std::thread               eventThread;

    // Device type: 0 = Kinect v1, 1 = Kinect v2
    int deviceType = 0;
    std::string lastDeviceTypeStr;

    // V2 device members - follow similar pattern to V1
    libfreenect2::Freenect2*       fn2_ctx = nullptr;
    MyFreenect2Device*             fn2_device = nullptr;
    libfreenect2::PacketPipeline*  fn2_pipeline = nullptr;
    std::string                    fn2_serial;
    std::atomic<bool>              fn2_rgbReady{false};
    std::atomic<bool>              fn2_depthReady{false};
    // Single buffer for Kinect v2
    std::vector<uint8_t>           fn2_rgbBuffer;
    std::vector<float>             fn2_depthBuffer;
    std::atomic<bool>              fn2_runEvents{false};
    std::thread                    fn2_eventThread;

    // --- Kinect v2 (libfreenect2) runtime members ---
    libfreenect2::Freenect2Device*      fn2_dev = nullptr;
    libfreenect2::SyncMultiFrameListener* fn2_listener = nullptr;
    bool                               fn2_started = false;
    std::thread                        fn2_thread;
    std::atomic<bool>                  fn2_runThread{false};
    std::mutex                         fn2_bufferMutex;

    // Device init/cleanup methods
    bool initDevice();
    void cleanupDevice();
    bool initDeviceV2();
    void cleanupDeviceV2();
    
    std::mutex freenectMutex;
};
