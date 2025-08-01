//
//  FreenectV2.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV2.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <thread>
#include <opencv.hpp>
//#import <Accelerate/Accelerate.h>

// MyFreenect2Device class constructor
MyFreenect2Device::MyFreenect2Device(libfreenect2::Freenect2Device* dev,
                                     std::atomic<bool>& rgbFlag, std::atomic<bool>& depthFlag)
    : device(dev), listener(nullptr), rgbReady(rgbFlag), depthReady(depthFlag),
      rgbBuffer(WIDTH * HEIGHT * 4, 0), depthBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0),
      hasNewRGB(false), hasNewDepth(false) {
    listener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
    device->setColorFrameListener(listener);
    device->setIrAndDepthFrameListener(listener);
}

// MyFreenect2Device class destructor
MyFreenect2Device::~MyFreenect2Device() {
    stop();
    if (listener) {
        delete listener;
        listener = nullptr;
    }
}

// Start the device streams using libfreenect2 API
bool MyFreenect2Device::start() {
    if (!device) return false;
    return device->start();
}

// Stop the device streams using libfreenect2 API
void MyFreenect2Device::stop() {
    if (device) device->stop();
}

// Set RGB buffer and mark as ready
void MyFreenect2Device::processFrames() {
    static int frameCount = 0;
    static auto lastTime = std::chrono::steady_clock::now();
    if (!listener) return;
    if (!listener->hasNewFrame()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Prevent busy-waiting
        return;
    }
    libfreenect2::FrameMap frames;
    listener->waitForNewFrame(frames, 0); // Immediately get the frame if available
    libfreenect2::Frame* rgb = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (rgb && rgb->data && rgb->width == WIDTH && rgb->height == HEIGHT) {
            memcpy(rgbBuffer.data(), rgb->data, WIDTH * HEIGHT * 4);
            hasNewRGB = true;
            rgbReady = true;
        }
        if (depth && depth->data && depth->width == DEPTH_WIDTH && depth->height == DEPTH_HEIGHT) {
            const float* src = reinterpret_cast<const float*>(depth->data);
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, depthBuffer.begin());
            hasNewDepth = true;
            depthReady = true;
        }
    }
    listener->release(frames);
    // Logging FPS
    frameCount++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
    if (elapsed >= 1) {
        std::cout << "[FreenectV2] processFrames FPS: " << frameCount << std::endl;
        frameCount = 0;
        lastTime = now;
    }
}

// Get RGB data
bool MyFreenect2Device::getRGB(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;
    out = rgbBuffer;
    hasNewRGB = false;
    return true;
}

// Get depth data
bool MyFreenect2Device::getDepth(std::vector<float>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    out = depthBuffer;
    hasNewDepth = false;
    return true;
}

// Metal shader - Get color frame with optional flipping and downscaling
//#include <Metal/Metal.h>


// CPU ONLY - Get color frame with optional flipping and downscaling
/* bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool flip, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    if (downscale) {
        out.resize(SCALED_WIDTH * SCALED_HEIGHT * 4);
        for (int y = 0; y < SCALED_HEIGHT; ++y) {
            int srcY = flip ? (HEIGHT - 1 - (y * HEIGHT / SCALED_HEIGHT)) : (y * HEIGHT / SCALED_HEIGHT);
            for (int x = 0; x < SCALED_WIDTH; ++x) {
                int srcX = x * WIDTH / SCALED_WIDTH;
                int srcIdx = (srcY * WIDTH + srcX) * 4;
                int dstIdx = (y * SCALED_WIDTH + x) * 4;
                out[dstIdx + 0] = rgbBuffer[srcIdx + 2]; // R
                out[dstIdx + 1] = rgbBuffer[srcIdx + 1]; // G
                out[dstIdx + 2] = rgbBuffer[srcIdx + 0]; // B
                out[dstIdx + 3] = 255;
            }
        }
    } else {
        out.resize(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            int srcY = flip ? (HEIGHT - 1 - y) : y;
            for (int x = 0; x < WIDTH; ++x) {
                int srcIdx = (srcY * WIDTH + x) * 4;
                int dstIdx = (y * WIDTH + x) * 4;
                out[dstIdx + 0] = rgbBuffer[srcIdx + 2]; // R
                out[dstIdx + 1] = rgbBuffer[srcIdx + 1]; // G
                out[dstIdx + 2] = rgbBuffer[srcIdx + 0]; // B
                out[dstIdx + 3] = 255;
            }
        }
    }

    hasNewRGB = false;
    return true;
} */

