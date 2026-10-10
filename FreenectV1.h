//
//  FreenectV1.h
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#pragma once

#include "logger.h"

#include <libfreenect/libfreenect.hpp>
#include <chrono>

#include "FreenectCommon.h"

enum class fn1_colorType {
    RGB,
    IR
};

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    static constexpr int WIDTH = 640;
    static constexpr int HEIGHT = 480;
    
    MyFreenectDevice(freenect_context* ctx, int index,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag, bool ir = false);
    ~MyFreenectDevice();
    void VideoCallback(void* rgb, uint32_t) override;
    void DepthCallback(void* depth, uint32_t) override;
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<uint16_t>& out);
    bool getColorFrame(std::vector<uint8_t>& out, fn1_colorType type);
    // Depth in millimetres (float), 0 = invalid / outside threshold
    bool getDepthFrame(std::vector<float>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax);
    // IR at native 640x480, 10-bit values scaled to the full 16-bit range
    bool getIRFrame(std::vector<uint16_t>& out);
    // RGB and IR share the video stream, so only one streams at a time. setIR and the depth format
    // requested by getDepthFrame are applied by applyStreamModes, on the event thread.
    void setIR(bool ir);
    void applyStreamModes();
    bool start();
    void stop();
private:
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<uint16_t> depthBuffer;
    std::vector<uint16_t> irBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
    bool                  hasNewIR = false;
    std::atomic<bool>     wantIR{false};
    std::atomic<freenect_depth_format> wantDepthFormat{FREENECT_DEPTH_MM};
    bool                  streamingIR = false; // event thread only
    // Video stream watchdog, event thread only (callbacks run inside freenect_process_events)
    std::chrono::steady_clock::time_point videoStartedAt;
    bool                  videoFrameSinceStart = false;
};
