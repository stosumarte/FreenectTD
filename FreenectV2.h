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
    bool getColorFrame(std::vector<uint8_t>& out);
    bool getDepthFrame(std::vector<uint16_t>& out, fn2_depthType type);
    bool getIRFrame(std::vector<uint16_t>& out);
    // Setters for buffer injection
    void setRGBBuffer(const std::vector<uint8_t>& buf, bool hasNew = true);
    void setDepthBuffer(const std::vector<float>& buf, bool hasNew = true);
    // Set resolutions
    void setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight, int bigdepthWidth, int bigdepthHeight);
    
    libfreenect2::Freenect2Device* getDevice() { return device; }
    
private:
    libfreenect2::Freenect2Device* device;
    libfreenect2::SyncMultiFrameListener* listener;
    libfreenect2::Frame depthFrame;
    libfreenect2::Frame rgbFrame;
    libfreenect2::Frame undistortedFrame;
    libfreenect2::Frame registeredFrame;
    libfreenect2::Frame bigdepthFrame;
    std::unique_ptr<libfreenect2::Registration> reg;
    std::atomic<bool>&      rgbReady;
    std::atomic<bool>&      depthReady;
    std::atomic<bool>&      irReady;
    std::vector<uint8_t>    rgbBuffer;
    std::vector<float>      depthBuffer;
    std::vector<float>      irBuffer;
    std::vector<float>      downscaledDepthBuffer;
    std::vector<float>      bigdepthBufferCropped;
    std::vector<float>      flipDstBuffer;
    std::vector<float>      registeredCroppedBuffer;
    bool                    lastRegisteredDepthValid = false;
    std::mutex              mutex;
    bool                    hasNewRGB;
    bool                    hasNewDepth;
    bool                    hasNewIR;
    int rgbWidth_ = RGB_WIDTH,
        rgbHeight_ = RGB_HEIGHT,
        depthWidth_ = DEPTH_WIDTH,
        depthHeight_ = DEPTH_HEIGHT,
        irWidth_ = IR_WIDTH,
        irHeight_ = IR_HEIGHT,
        bigdepthWidth_ = BIGDEPTH_WIDTH,
        bigdepthHeight_ = BIGDEPTH_HEIGHT - 2; // Crop to 1080 from 1082
};