// Accelerate Framework - Get color frame with optional flipping and downscaling
/* bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool flip, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    int srcW = WIDTH, srcH = HEIGHT;
    int dstW = downscale ? SCALED_WIDTH : WIDTH;
    int dstH = downscale ? SCALED_HEIGHT : HEIGHT;
    size_t rowBytesSrc = srcW * 4;
    size_t rowBytesDst = dstW * 4;

    // Allocate output buffer
    out.assign(dstW * dstH * 4, 0);

    // Input buffer descriptor
    vImage_Buffer srcBuf;
    srcBuf.data = rgbBuffer.data();
    srcBuf.height = static_cast<vImagePixelCount>(srcH);
    srcBuf.width  = static_cast<vImagePixelCount>(srcW);
    srcBuf.rowBytes = rowBytesSrc;

    // Intermediate buffer for scaling
    std::vector<uint8_t> tempBuf(downscale ? dstW * dstH * 4 : 0);
    vImage_Buffer dstBuf;
    dstBuf.data = downscale ? tempBuf.data() : srcBuf.data;
    dstBuf.height = static_cast<vImagePixelCount>(dstH);
    dstBuf.width  = static_cast<vImagePixelCount>(dstW);
    dstBuf.rowBytes = rowBytesDst;

    vImage_Error err = vImageScale_ARGB8888(&srcBuf, &dstBuf, nullptr, kvImageHighQualityResampling);
    if (err != kvImageNoError) return false;

    // Optionally flip vertically
    if (flip) {
        err = vImageVerticalReflect_ARGB8888(&dstBuf, &dstBuf, kvImageNoFlags);
        if (err != kvImageNoError) return false;
    }

    // Permute channels: BGRA -> RGBA
    const uint8_t permuteMap[4] = {2, 1, 0, 3};
    err = vImagePermuteChannels_ARGB8888(&dstBuf, &dstBuf, permuteMap, kvImageNoFlags);
    if (err != kvImageNoError) return false;

    // Copy to output
    memcpy(out.data(), dstBuf.data, dstH * rowBytesDst);

    hasNewRGB = false;
    return true;
} */


// OPENCV - Get color frame with optional flipping and downscaling
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool flip, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewRGB) return false;

    // Wrap rgbBuffer as BGRA image
    cv::Mat src(HEIGHT, WIDTH, CV_8UC4, rgbBuffer.data());
    cv::Mat dst;
    // Resize if needed
    if (downscale) {
        cv::resize(src, dst, cv::Size(SCALED_WIDTH, SCALED_HEIGHT), 0, 0, cv::INTER_LINEAR);
    } else {
        dst = src;
    }
    // Flip image
    cv::flip(dst, dst, 0);
    // Convert BGRA to RGBA
    cv::cvtColor(dst, dst, cv::COLOR_BGRA2RGBA);
    // Copy to output vector
    out.assign(dst.data, dst.data + dst.total() * dst.elemSize());
    hasNewRGB = false;
    return true;
}

// Get depth frame with optional inversion and undistortion
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out, bool invert, bool undistort) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasNewDepth) return false;
    int width = 512, height = 424;
    out.resize(width * height);
    if (undistort && device) {
        static libfreenect2::Registration registration(device->getIrCameraParams(), device->getColorCameraParams());
        static libfreenect2::Frame undistorted(width, height, 4);
        static libfreenect2::Frame registered(width, height, 4);
        // Simulate a color frame for registration (not used for depth only)
        libfreenect2::Frame fakeColor(1920, 1080, 4);
        libfreenect2::Frame depthFrame(width, height, 4, reinterpret_cast<unsigned char*>(depthBuffer.data()));
        registration.apply(&fakeColor, &depthFrame, &undistorted, &registered);
        float* undistData = reinterpret_cast<float*>(undistorted.data);
        for (int i = 0; i < width * height; ++i) {
            float d = undistData[i];
            uint16_t val = 0;
            if (d > 0.1f && d < 4.5f) {
                val = static_cast<uint16_t>(d / 4.5f * 65535.0f);
                if (invert) val = 65535 - val;
            }
            out[i] = val;
        }
    } else {
        for (int i = 0; i < width * height; ++i) {
            float d = depthBuffer[i];
            uint16_t val = 0;
            if (d > 0.1f && d < 4.5f) {
                val = static_cast<uint16_t>(d / 4.5f * 65535.0f);
                if (invert) val = 65535 - val;
            }
            out[i] = val;
        }
    }
    hasNewDepth = false;
    return true;
}

// Set RGB buffer and mark as ready
void MyFreenect2Device::setRGBBuffer(const std::vector<uint8_t>& buffer, bool markReady) {
    std::lock_guard<std::mutex> lock(mutex);
    rgbBuffer = buffer;
    hasNewRGB = markReady;
    if (markReady) rgbReady = true;
}

// Set depth buffer and mark as ready
void MyFreenect2Device::setDepthBuffer(const std::vector<float>& buffer, bool markReady) {
    std::lock_guard<std::mutex> lock(mutex);
    depthBuffer = buffer;
    hasNewDepth = markReady;
    if (markReady) depthReady = true;
}
