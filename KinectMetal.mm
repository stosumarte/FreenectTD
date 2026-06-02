//
//  KinectMetal.mm
//  FreenectTD
//
//  Objective-C++ implementation of Metal-accelerated Kinect frame processing.
//  Metal shader source is embedded as a string to avoid needing a .metal file
//  in the Xcode project.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "KinectMetal.h"

// ---------------------------------------------------------------------------
// Metal shader source (embedded as NSString)
// ---------------------------------------------------------------------------
static NSString* const kShaderSource = @R"MTL(
#include <metal_stdlib>
using namespace metal;

struct ColorParams {
    uint srcW, srcH, dstW, dstH;
};

kernel void processColor(
    device const uchar4*      src    [[buffer(0)]],
    device       uchar4*      dst    [[buffer(1)]],
    constant     ColorParams& p      [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= p.dstW || gid.y >= p.dstH) return;

    // Horizontal flip: map destination x -> source x (mirrored)
    float u = (p.dstW > 1) ? float(p.dstW - 1 - gid.x) / float(p.dstW - 1) : 0.5f;
    float v = (p.dstH > 1) ? float(gid.y)               / float(p.dstH - 1) : 0.5f;

    float sx = u * float(p.srcW - 1);
    float sy = v * float(p.srcH - 1);

    uint x0 = uint(sx);  uint y0 = uint(sy);
    uint x1 = min(x0 + 1u, p.srcW - 1u);
    uint y1 = min(y0 + 1u, p.srcH - 1u);
    float fx = sx - float(x0);
    float fy = sy - float(y0);

    // BGRX input: .x=B .y=G .z=R .w=X  →  output RGBA
    uchar4 p00 = src[y0 * p.srcW + x0];
    uchar4 p10 = src[y0 * p.srcW + x1];
    uchar4 p01 = src[y1 * p.srcW + x0];
    uchar4 p11 = src[y1 * p.srcW + x1];

    // Expand to float (swap R and B for BGRX → RGBA)
    float4 f00 = float4(p00.z, p00.y, p00.x, 255.0f);
    float4 f10 = float4(p10.z, p10.y, p10.x, 255.0f);
    float4 f01 = float4(p01.z, p01.y, p01.x, 255.0f);
    float4 f11 = float4(p11.z, p11.y, p11.x, 255.0f);

    float4 c = mix(mix(f00, f10, fx), mix(f01, f11, fx), fy);

    dst[gid.y * p.dstW + gid.x] = uchar4(uchar(c.x), uchar(c.y), uchar(c.z), 255u);
}

struct DepthParams {
    uint  srcW, srcH, dstW, dstH;
    float minD, maxD;
};

kernel void processDepth(
    device const float*       src    [[buffer(0)]],
    device       ushort*      dst    [[buffer(1)]],
    constant     DepthParams& p      [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= p.dstW || gid.y >= p.dstH) return;

    float u = (p.dstW > 1) ? float(p.dstW - 1 - gid.x) / float(p.dstW - 1) : 0.5f;
    float v = (p.dstH > 1) ? float(gid.y)               / float(p.dstH - 1) : 0.5f;

    uint sx = min(uint(round(u * float(p.srcW - 1))), p.srcW - 1u);
    uint sy = min(uint(round(v * float(p.srcH - 1))), p.srcH - 1u);

    float d = src[sy * p.srcW + sx];
    float denom = max(p.maxD - p.minD, 1.0f);

    ushort out = 0;
    if (isfinite(d) && d > p.minD && d < p.maxD)
        out = ushort(clamp((d - p.minD) / denom, 0.0f, 1.0f) * 65535.0f);

    dst[gid.y * p.dstW + gid.x] = out;
}
)MTL";

// ---------------------------------------------------------------------------
// Impl struct
// ---------------------------------------------------------------------------
struct KinectMetal::Impl {
    id<MTLDevice>               device    = nil;
    id<MTLCommandQueue>         queue     = nil;
    id<MTLComputePipelineState> colorPSO  = nil;
    id<MTLComputePipelineState> depthPSO  = nil;

    id<MTLBuffer> inColor  = nil;
    id<MTLBuffer> inDepth  = nil;
    id<MTLBuffer> outColor = nil;
    id<MTLBuffer> outDepth = nil;

