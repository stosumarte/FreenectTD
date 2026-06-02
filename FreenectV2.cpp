//
//  FreenectV2.cpp
//  FreenectTD
//

#include "FreenectV2.h"
#include "KinectMetal.h"
#include <cstring>
#include <algorithm>
#include <thread>
#include <Accelerate/Accelerate.h>

enum class depthFormatEnum { Raw, RawUndistorted, Registered };

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

MyFreenect2Device::MyFreenect2Device(
    libfreenect2::Freenect2Device* dev,
    std::atomic<bool>& rgbFlag,
    std::atomic<bool>& depthFlag,
    std::atomic<bool>& irFlag)
: device(dev), listener(nullptr), reg(nullptr),
  rgbReady(rgbFlag), depthReady(depthFlag), irReady(irFlag),
  rgbBuffer(RGB_WIDTH * RGB_HEIGHT * 4, 0),
  depthBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0.f),
  irBuffer(DEPTH_WIDTH * DEPTH_HEIGHT, 0.f),
  hasNewRGB(false), hasNewDepth(false), hasNewIR(false),
  sc_rawRGB_(RGB_WIDTH * RGB_HEIGHT * 4, 0),
  sc_rawDepth_(DEPTH_WIDTH * DEPTH_HEIGHT, 0.f),
  sc_rawIR_(DEPTH_WIDTH * DEPTH_HEIGHT, 0.f),
  metal_(new KinectMetal()),
  depthFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  rgbFrame(RGB_WIDTH, RGB_HEIGHT, 4),
  undistortedFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  registeredFrame(DEPTH_WIDTH, DEPTH_HEIGHT, 4),
  bigdepthFrame(BIGDEPTH_WIDTH, BIGDEPTH_HEIGHT, 4)
{
    LOG("[FreenectV2] constructor");
    listener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
    device->setColorFrameListener(listener);
    device->setIrAndDepthFrameListener(listener);
}

