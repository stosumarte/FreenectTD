//
//  FreenectV2.h
//  FreenectTD
//

#pragma once

#include "logger.h"
#include "KinectMetal.h"

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>

#include <thread>
#include <mutex>
#include <atomic>

// Forward declaration - depthFormatEnum is defined in FreenectTOP.h
enum class depthFormatEnum;

class MyFreenect2Device {
public:
    static constexpr int RGB_WIDTH    = 1920;
    static constexpr int RGB_HEIGHT   = 1080;
    static constexpr int SCALED_WIDTH = 1280;
    static constexpr int SCALED_HEIGHT = 720;
    static constexpr int DEPTH_WIDTH  = 512;
    static constexpr int DEPTH_HEIGHT = 424;
    static constexpr int BIGDEPTH_WIDTH  = 1920;
    static constexpr int BIGDEPTH_HEIGHT = 1082;
    static constexpr int IR_WIDTH  = 512;
    static constexpr int IR_HEIGHT = 424;

    MyFreenect2Device(libfreenect2::Freenect2Device* device,
                     std::atomic<bool>& rgbFlag,
                     std::atomic<bool>& depthFlag,
                     std::atomic<bool>& irFlag);
    ~MyFreenect2Device();

    bool start();
    void stop();

    void setResolutions(int rgbWidth,   int rgbHeight,
                        int depthWidth, int depthHeight,
                        int pcWidth,    int pcHeight,
                        int irWidth,    int irHeight);

    void setParams(int depthType, float minD, float maxD,
                   bool enableDepth, bool enableIR, bool enablePC);

    // Cook-thread getters — non-blocking
    bool getColorFrame     (std::vector<uint8_t>&  out, int& w, int& h);
    bool getDepthFrame     (std::vector<uint16_t>& out, int& w, int& h);
    bool getIRFrame        (std::vector<uint16_t>& out, int& w, int& h);
    bool getPointCloudFrame(std::vector<float>&    out, int& w, int& h);

    void setRGBBuffer  (const std::vector<uint8_t>& buf, bool hasNew = true);
    void setDepthBuffer(const std::vector<float>&   buf, bool hasNew = true);

    libfreenect2::Freenect2Device* getDevice() { return device; }

private:
    // ── device handles ──────────────────────────────────────────────────────
    libfreenect2::Freenect2Device*            device;
    libfreenect2::SyncMultiFrameListener*     listener;
    std::unique_ptr<libfreenect2::Registration> reg;

    libfreenect2::Frame depthFrame;
    libfreenect2::Frame rgbFrame;
    libfreenect2::Frame undistortedFrame;
    libfreenect2::Frame registeredFrame;
    libfreenect2::Frame bigdepthFrame;

    std::atomic<bool>& rgbReady;
    std::atomic<bool>& depthReady;
    std::atomic<bool>& irReady;

    // ── raw input buffers (protected by rawMutex_) ───────────────────────────
    std::mutex           rawMutex_;
    std::vector<uint8_t> rgbBuffer;
    std::vector<float>   depthBuffer;
    std::vector<float>   irBuffer;
    bool                 hasNewRGB{false};
    bool                 hasNewDepth{false};
    bool                 hasNewIR{false};

    // ── ready-to-upload buffers (protected by readyMutex_) ───────────────────
    struct ReadyBuffers {
        std::vector<uint8_t>  color;
        std::vector<uint16_t> depth;
        std::vector<uint16_t> ir;
        std::vector<float>    pc;
        bool hasColor{false}, hasDepth{false}, hasIR{false}, hasPC{false};
        int  colorW{0}, colorH{0};
        int  depthW{0}, depthH{0};
        int  irW{0},    irH{0};
        int  pcW{0},    pcH{0};
    };
    ReadyBuffers ready_;
    std::mutex   readyMutex_;

    // ── processing parameters (protected by paramMutex_) ─────────────────────
    struct ProcessingParams {
        int   rgbW{RGB_WIDTH},     rgbH{RGB_HEIGHT};
        int   depthW{DEPTH_WIDTH}, depthH{DEPTH_HEIGHT};
        int   pcW{DEPTH_WIDTH},    pcH{DEPTH_HEIGHT};
        int   irW{IR_WIDTH},       irH{IR_HEIGHT};
        int   depthType{0};
        float depthMin{100.f},     depthMax{4500.f};
        bool  enableDepth{true},   enableIR{false}, enablePC{false};
    };
    ProcessingParams params_;
    std::mutex       paramMutex_;

    // ── worker-only persistent scratch (no locking — single writer) ────────────
    // Raw snapshots — swapped O(1) with rgbBuffer/depthBuffer/irBuffer under rawMutex_
    std::vector<uint8_t>  sc_rawRGB_;
    std::vector<float>    sc_rawDepth_;
    std::vector<float>    sc_rawIR_;
    // Processed outputs — swapped into ready_ each frame; resize = no-op in steady state
    std::vector<uint8_t>  sc_color_;
    std::vector<uint8_t>  sc_colorTmp_;     // vImage flip intermediate (full sensor res)
    std::vector<uint16_t> sc_depth_;
    std::vector<float>    sc_depthFlipped_;
    std::vector<float>    sc_depthScaled_;
    std::vector<uint16_t> sc_ir_;
    std::vector<float>    sc_irRefl_;
    std::vector<float>    sc_irScaled_;
    std::vector<float>    sc_pc_;
    std::vector<float>    sc_pcRaw_;
    std::vector<float>    sc_pcFlipped_;
    // Registration helpers
    std::vector<float>    registeredCroppedBuffer;
    bool                  lastRegisteredDepthValid{false};

    KinectMetal* metal_{nullptr};

    // ── worker thread ────────────────────────────────────────────────────────
    std::thread       workerThread;
    std::atomic<bool> stopWorker{true};

    void runWorker();
    bool processFramesRaw();

    // Async Metal pipeline: submit GPU work for current frame (non-blocking),
    // collect result and publish at the start of the *next* frame iteration.
    // Returns true if Metal work was submitted and collectAndPublish() must be called.
    // Returns false if data was processed synchronously (Metal unavailable) or no data.
    bool processAndSubmitAsync();
    void collectAndPublish();

    // Pending frame state — set by processAndSubmitAsync, consumed by collectAndPublish
    bool             hasPendingFrame_{false};
    bool             pendingMetalSubmitted_{false};  // true = collectAsync needed
    bool             pendingColorOK_{false};
    bool             pendingDepthOK_{false};
    bool             pendingIROK_{false};
    bool             pendingPCOK_{false};
    int              pendingDepthOutW_{0}, pendingDepthOutH_{0};
    ProcessingParams pendingParams_{};
};