    size_t inColorSz  = 0;
    size_t inDepthSz  = 0;
    size_t outColorSz = 0;
    size_t outDepthSz = 0;

    // Async pipeline state
    id<MTLCommandBuffer> pendingCmd    = nil;
    bool                 asyncHasColor = false;
    bool                 asyncHasDepth = false;

    bool ready = false;
};

// ---------------------------------------------------------------------------
// Helper: grow a buffer if needed (never shrinks)
// ---------------------------------------------------------------------------
static id<MTLBuffer> ensureBuf(id<MTLDevice> dev, id<MTLBuffer> buf,
                                size_t& cur, size_t needed)
{
    if (cur >= needed) return buf;
    buf = [dev newBufferWithLength:needed
                           options:MTLResourceStorageModeShared];
    cur = (buf != nil) ? needed : 0;
    return buf;
}

// ---------------------------------------------------------------------------
// KinectMetal constructor / destructor
// ---------------------------------------------------------------------------
KinectMetal::KinectMetal() : impl_(new Impl())
{
    @autoreleasepool {
        // Pick the default (best available) GPU
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) return;

        impl_->device = dev;

        // Command queue
        id<MTLCommandQueue> q = [dev newCommandQueue];
        if (!q) return;
        impl_->queue = q;

        // Compile shaders from source
        NSError* err = nil;
        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        opts.languageVersion = MTLLanguageVersion2_4;

        id<MTLLibrary> lib = [dev newLibraryWithSource:kShaderSource
                                               options:opts
                                                 error:&err];
        if (!lib) {
            NSLog(@"[KinectMetal] Shader compile error: %@",
                  err ? err.localizedDescription : @"(unknown)");
            return;
        }

        // Color pipeline
        id<MTLFunction> colorFn = [lib newFunctionWithName:@"processColor"];
        if (!colorFn) { NSLog(@"[KinectMetal] processColor function not found"); return; }

        id<MTLComputePipelineState> colorPSO =
            [dev newComputePipelineStateWithFunction:colorFn error:&err];
        if (!colorPSO) {
            NSLog(@"[KinectMetal] colorPSO error: %@", err.localizedDescription);
            return;
        }
        impl_->colorPSO = colorPSO;

        // Depth pipeline
        id<MTLFunction> depthFn = [lib newFunctionWithName:@"processDepth"];
        if (!depthFn) { NSLog(@"[KinectMetal] processDepth function not found"); return; }

        id<MTLComputePipelineState> depthPSO =
            [dev newComputePipelineStateWithFunction:depthFn error:&err];
        if (!depthPSO) {
            NSLog(@"[KinectMetal] depthPSO error: %@", err.localizedDescription);
            return;
        }
        impl_->depthPSO = depthPSO;

        impl_->ready = true;
        NSLog(@"[KinectMetal] Initialized on device: %@", dev.name);
    }
}