MyFreenect2Device::~MyFreenect2Device() {
    LOG("[FreenectV2] destructor");
    stop();
    delete metal_; metal_ = nullptr;
    if (listener) { delete listener; listener = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Start / Stop
// ─────────────────────────────────────────────────────────────────────────────

bool MyFreenect2Device::start() {
    if (!device) return false;
    if (!device->startStreams(true, true)) return false;
    stopWorker = false;
    if (!workerThread.joinable())
        workerThread = std::thread(&MyFreenect2Device::runWorker, this);
    return true;
}

void MyFreenect2Device::stop() {
    stopWorker = true;
    if (workerThread.joinable()) workerThread.join();
    if (device) { device->stop(); device->close(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter setters
// ─────────────────────────────────────────────────────────────────────────────

void MyFreenect2Device::setResolutions(int rgbW, int rgbH, int depW, int depH,
                                        int pcW, int pcH, int irW, int irH) {
    std::lock_guard<std::mutex> lock(paramMutex_);
    params_.rgbW=rgbW; params_.rgbH=rgbH; params_.depthW=depW; params_.depthH=depH;
    params_.pcW=pcW;   params_.pcH=pcH;   params_.irW=irW;     params_.irH=irH;
}

void MyFreenect2Device::setParams(int depthType, float minD, float maxD,
                                   bool enableDepth, bool enableIR, bool enablePC) {
    std::lock_guard<std::mutex> lock(paramMutex_);
    params_.depthType=depthType; params_.depthMin=minD; params_.depthMax=maxD;
    params_.enableDepth=enableDepth; params_.enableIR=enableIR; params_.enablePC=enablePC;
}

// ─────────────────────────────────────────────────────────────────────────────
// Worker thread
// ─────────────────────────────────────────────────────────────────────────────

void MyFreenect2Device::runWorker() {
    LOG("[FreenectV2] runWorker: started");
    while (!stopWorker.load()) {
        // Phase A: collect previous Metal result + publish
        // GPU had ~33ms (the Kinect wait below) to finish — waitUntilCompleted returns instantly
        if (hasPendingFrame_) {
            collectAndPublish();
            hasPendingFrame_ = false;
        }

        // Phase B: wait for next Kinect frame (~33ms)
        if (!processFramesRaw()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Phase C: CPU pre-processing + submit Metal async (non-blocking)
        hasPendingFrame_ = processAndSubmitAsync();
    }

    // Flush: collect and publish the last in-flight frame
    if (hasPendingFrame_) {
        collectAndPublish();
        hasPendingFrame_ = false;
    }
    LOG("[FreenectV2] runWorker: exiting");
}

bool MyFreenect2Device::processFramesRaw() {
    if (!listener) return false;
    libfreenect2::FrameMap frames;
    if (!listener->waitForNewFrame(frames, 50)) return false;
    libfreenect2::Frame* rgb   = frames[libfreenect2::Frame::Color];
    libfreenect2::Frame* depth = frames[libfreenect2::Frame::Depth];
    libfreenect2::Frame* ir    = frames[libfreenect2::Frame::Ir];
    bool got = false;
    {
        std::lock_guard<std::mutex> lock(rawMutex_);
        if (rgb && rgb->data && rgb->width==RGB_WIDTH && rgb->height==RGB_HEIGHT) {
            std::memcpy(rgbBuffer.data(), rgb->data, RGB_WIDTH*RGB_HEIGHT*4);
            hasNewRGB=true; rgbReady=true; got=true;
        }
        if (depth && depth->data && depth->width==DEPTH_WIDTH && depth->height==DEPTH_HEIGHT) {
            std::copy(reinterpret_cast<const float*>(depth->data),
                      reinterpret_cast<const float*>(depth->data)+DEPTH_WIDTH*DEPTH_HEIGHT,
                      depthBuffer.begin());
            hasNewDepth=true; depthReady=true; got=true;
        }
        if (ir && ir->data && ir->width==DEPTH_WIDTH && ir->height==DEPTH_HEIGHT) {
            std::copy(reinterpret_cast<const float*>(ir->data),
                      reinterpret_cast<const float*>(ir->data)+DEPTH_WIDTH*DEPTH_HEIGHT,
                      irBuffer.begin());
            hasNewIR=true; irReady=true; got=true;
        }
    }
    listener->release(frames);
    return got;
}

// ─────────────────────────────────────────────────────────────────────────────
// processAndCache — worker thread only
// ─────────────────────────────────────────────────────────────────────────────

// processAndSubmitAsync — CPU pre-processing + non-blocking Metal submit.
// Returns true if Metal was submitted (collectAndPublish must be called next iteration).
// Returns false if processed synchronously (Metal unavailable) or no data.
bool MyFreenect2Device::processAndSubmitAsync() {

    // 1. Snapshot raw buffers — O(1) swap under mutex
    bool gotRGB=false, gotDepth=false, gotIR=false;
    {
        std::lock_guard<std::mutex> lock(rawMutex_);
        if (hasNewRGB)   { std::swap(sc_rawRGB_,  rgbBuffer);   hasNewRGB=false;   gotRGB=true;   }
        if (hasNewDepth) { std::swap(sc_rawDepth_, depthBuffer); hasNewDepth=false; gotDepth=true; }
        if (hasNewIR)    { std::swap(sc_rawIR_,    irBuffer);    hasNewIR=false;    gotIR=true;    }
    }
    if (!gotRGB && !gotDepth && !gotIR) return false;

    // 2. Snapshot params
    ProcessingParams p;
    { std::lock_guard<std::mutex> lock(paramMutex_); p = params_; }
    pendingParams_ = p;

    // 3. Init Registration lazily
    if (!reg && device && (gotDepth || p.enablePC)) {
        reg = std::make_unique<libfreenect2::Registration>(
            device->getIrCameraParams(), device->getColorCameraParams());
        LOG("[FreenectV2] Registration created");
    }

    // 4. Depth CPU pre-processing → get srcData for Metal/vImage
    const float* depthSrc = nullptr;
    int depthSrcW=0, depthSrcH=0, depthOutW=0, depthOutH=0;

    if (p.enableDepth && gotDepth &&
        sc_rawDepth_.size()==static_cast<size_t>(DEPTH_WIDTH*DEPTH_HEIGHT)) {
        switch (static_cast<depthFormatEnum>(p.depthType)) {
            case depthFormatEnum::Raw:
                depthSrc=sc_rawDepth_.data();
                depthSrcW=DEPTH_WIDTH; depthSrcH=DEPTH_HEIGHT;
                depthOutW=p.depthW;   depthOutH=p.depthH;
                break;
            case depthFormatEnum::RawUndistorted:
                if (reg) {
                    std::memcpy(depthFrame.data,sc_rawDepth_.data(),DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(float));
                    reg->undistortDepth(&depthFrame,&undistortedFrame);
                    depthSrc=reinterpret_cast<float*>(undistortedFrame.data);
                    depthSrcW=DEPTH_WIDTH; depthSrcH=DEPTH_HEIGHT;
                    depthOutW=p.depthW;   depthOutH=p.depthH;
                }
                break;
            case depthFormatEnum::Registered:
                if (reg && gotRGB &&
                    sc_rawRGB_.size()==static_cast<size_t>(RGB_WIDTH*RGB_HEIGHT*4)) {
                    std::memcpy(rgbFrame.data,  sc_rawRGB_.data(),   RGB_WIDTH*RGB_HEIGHT*4);
                    std::memcpy(depthFrame.data,sc_rawDepth_.data(), DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(float));
                    reg->apply(&rgbFrame,&depthFrame,&undistortedFrame,&registeredFrame,true,&bigdepthFrame);
                    const int croppedH=BIGDEPTH_HEIGHT-2;
                    if (registeredCroppedBuffer.size()!=static_cast<size_t>(BIGDEPTH_WIDTH*croppedH))
                        registeredCroppedBuffer.resize(BIGDEPTH_WIDTH*croppedH);
                    const float* bd=reinterpret_cast<float*>(bigdepthFrame.data);
                    for (int y=0;y<croppedH;++y)
                        std::memcpy(registeredCroppedBuffer.data()+y*BIGDEPTH_WIDTH,
                                    bd+(y+1)*BIGDEPTH_WIDTH,BIGDEPTH_WIDTH*sizeof(float));
                    int vp=0,tot=BIGDEPTH_WIDTH*croppedH;
                    for (float& v:registeredCroppedBuffer){if(!std::isfinite(v))v=0.f;if(v>100.f&&v<4500.f)++vp;}
                    lastRegisteredDepthValid=(vp>=tot/10);
                    if (lastRegisteredDepthValid) {
                        depthSrc=registeredCroppedBuffer.data();
                        depthSrcW=BIGDEPTH_WIDTH; depthSrcH=croppedH;
                        depthOutW=p.rgbW;         depthOutH=p.rgbH;
                    }
                }
                break;
        }
    }

    pendingDepthOutW_ = depthOutW;
    pendingDepthOutH_ = depthOutH;

    // 5. IR — synchronous CPU/vImage (no Metal)
    pendingIROK_ = false;
    if (p.enableIR && gotIR && sc_rawIR_.size()==static_cast<size_t>(IR_WIDTH*IR_HEIGHT)) {
        const size_t iSz=static_cast<size_t>(p.irW)*p.irH;
        sc_ir_.resize(iSz); sc_irRefl_.resize(IR_WIDTH*IR_HEIGHT); sc_irScaled_.resize(iSz);
        vImage_Buffer src={sc_rawIR_.data(),(vImagePixelCount)IR_HEIGHT,(vImagePixelCount)IR_WIDTH,IR_WIDTH*sizeof(float)};
        vImage_Buffer tmp={sc_irRefl_.data(),(vImagePixelCount)IR_HEIGHT,(vImagePixelCount)IR_WIDTH,IR_WIDTH*sizeof(float)};
        vImageHorizontalReflect_PlanarF(&src,&tmp,kvImageNoFlags);
        vImage_Buffer dst={sc_irScaled_.data(),(vImagePixelCount)p.irH,(vImagePixelCount)p.irW,p.irW*sizeof(float)};
        if (p.irW!=IR_WIDTH||p.irH!=IR_HEIGHT)
            vImageScale_PlanarF(&tmp,&dst,nullptr,kvImageHighQualityResampling);
        else std::memcpy(sc_irScaled_.data(),sc_irRefl_.data(),IR_WIDTH*IR_HEIGHT*sizeof(float));
        for (size_t i=0,iSz2=iSz;i<iSz2;++i){ float d=sc_irScaled_[i]; if(!std::isfinite(d)||d<=0)d=0; sc_ir_[i]=static_cast<uint16_t>(std::min(d,65535.f)); }
        pendingIROK_=true;
    }

    // 6. Point cloud — synchronous CPU
    pendingPCOK_ = false;
    if (p.enablePC && gotDepth && gotRGB && device && reg &&
        sc_rawDepth_.size()==static_cast<size_t>(DEPTH_WIDTH*DEPTH_HEIGHT) &&
        sc_rawRGB_.size()==static_cast<size_t>(RGB_WIDTH*RGB_HEIGHT*4)) {
        sc_pc_.resize(static_cast<size_t>(p.pcW)*p.pcH*4);
        std::memcpy(rgbFrame.data,  sc_rawRGB_.data(),   RGB_WIDTH*RGB_HEIGHT*4);
        std::memcpy(depthFrame.data,sc_rawDepth_.data(), DEPTH_WIDTH*DEPTH_HEIGHT*sizeof(float));
        reg->apply(&rgbFrame,&depthFrame,&undistortedFrame,&registeredFrame,true,nullptr);
        const int sW=DEPTH_WIDTH,sH=DEPTH_HEIGHT;
        sc_pcRaw_.resize(static_cast<size_t>(sW)*sH*4);
        float* o=sc_pcRaw_.data();
        for (int r=0;r<sH;++r) for (int c=0;c<sW;++c) {
            float x,y,z; reg->getPointXYZ(&undistortedFrame,r,c,x,y,z);
            size_t i=(size_t)(r*sW+c)*4;
            if(z>0){o[i]=x;o[i+1]=-y;o[i+2]=z;}else{o[i]=o[i+1]=o[i+2]=0;}
            o[i+3]=1.f;
        }
        sc_pcFlipped_.resize(sc_pcRaw_.size());
        vImage_Buffer sb={sc_pcRaw_.data(),(vImagePixelCount)sH,(vImagePixelCount)sW,(size_t)sW*4*sizeof(float)};
        vImage_Buffer fb={sc_pcFlipped_.data(),(vImagePixelCount)sH,(vImagePixelCount)sW,(size_t)sW*4*sizeof(float)};
        vImageHorizontalReflect_ARGBFFFF(&sb,&fb,kvImageDoNotTile);
        vImage_Buffer db={sc_pc_.data(),(vImagePixelCount)p.pcH,(vImagePixelCount)p.pcW,(size_t)p.pcW*4*sizeof(float)};
        if (p.pcW!=sW||p.pcH!=sH) vImageScale_ARGBFFFF(&fb,&db,nullptr,kvImageHighQualityResampling|kvImageDoNotTile);
        else std::memcpy(sc_pc_.data(),sc_pcFlipped_.data(),sc_pcFlipped_.size()*sizeof(float));
        pendingPCOK_=true;
    }

    // 7. Submit Metal async (non-blocking) — GPU processes color+depth while CPU waits for next Kinect frame
    const bool colorAvail = gotRGB && sc_rawRGB_.size()==static_cast<size_t>(RGB_WIDTH*RGB_HEIGHT*4);
    const bool depthAvail = depthSrc && depthOutW>0 && depthOutH>0;

    pendingColorOK_ = false;
    pendingDepthOK_ = false;
    pendingMetalSubmitted_ = false;

    if (metal_ && metal_->available() && (colorAvail || depthAvail)) {
        if (metal_->submitAsync(
                colorAvail ? sc_rawRGB_.data() : nullptr, RGB_WIDTH, RGB_HEIGHT, p.rgbW, p.rgbH,
                depthAvail ? depthSrc          : nullptr, depthSrcW, depthSrcH,  depthOutW, depthOutH,
                p.depthMin, p.depthMax)) {
            pendingColorOK_        = colorAvail;
            pendingDepthOK_        = depthAvail;
            pendingMetalSubmitted_ = true;
            return true;  // collectAndPublish must be called next iteration
        }
    }

    // Metal unavailable or failed — synchronous vImage fallback
    if (colorAvail) {
        const size_t cSz=static_cast<size_t>(p.rgbW)*p.rgbH*4;
        sc_color_.resize(cSz); sc_colorTmp_.resize(RGB_WIDTH*RGB_HEIGHT*4);
        vImage_Buffer src={sc_rawRGB_.data(),(vImagePixelCount)RGB_HEIGHT,(vImagePixelCount)RGB_WIDTH,(size_t)RGB_WIDTH*4};
        vImage_Buffer tmpBuf={sc_colorTmp_.data(),(vImagePixelCount)RGB_HEIGHT,(vImagePixelCount)RGB_WIDTH,(size_t)RGB_WIDTH*4};
        vImageHorizontalReflect_ARGB8888(&src,&tmpBuf,kvImageDoNotTile);
        vImage_Buffer dst={sc_color_.data(),(vImagePixelCount)p.rgbH,(vImagePixelCount)p.rgbW,(size_t)p.rgbW*4};
        if (p.rgbW!=RGB_WIDTH||p.rgbH!=RGB_HEIGHT)
            vImageScale_ARGB8888(&tmpBuf,&dst,nullptr,kvImageHighQualityResampling|kvImageDoNotTile);
        else std::memcpy(sc_color_.data(),sc_colorTmp_.data(),cSz);
        const uint8_t perm[4]={2,1,0,3};
        vImagePermuteChannels_ARGB8888(&dst,&dst,perm,kvImageNoFlags);
        pendingColorOK_=true;
    }
    if (depthAvail) {
        const size_t dSz=static_cast<size_t>(depthOutW)*depthOutH;
        const size_t srcSz=static_cast<size_t>(depthSrcW)*depthSrcH;
        sc_depth_.resize(dSz); sc_depthFlipped_.resize(srcSz); sc_depthScaled_.resize(dSz);
        vImage_Buffer srcBuf={const_cast<float*>(depthSrc),(vImagePixelCount)depthSrcH,(vImagePixelCount)depthSrcW,(size_t)depthSrcW*sizeof(float)};
        vImage_Buffer flipBuf={sc_depthFlipped_.data(),(vImagePixelCount)depthSrcH,(vImagePixelCount)depthSrcW,(size_t)depthSrcW*sizeof(float)};
        vImageHorizontalReflect_PlanarF(&srcBuf,&flipBuf,kvImageDoNotTile);
        vImage_Buffer dstBuf={sc_depthScaled_.data(),(vImagePixelCount)depthOutH,(vImagePixelCount)depthOutW,(size_t)depthOutW*sizeof(float)};
        if (depthOutW!=depthSrcW||depthOutH!=depthSrcH)
            vImageScale_PlanarF(&flipBuf,&dstBuf,nullptr,kvImageHighQualityResampling|kvImageDoNotTile);
        else std::memcpy(sc_depthScaled_.data(),sc_depthFlipped_.data(),srcSz*sizeof(float));
        const float denom=std::max(p.depthMax-p.depthMin,1.f);
        for (size_t i=0;i<dSz;++i) {
            const float d=sc_depthScaled_[i];
            if (!std::isfinite(d)||d<=p.depthMin||d>=p.depthMax) sc_depth_[i]=0;
            else { float n=(d-p.depthMin)/denom; if(n<0)n=0;if(n>1)n=1; sc_depth_[i]=static_cast<uint16_t>(n*65535.f); }
        }
        pendingDepthOK_=true;
    }

    // Synchronous path: publish immediately (no pending Metal work)
    {
        std::lock_guard<std::mutex> lock(readyMutex_);
        if (pendingColorOK_){ std::swap(ready_.color, sc_color_); ready_.colorW=p.rgbW; ready_.colorH=p.rgbH; ready_.hasColor=true; }
        if (pendingDepthOK_){ std::swap(ready_.depth, sc_depth_); ready_.depthW=depthOutW; ready_.depthH=depthOutH; ready_.hasDepth=true; }
        if (pendingIROK_)   { std::swap(ready_.ir,    sc_ir_);    ready_.irW=p.irW; ready_.irH=p.irH; ready_.hasIR=true; }
        if (pendingPCOK_)   { std::swap(ready_.pc,    sc_pc_);    ready_.pcW=p.pcW; ready_.pcH=p.pcH; ready_.hasPC=true; }
    }
    return false;  // no pending Metal work
}

// collectAndPublish — wait for async Metal + publish all streams.
// Called at the START of the next frame iteration, giving the GPU ~33ms to finish.
void MyFreenect2Device::collectAndPublish() {
    const ProcessingParams& p = pendingParams_;

    if (pendingMetalSubmitted_) {
        // waitUntilCompleted returns almost instantly — GPU had 33ms
        if (pendingColorOK_) sc_color_.resize(static_cast<size_t>(p.rgbW)*p.rgbH*4);
        if (pendingDepthOK_) sc_depth_.resize(static_cast<size_t>(pendingDepthOutW_)*pendingDepthOutH_);

        bool ok = metal_ && metal_->collectAsync(
            pendingColorOK_ ? sc_color_.data() : nullptr, p.rgbW,            p.rgbH,
            pendingDepthOK_ ? sc_depth_.data() : nullptr, pendingDepthOutW_, pendingDepthOutH_);

        if (!ok) { pendingColorOK_ = false; pendingDepthOK_ = false; }
    }

    {
        std::lock_guard<std::mutex> lock(readyMutex_);
        if (pendingColorOK_){ std::swap(ready_.color, sc_color_); ready_.colorW=p.rgbW; ready_.colorH=p.rgbH; ready_.hasColor=true; }
        if (pendingDepthOK_){ std::swap(ready_.depth, sc_depth_); ready_.depthW=pendingDepthOutW_; ready_.depthH=pendingDepthOutH_; ready_.hasDepth=true; }
        if (pendingIROK_)   { std::swap(ready_.ir,    sc_ir_);    ready_.irW=p.irW; ready_.irH=p.irH; ready_.hasIR=true; }
        if (pendingPCOK_)   { std::swap(ready_.pc,    sc_pc_);    ready_.pcW=p.pcW; ready_.pcH=p.pcH; ready_.hasPC=true; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cook-thread getters
// ─────────────────────────────────────────────────────────────────────────────

bool MyFreenect2Device::getColorFrame(std::vector<uint8_t>& out, int& w, int& h) {
    std::lock_guard<std::mutex> lock(readyMutex_);
    if (!ready_.hasColor) return false;
    std::swap(ready_.color, out); w=ready_.colorW; h=ready_.colorH; ready_.hasColor=false; return true;
}
bool MyFreenect2Device::getDepthFrame(std::vector<uint16_t>& out, int& w, int& h) {
    std::lock_guard<std::mutex> lock(readyMutex_);
    if (!ready_.hasDepth) return false;
    std::swap(ready_.depth, out); w=ready_.depthW; h=ready_.depthH; ready_.hasDepth=false; return true;
}
bool MyFreenect2Device::getIRFrame(std::vector<uint16_t>& out, int& w, int& h) {
    std::lock_guard<std::mutex> lock(readyMutex_);
    if (!ready_.hasIR) return false;
    std::swap(ready_.ir, out); w=ready_.irW; h=ready_.irH; ready_.hasIR=false; return true;
}
bool MyFreenect2Device::getPointCloudFrame(std::vector<float>& out, int& w, int& h) {
    std::lock_guard<std::mutex> lock(readyMutex_);
    if (!ready_.hasPC) return false;
    std::swap(ready_.pc, out); w=ready_.pcW; h=ready_.pcH; ready_.hasPC=false; return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy
// ─────────────────────────────────────────────────────────────────────────────

void MyFreenect2Device::setRGBBuffer(const std::vector<uint8_t>& buf, bool m) {
    std::lock_guard<std::mutex> lock(rawMutex_); rgbBuffer=buf; hasNewRGB=m; if(m)rgbReady=true;
}
void MyFreenect2Device::setDepthBuffer(const std::vector<float>& buf, bool m) {
    std::lock_guard<std::mutex> lock(rawMutex_); depthBuffer=buf; hasNewDepth=m; if(m)depthReady=true;
}
