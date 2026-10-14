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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include <touchdesigner_2025/TOP_CPlusPlusBase.h>
#pragma clang diagnostic pop

#include "FreenectCommon.h"

#include <array>
#include <thread>
#include <mutex>
#include <atomic>

// What the worker thread makes from each frame set; the cook passes it in with setSettings
struct V2Settings {
    int colorW = 1920, colorH = 1080; // RGB output, and Registered depth / color-space point cloud
    depthFormatEnum depthFormat = depthFormatEnum::Raw;
    depthOutputEnum depthOutput = depthOutputEnum::Normalized;
    float threshMin = 100.0f, threshMax = 4500.0f;
    float unknownDepth = 0.0f;
    float unknownPoint[3] = {};
    bool depth = true, pointCloud = false, ir = false, regColor = false, uv = false;
    bool flipX = false, flipY = false, flipZ = false;
};

// A finished output, ready for TOP_Output::uploadBuffer; an empty buf means no new image
struct V2Image {
    TD::OP_SmartRef<TD::TOP_Buffer> buf;
    int width = 0, height = 0;
    TD::OP_PixelFormat format = TD::OP_PixelFormat::RGBA8Fixed;
};

class MyFreenect2Device : public libfreenect2::FrameListener {
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
    
    static constexpr int NUM_OUTPUTS = 6; // 0 RGB, 1 depth, 2 point cloud, 3 IR, 4 registered color, 5 depth->color UV
    using Images = std::array<V2Image, NUM_OUTPUTS>;

    MyFreenect2Device(libfreenect2::Freenect2Device* device, TD::TOP_Context* context);
    ~MyFreenect2Device();
    bool start();
    void stop();
    // Outputs made from frames arriving from now on
    void setSettings(const V2Settings& settings);
    // Moves the images finished since the last call into `out`, by output index
    void takeImages(Images& out);
    uint64_t getDepthSeq();
    // Color frames libfreenect2 delivered (for rgb_fps): below 30 fps the color camera holds depth back too,
    // since each depth frame is paired with a color frame. It drops to 15 fps in low light.
    uint64_t getColorFrames() const { return colorFrames.load(); }
    // Worker time per frame set in ms, smoothed (for process_ms)
    float getProcessMs() const { return processMs.load(); }
    bool onNewFrame(libfreenect2::Frame::Type type, libfreenect2::Frame* frame) override;
    
    libfreenect2::Freenect2Device* getDevice() { return device; }
    
private:
    libfreenect2::Freenect2Device* device;
    TD::TOP_Context* context; // makes the output buffers, from the worker thread
    libfreenect2::SyncMultiFrameListener* listener;
    std::atomic<uint64_t> colorFrames{0};
    std::atomic<float> processMs{0.0f};

    // Guarded by mutex: shared with the cook thread
    std::mutex mutex;
    V2Settings settings;
    Images     images;
    uint64_t   depthSeq = 0; // incremented for every new depth frame

    // Worker thread only
    libfreenect2::Frame undistortedFrame;
    libfreenect2::Frame registeredFrame;
    libfreenect2::Frame bigdepthFrame;
    std::unique_ptr<libfreenect2::Registration> reg;
    libfreenect2::Freenect2Device::ColorCameraParams colorCameraParams{}; // captured with reg
    std::vector<int>     colorDepthMap;
    std::vector<uint8_t> rgbScratch;
    std::vector<float>   depthScratch;
    std::vector<float>   pcScratch;
    std::thread          workerThread;
    std::atomic<bool>    stopWorker{true};
    void runWorker();
    void processFrames();
    V2Image newImage(int width, int height, TD::OP_PixelFormat format, size_t bytesPerPixel);
    V2Image makeColor(const libfreenect2::Frame& rgb, const V2Settings& s);
    V2Image makeDepth(const libfreenect2::Frame& depth, const V2Settings& s);
    V2Image makePointCloud(const V2Settings& s);
    void makeRegisteredColor(const V2Settings& s, V2Image& color, V2Image& uv);
    V2Image makeIR(const libfreenect2::Frame& ir);
};
