#include "FreenectV1.h"
#include <algorithm>
#include <cstring>

MyFreenectDevice::MyFreenectDevice(freenect_context* ctx, int index,
                                   std::atomic<bool>& rgbFlag,
                                   std::atomic<bool>& depthFlag)
    : FreenectDevice(ctx, index),
      rgbReady(rgbFlag),
      depthReady(depthFlag),
      rgbBuffer(640 * 480 * 3),
      depthBuffer(640 * 480),
      hasNewRGB(false),
      hasNewDepth(false) {
    setVideoFormat(FREENECT_VIDEO_RGB);
    setDepthFormat(FREENECT_DEPTH_11BIT);
}

MyFreenectDevice::~MyFreenectDevice() {
    stop();
}

void MyFreenectDevice::VideoCallback(void* rgb, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!rgb) return;
    auto ptr = static_cast<uint8_t*>(rgb);
    std::copy(ptr, ptr + rgbBuffer.size(), rgbBuffer.begin());
    hasNewRGB = true;
    rgbReady = true;
}

void MyFreenectDevice::DepthCallback(void* depth, uint32_t) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!depth) return;
    auto ptr = static_cast<uint16_t*>(depth);
    std::copy(ptr, ptr + depthBuffer.size(), depthBuffer.begin());
    hasNewDepth = true;
    depthReady = true;
}

bool MyFreenectDevice::start() {
    startVideo();
    startDepth();
    return true;
}

void MyFreenectDevice::stop() {
    stopVideo();
    stopDepth();
}

bool MyFreenectDevice::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    out = rgbBuffer;
    hasNewRGB = false;
    return true;
}

bool MyFreenectDevice::getDepth(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    out = depthBuffer;
    hasNewDepth = false;
    return true;
}

bool MyFreenectDevice::getColorFrame(std::vector<uint8_t>& out, bool flip) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    int width = WIDTH, height = HEIGHT;
    out.resize(width * height * 4);
    for (int y = 0; y < height; ++y) {
        int srcY = flip ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            int srcIdx = (srcY * width + x) * 3;
            int dstIdx = (y * width + x) * 4;
            out[dstIdx + 0] = rgbBuffer[srcIdx + 0];
            out[dstIdx + 1] = rgbBuffer[srcIdx + 1];
            out[dstIdx + 2] = rgbBuffer[srcIdx + 2];
            out[dstIdx + 3] = 255;
        }
    }
    hasNewRGB = false;
    return true;
}

bool MyFreenectDevice::getDepthFrame(std::vector<uint16_t>& out, bool invert, bool flip) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    int width = WIDTH, height = HEIGHT;
    out.resize(width * height);
    for (int y = 0; y < height; ++y) {
        int srcY = flip ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            int idx = srcY * width + x;
            uint16_t raw = depthBuffer[idx];
            if (raw > 0 && raw < 2047) {
                uint16_t scaled = raw << 5;
                out[y * width + x] = invert ? 65535 - scaled : scaled;
            } else {
                out[y * width + x] = 0;
            }
        }
    }
    hasNewDepth = false;
    return true;
}
