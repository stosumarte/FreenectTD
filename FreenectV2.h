//
//  FreenectV2.h
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#pragma once

#include "logger.h"

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>

enum class fn2_depthType {
    Raw,
    Undistorted,
    Registered
};

class MyFreenect2Device {
public:
    static constexpr int RGB_WIDTH = 1920;
    static constexpr int RGB_HEIGHT = 1080;
    
    static constexpr int SCALED_WIDTH = 1280;
    static constexpr int SCALED_HEIGHT = 720;
    
    static constexpr int DEPTH_WIDTH = 512;
    static constexpr int DEPTH_HEIGHT = 424;
    
    static constexpr int BIGDEPTH_WIDTH = 1920;
    static constexpr int BIGDEPTH_HEIGHT = 1082; // Note: 1082, not 1080
    
    static constexpr int IR_WIDTH = 512;
    static constexpr int IR_HEIGHT = 424;
    
    MyFreenect2Device(libfreenect2::Freenect2Device* device,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag, std::atomic<bool>& irFlag);
    ~MyFreenect2Device();
    bool start();
    void stop();
    void close();
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<float>& out);
    bool getIR(std::vector<float>& out);
    void processFrames();
    // Unified processed frame methods for v2
    bool getColorFrame(std::vector<uint8_t>& out, bool downscale);
    bool getDepthFrame(std::vector<uint16_t>& out, fn2_depthType type, bool downscale, int& width, int& height);
    //bool getUndistortedDepthFrame(std::vector<uint16_t>& out);
    //bool getRegisteredDepthFrame(std::vector<uint16_t>& out, bool downscale);
    bool getIRFrame(std::vector<uint16_t>& out, int& width, int& height);
    // Setters for buffer injection
    void setRGBBuffer(const std::vector<uint8_t>& buf, bool hasNew = true);
    void setDepthBuffer(const std::vector<float>& buf, bool hasNew = true);
    
    libfreenect2::Freenect2Device* getDevice() { return device; }
    
private:
    libfreenect2::Freenect2Device* device;
    libfreenect2::SyncMultiFrameListener* listener;
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::atomic<bool>&    irReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<float>    depthBuffer;
    std::vector<float>    irBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
    bool                  hasNewIR;
};
