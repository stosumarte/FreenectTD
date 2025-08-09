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
    listener = new libfreenect2::SyncMultiFrameListener
          (libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
    device->setColorFrameListener(listener);
    device->setIrAndDepthFrameListener(listener);
}

// MyFreenect2Device class destructor
MyFreenect2Device::~MyFreenect2Device() {
    std::cout << "[FreenectV2.cpp] MyFreenect2Device destructor called \n";
    stop();
    std::cout << "[FreenectV2.cpp] MyFreenect2Device stop called \n";
    //close();
    //std::cout << "[FreenectV2.cpp] MyFreenect2Device close called \n";
    if (listener) {
        std::cout << "[FreenectV2.cpp] Deleting listener \n";
        delete listener;
        std::cout << "[FreenectV2.cpp] listener deleted \n";
        listener = nullptr;
        std::cout << "[FreenectV2.cpp] listener set to nullptr \n";
    }
}

// Start the device streams using libfreenect2 API
bool MyFreenect2Device::start() {
    if (!device) return false;
    return device->startStreams(true,true);
}

// Stop the device streams using libfreenect2 API
void MyFreenect2Device::stop() {
    if (device) {
        device->stop();
        std::cout << "[FreenectV2.cpp] device->stop \n";
    }
}

// Close the device using libfreenect2 API
void MyFreenect2Device::close() {
    if (device) {
        device->close();
        std::cout << "[FreenectV2.cpp] device->close \n";
    }
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
    listener->waitForNewFrame(frames, 1000); // Immediately get the frame if available
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
            // Debug: print first 100 raw depth values from device
            //std::cout << "[processFrames] First 100 raw device depth values: /n";
            //for (int i = 0; i < 100; ++i) std::cout << src[i] << " ";
            //std::cout << std::endl;
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
    if (!hasNewDepth) {
        return false;
    }
    else {
        out = depthBuffer;
        hasNewDepth = false;
        return true;
    }
}

// OPENCV - Get color frame with flipping and optional downscaling
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!hasNewRGB) return false;

    cv::Mat src(HEIGHT, WIDTH, CV_8UC4, rgbBuffer.data());
    cv::Mat dst;
    
    if (downscale) { // Downscale to 1280x720 if requested
        cv::resize(src, dst, cv::Size(SCALED_WIDTH, SCALED_HEIGHT), 0, 0, cv::INTER_LINEAR);
    } else {
        dst = src;
    }
    
    cv::flip(dst, dst, -1); // Flip both horizontally and vertically
    cv::cvtColor(dst, dst, cv::COLOR_BGRA2RGBA); // Convert from BGRA to RGBA
    out.assign(dst.data, dst.data + dst.total() * dst.elemSize());
    hasNewRGB = false;
    return true;
}

// Get depth frame
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewDepth) return false;

    int width = DEPTH_WIDTH, height = DEPTH_HEIGHT;
    cv::Mat src(height, width, CV_32F, depthBuffer.data());
    cv::Mat flipped;
    cv::flip(src, flipped, -1); // Flip both horizontally and vertically

    out.resize(width * height);
    for (int i = 0; i < width * height; ++i) {
        float d = flipped.at<float>(i);
        uint16_t val = 0;
        if (d > 100.0f && d < 4500.0f) {
            val = static_cast<uint16_t>(d / 4500.0f * 65535.0f);
        }
        out[i] = val;
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
