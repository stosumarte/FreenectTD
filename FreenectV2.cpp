//
//  FreenectV2.cpp
//  FreenectTD
//
//  Created by marte on 27/07/2025.
//

#include "FreenectV2.h"

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
    LOG("[FreenectV2.cpp] MyFreenect2Device destructor called");
    stop();
    LOG("[FreenectV2.cpp] MyFreenect2Device stop called");
    if (listener) {
        LOG("[FreenectV2.cpp] Deleting listener");
        delete listener;
        LOG("[FreenectV2.cpp] listener deleted");
        listener = nullptr;
        LOG("[FreenectV2.cpp] listener set to nullptr");
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
        LOG("[FreenectV2.cpp] device->stop");
    }
}

// Close the device using libfreenect2 API
void MyFreenect2Device::close() {
    if (device) {
        device->close();
        LOG("[FreenectV2.cpp] device->close");
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
            std::copy(src, src + DEPTH_WIDTH * DEPTH_HEIGHT, depthBuffer.begin());
            hasNewDepth = true;
            depthReady = true;
        }
    }
    listener->release(frames);
    // Logging FPS
    frameCount++;
    auto now = std::chrono::steady_clock::now();
    auto frameCountString = std::to_string(frameCount);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
    if (elapsed >= 1) {
        LOG("[FreenectV2] processFrames FPS: " + frameCountString);
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
/*bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool downscale) {
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
}*/


// CImg - Get color frame with flipping and optional downscaling
bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, bool downscale) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewRGB) return false;

    // Create CImg from BGRA buffer
    cimg_library::CImg<uint8_t> img(rgbBuffer.data(), WIDTH, HEIGHT, 1, 4);

    // Downscale if requested
    if (downscale) {
        img.resize(SCALED_WIDTH, SCALED_HEIGHT, 1, 4, 3);
    }

    // Flip horizontally and vertically
    img.mirror('x').mirror('y');

    // Convert BGRA to RGBA (swap channels 0 and 2)
    cimg_forXY(img, x, y) {
        std::swap(img(x, y, 0, 0), img(x, y, 0, 2));
    }

    // Copy data to out
    out.resize(img.width() * img.height() * 4);
    std::memcpy(out.data(), img.data(), out.size());

    hasNewRGB = false;
    return true;
}

// Get depth frame
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewDepth) return false;

    int width = DEPTH_WIDTH, height = DEPTH_HEIGHT;
    
    /*cv::Mat src(height, width, CV_32F, depthBuffer.data());
    cv::Mat flipped;
    cv::flip(src, flipped, -1); // Flip both horizontally and vertically*/
    
    // Using CImg for flipping
    // Create a CImg image from the depth buffer
    cimg_library::CImg<float> src(depthBuffer.data(), width, height, 1, 1, true);

    // Flip both horizontally and vertically
    cimg_library::CImg<float> flipped = src.get_mirror('x').mirror('y');
    

    out.resize(width * height);
    for (int i = 0; i < width * height; ++i) {
        float d = flipped.data()[i];
        uint16_t val = 0;
        if (d > 100.0f && d < 4500.0f) {
            val = static_cast<uint16_t>(d / 4500.0f * 65535.0f);
        }
        out[i] = val;
    }

    hasNewDepth = false;
    return true;
}

// Get undistorted depth frame using libfreenect2::Registration
bool MyFreenect2Device::getUndistortedDepthFrame(std::vector<uint16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!hasNewDepth || !device) return false;
    
    int width = DEPTH_WIDTH, height = DEPTH_HEIGHT;
    
    // Get IR and color camera params
    const libfreenect2::Freenect2Device::IrCameraParams& irParams = device->getIrCameraParams();
    const libfreenect2::Freenect2Device::ColorCameraParams& colorParams = device->getColorCameraParams();
    libfreenect2::Registration reg(irParams, colorParams);
    
    // Create Frame for depth input and copy buffer
    libfreenect2::Frame depthFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));
    std::memcpy(depthFrame.data, depthBuffer.data(), DEPTH_WIDTH * DEPTH_HEIGHT * sizeof(float));
    libfreenect2::Frame undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, sizeof(float));
    
    // Only undistort, do not register
    reg.undistortDepth(&depthFrame, &undistortedFrame);
    
    // Convert to OpenCV matrix for flipping
    /*cv::Mat undistortedMat(DEPTH_HEIGHT, DEPTH_WIDTH, CV_32F, undistortedFrame.data);
    cv::Mat flippedMat;
    cv::flip(undistortedMat, flippedMat, -1); // Flip both horizontally and vertically*/
    
    // Convert flipped undistorted float depth to uint16_t
    /*out.resize(DEPTH_WIDTH * DEPTH_HEIGHT);
    for (size_t i = 0; i < DEPTH_WIDTH * DEPTH_HEIGHT; ++i) {
        float d = flippedMat.at<float>(i);
        uint16_t val = 0;
        if (d > 100.0f && d < 4500.0f) {
            val = static_cast<uint16_t>(d / 4500.0f * 65535.0f);
        }
        out[i] = val;
    }*/
    
    // Using CImg for flipping
    // Create a CImg image from the depth buffer
    cimg_library::CImg<float> src(depthBuffer.data(), width, height, 1, 1, true);

    // Flip both horizontally and vertically
    cimg_library::CImg<float> flipped = src.get_mirror('x').mirror('y');
    
    out.resize(width * height);
    for (int i = 0; i < width * height; ++i) {
        float d = flipped.data()[i];
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
