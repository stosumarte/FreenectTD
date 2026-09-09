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
#include "FreenectTypes.h"
#include <libfreenect2/packet_pipeline.h>

#include <thread>
#include <mutex>
#include <atomic>


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
    // Depth in millimetres (float), 0 = invalid / outside threshold
    bool getDepthFrame(std::vector<float>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax);
    bool getIRFrame(std::vector<uint16_t>& out);
    // XYZ (m) + validity in A; space selects depth-camera (512x424) or color-camera (1920x1080) frame
    // flipX/flipY/flipZ negate the corresponding axis (e.g. flipZ makes +Z point toward the viewer, TouchDesigner style)
    bool getPointCloudFrame(std::vector<float>& out, pcSpaceEnum space, float depthThreshMin, float depthThreshMax, bool flipX = false, bool flipY = false, bool flipZ = false);
    // RGB mapped onto the depth grid (512x424 RGBA8) + depth->color UV map (512x424 RGBA32F)
    bool getRegisteredColorFrame(std::vector<uint8_t>& color, std::vector<float>& uv);
    // Setters for buffer injection
    void setRGBBuffer(const std::vector<uint8_t>& buf, bool hasNew = true);
    void setDepthBuffer(const std::vector<float>& buf, bool hasNew = true);
    // Set resolutions
    void setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int pcWidth, int pcHeight, int irWidth, int irHeight);
    
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
    std::vector<float>      pcScratch;
    std::vector<int>        colorDepthMap;
    uint64_t                depthSeq_ = 0;   // incremented for every new depth frame
    uint64_t                regSeq_ = 0;     // depthSeq_ the cached registration was computed for
    bool                    regHasBigdepth_ = false;
    libfreenect2::Freenect2Device::ColorCameraParams colorParams_{};
    bool ensureRegistration(bool needBigdepth);
    std::mutex              mutex;
    bool                    hasNewRGB;
    bool                    hasNewDepth;
    bool                    hasNewIR;
    int rgbWidth_ = RGB_WIDTH,
        rgbHeight_ = RGB_HEIGHT,
        depthWidth_ = DEPTH_WIDTH,
        depthHeight_ = DEPTH_HEIGHT,
        pcWidth_ = DEPTH_WIDTH,
        pcHeight_ = DEPTH_HEIGHT,
        irWidth_ = IR_WIDTH,
        irHeight_ = IR_HEIGHT,
        bigdepthWidth_ = BIGDEPTH_WIDTH,
        bigdepthHeight_ = BIGDEPTH_HEIGHT - 2; // Crop to 1080 from 1082
    std::thread             workerThread;
    std::atomic<bool>       stopWorker{true};
    void runWorker();
};
