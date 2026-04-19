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

// V1 video source is modal: RGB and IR share the same USB endpoint on Kinect v1,
// so only one can stream at a time. Switching requires stop/setMode/start.
enum class fn1_videoSource {
    RGB = 0,
    IR_10BIT = 1,
    IR_8BIT = 2
};

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    static constexpr int WIDTH = 640;
    static constexpr int HEIGHT = 480;

    MyFreenectDevice(freenect_context* ctx, int index,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag,
                     std::atomic<bool>& irFlag);
    ~MyFreenectDevice();
    void VideoCallback(void* video, uint32_t) override;
    void DepthCallback(void* depth, uint32_t) override;
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<uint16_t>& out);
    bool getColorFrame(std::vector<uint8_t>& out, fn1_colorType type);
    bool getDepthFrame(std::vector<uint16_t>& out, depthFormatEnum type, float depthThreshMin, float depthThreshMax);
    bool getIRFrame(std::vector<uint16_t>& out, float irThreshMin, float irThreshMax);
    bool start();
    void stop();
    void setResolutions(int rgbWidth, int rgbHeight, int depthWidth, int depthHeight, int irWidth, int irHeight);

    // Request a video source change from any thread. The actual stop/start
    // happens inside applyPendingVideoSource(), which must be called from the
    // libfreenect event-processing thread (between freenect_process_events calls).
    void requestVideoSource(fn1_videoSource src);
    fn1_videoSource getCurrentVideoSource() const { return currentVideoSource_.load(); }
    // Returns true if a format change was applied.
    bool applyPendingVideoSource();
private:
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::atomic<bool>&    irReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<uint16_t> depthBuffer;
    std::vector<uint16_t> irBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
    bool                  hasNewIR;
    int rgbWidth_ = WIDTH;
    int rgbHeight_ = HEIGHT;
    int depthWidth_ = WIDTH;
    int depthHeight_ = HEIGHT;
    int irWidth_ = WIDTH;
    int irHeight_ = HEIGHT;

    std::atomic<fn1_videoSource> currentVideoSource_{fn1_videoSource::RGB};
    std::atomic<fn1_videoSource> pendingVideoSource_{fn1_videoSource::RGB};
};
