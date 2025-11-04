//
//  FreenectV1.h
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#pragma once

#include "logger.h"

#include <libfreenect/libfreenect.hpp>

// Forward declaration - depthFormatEnum is defined in FreenectTOP.h
enum class depthFormatEnum;

enum class fn1_colorType {
    RGB,
    IR
};

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    static constexpr int WIDTH = 640;
    static constexpr int HEIGHT = 480;
    
    MyFreenectDevice(freenect_context* ctx, int index,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag);
    ~MyFreenectDevice();
    void VideoCallback(void* rgb, uint32_t) override;
    void DepthCallback(void* depth, uint32_t) override;
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<uint16_t>& out);
    bool getColorFrame(std::vector<uint8_t>& out, fn1_colorType type);
    bool getDepthFrame(std::vector<uint16_t>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax);
    bool start();
    void stop();
    void setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight);
private:
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<uint16_t> depthBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
    int rgbWidth_ = WIDTH;
    int rgbHeight_ = HEIGHT;
    int depthWidth_ = WIDTH;
    int depthHeight_ = HEIGHT;
    int irWidth_ = WIDTH;
    int irHeight_ = HEIGHT;
};