KinectMetal::~KinectMetal()
{
    // ARC handles release of all ObjC objects; just delete the Impl struct.
    delete impl_;
    impl_ = nullptr;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool KinectMetal::available() const
{
    return impl_ && impl_->ready;
}

bool KinectMetal::processColor(const uint8_t* bgrx, int srcW, int srcH,
                                uint8_t*       rgba,  int dstW, int dstH)
{
    if (!impl_ || !impl_->ready) return false;
    if (!bgrx || !rgba || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return false;

    @autoreleasepool {
        id<MTLDevice> dev = impl_->device;

        // Ensure input / output buffers
        const size_t inSz  = (size_t)srcW * srcH * 4;
        const size_t outSz = (size_t)dstW * dstH * 4;

        impl_->inColor  = ensureBuf(dev, impl_->inColor,  impl_->inColorSz,  inSz);
        impl_->outColor = ensureBuf(dev, impl_->outColor, impl_->outColorSz, outSz);
        if (!impl_->inColor || !impl_->outColor) return false;

        // Copy input data
        memcpy(impl_->inColor.contents, bgrx, inSz);

        // Build params
        struct { uint32_t srcW, srcH, dstW, dstH; } params = {
            (uint32_t)srcW, (uint32_t)srcH, (uint32_t)dstW, (uint32_t)dstH
        };

        // Encode and dispatch
        id<MTLCommandBuffer>        cmd     = [impl_->queue commandBuffer];
        id<MTLComputeCommandEncoder> enc    = [cmd computeCommandEncoder];
        [enc setComputePipelineState:impl_->colorPSO];
        [enc setBuffer:impl_->inColor  offset:0 atIndex:0];
        [enc setBuffer:impl_->outColor offset:0 atIndex:1];
        [enc setBytes:&params length:sizeof(params) atIndex:2];

        NSUInteger tw = impl_->colorPSO.threadExecutionWidth;
        NSUInteger th = impl_->colorPSO.maxTotalThreadsPerThreadgroup / tw;
        MTLSize threads    = MTLSizeMake(tw, th, 1);
        MTLSize grid       = MTLSizeMake((NSUInteger)dstW, (NSUInteger)dstH, 1);
        [enc dispatchThreads:grid threadsPerThreadgroup:threads];
        [enc endEncoding];

        [cmd commit];
        [cmd waitUntilCompleted];

        if (cmd.error) {
            NSLog(@"[KinectMetal] processColor GPU error: %@", cmd.error.localizedDescription);
            return false;
        }

        // Copy result
        memcpy(rgba, impl_->outColor.contents, outSz);
        return true;
    }
}

bool KinectMetal::processDepth(const float* depth, int srcW, int srcH,
                                uint16_t*    out,   int dstW, int dstH,
                                float minD, float maxD)
{
    if (!impl_ || !impl_->ready) return false;
    if (!depth || !out || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return false;

    @autoreleasepool {
        id<MTLDevice> dev = impl_->device;

        const size_t inSz  = (size_t)srcW * srcH * sizeof(float);
        const size_t outSz = (size_t)dstW * dstH * sizeof(uint16_t);

        impl_->inDepth  = ensureBuf(dev, impl_->inDepth,  impl_->inDepthSz,  inSz);
        impl_->outDepth = ensureBuf(dev, impl_->outDepth, impl_->outDepthSz, outSz);
        if (!impl_->inDepth || !impl_->outDepth) return false;

        memcpy(impl_->inDepth.contents, depth, inSz);

        struct {
            uint32_t srcW, srcH, dstW, dstH;
            float    minD, maxD;
        } params = {
            (uint32_t)srcW, (uint32_t)srcH, (uint32_t)dstW, (uint32_t)dstH,
            minD, maxD
        };

        id<MTLCommandBuffer>         cmd = [impl_->queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:impl_->depthPSO];
        [enc setBuffer:impl_->inDepth  offset:0 atIndex:0];
        [enc setBuffer:impl_->outDepth offset:0 atIndex:1];
        [enc setBytes:&params length:sizeof(params) atIndex:2];

        NSUInteger tw = impl_->depthPSO.threadExecutionWidth;
        NSUInteger th = impl_->depthPSO.maxTotalThreadsPerThreadgroup / tw;
        MTLSize threads = MTLSizeMake(tw, th, 1);
        MTLSize grid    = MTLSizeMake((NSUInteger)dstW, (NSUInteger)dstH, 1);
        [enc dispatchThreads:grid threadsPerThreadgroup:threads];
        [enc endEncoding];

        [cmd commit];
        [cmd waitUntilCompleted];

        if (cmd.error) {
            NSLog(@"[KinectMetal] processDepth GPU error: %@", cmd.error.localizedDescription);
            return false;
        }

        memcpy(out, impl_->outDepth.contents, outSz);
        return true;
    }
}

bool KinectMetal::submitAsync(
    const uint8_t* bgrx,     int cSrcW, int cSrcH, int cDstW, int cDstH,
    const float*   depth,    int dSrcW, int dSrcH, int dDstW, int dDstH,
    float minD, float maxD)
{
    if (!impl_ || !impl_->ready) return false;

    @autoreleasepool {
        id<MTLDevice> dev = impl_->device;
        id<MTLCommandBuffer> cmd = [impl_->queue commandBuffer];

        impl_->asyncHasColor = false;
        impl_->asyncHasDepth = false;

        // Color pass
        if (bgrx && cSrcW > 0 && cSrcH > 0 && cDstW > 0 && cDstH > 0) {
            const size_t inSz  = (size_t)cSrcW * cSrcH * 4;
            const size_t outSz = (size_t)cDstW * cDstH * 4;
            impl_->inColor  = ensureBuf(dev, impl_->inColor,  impl_->inColorSz,  inSz);
            impl_->outColor = ensureBuf(dev, impl_->outColor, impl_->outColorSz, outSz);
            if (impl_->inColor && impl_->outColor) {
                memcpy(impl_->inColor.contents, bgrx, inSz);
                struct { uint32_t srcW, srcH, dstW, dstH; } p = {
                    (uint32_t)cSrcW, (uint32_t)cSrcH, (uint32_t)cDstW, (uint32_t)cDstH
                };
                id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
                [enc setComputePipelineState:impl_->colorPSO];
                [enc setBuffer:impl_->inColor  offset:0 atIndex:0];
                [enc setBuffer:impl_->outColor offset:0 atIndex:1];
                [enc setBytes:&p length:sizeof(p) atIndex:2];
                NSUInteger tw = impl_->colorPSO.threadExecutionWidth;
                NSUInteger th = impl_->colorPSO.maxTotalThreadsPerThreadgroup / tw;
                [enc dispatchThreads:MTLSizeMake((NSUInteger)cDstW, (NSUInteger)cDstH, 1)
                 threadsPerThreadgroup:MTLSizeMake(tw, th, 1)];
                [enc endEncoding];
                impl_->asyncHasColor = true;
            }
        }

        // Depth pass
        if (depth && dSrcW > 0 && dSrcH > 0 && dDstW > 0 && dDstH > 0) {
            const size_t inSz  = (size_t)dSrcW * dSrcH * sizeof(float);
            const size_t outSz = (size_t)dDstW * dDstH * sizeof(uint16_t);
            impl_->inDepth  = ensureBuf(dev, impl_->inDepth,  impl_->inDepthSz,  inSz);
            impl_->outDepth = ensureBuf(dev, impl_->outDepth, impl_->outDepthSz, outSz);
            if (impl_->inDepth && impl_->outDepth) {
                memcpy(impl_->inDepth.contents, depth, inSz);
                struct { uint32_t srcW, srcH, dstW, dstH; float minD, maxD; } p = {
                    (uint32_t)dSrcW, (uint32_t)dSrcH, (uint32_t)dDstW, (uint32_t)dDstH,
                    minD, maxD
                };
                id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
                [enc setComputePipelineState:impl_->depthPSO];
                [enc setBuffer:impl_->inDepth  offset:0 atIndex:0];
                [enc setBuffer:impl_->outDepth offset:0 atIndex:1];
                [enc setBytes:&p length:sizeof(p) atIndex:2];
                NSUInteger tw = impl_->depthPSO.threadExecutionWidth;
                NSUInteger th = impl_->depthPSO.maxTotalThreadsPerThreadgroup / tw;
                [enc dispatchThreads:MTLSizeMake((NSUInteger)dDstW, (NSUInteger)dDstH, 1)
                 threadsPerThreadgroup:MTLSizeMake(tw, th, 1)];
                [enc endEncoding];
                impl_->asyncHasDepth = true;
            }
        }

        [cmd commit];  // non-blocking — GPU starts immediately
        impl_->pendingCmd = cmd;
        return impl_->asyncHasColor || impl_->asyncHasDepth;
    }
}

bool KinectMetal::collectAsync(
    uint8_t*  rgba,     int cDstW, int cDstH,
    uint16_t* depthOut, int dDstW, int dDstH)
{
    if (!impl_ || !impl_->pendingCmd) return false;

    @autoreleasepool {
        [impl_->pendingCmd waitUntilCompleted];  // GPU had ~33ms — returns almost instantly
        bool ok = (impl_->pendingCmd.error == nil);
        impl_->pendingCmd = nil;
        if (!ok) return false;

        if (rgba && impl_->asyncHasColor && impl_->outColor && cDstW > 0 && cDstH > 0)
            memcpy(rgba, impl_->outColor.contents, (size_t)cDstW * cDstH * 4);
        if (depthOut && impl_->asyncHasDepth && impl_->outDepth && dDstW > 0 && dDstH > 0)
            memcpy(depthOut, impl_->outDepth.contents, (size_t)dDstW * dDstH * sizeof(uint16_t));
        return true;
    }
}

