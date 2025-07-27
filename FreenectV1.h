//
//  FreenectV1.h
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#pragma once
#include "libfreenect.hpp"
#include <atomic>
#include <vector>
#include <mutex>

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    MyFreenectDevice(freenect_context* ctx, int index,
                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag);
    void VideoCallback(void* rgb, uint32_t) override;
    void DepthCallback(void* depth, uint32_t) override;
    bool getRGB(std::vector<uint8_t>& out);
    bool getDepth(std::vector<uint16_t>& out);
    // New: unified processed frame methods
    bool getColorFrame(std::vector<uint8_t>& out, bool flip);
    bool getDepthFrame(std::vector<uint16_t>& out, bool invert, bool flip);
private:
    std::atomic<bool>&    rgbReady;
    std::atomic<bool>&    depthReady;
    std::vector<uint8_t>  rgbBuffer;
    std::vector<uint16_t> depthBuffer;
    std::mutex            mutex;
    bool                  hasNewRGB;
    bool                  hasNewDepth;
};
