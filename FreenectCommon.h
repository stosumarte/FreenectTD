//
//  FreenectCommon.h
//  FreenectTD
//
//  Shared enums and helpers used by the TOP and both device backends.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <dispatch/dispatch.h>

// Which Kinect a device is
enum class kinectVersionEnum {
    None, // no Kinect (Auto with nothing plugged in)
    V1,   // Xbox 360 / Kinect for Windows, opened with libfreenect
    V2    // Xbox One / Kinect for Windows v2, opened with libfreenect2
};

// How the depth map is generated
enum class depthFormatEnum {
    Raw,            // native depth camera image (512x424 on v2, 640x480 on v1)
    RawUndistorted, // v2 only: lens-undistorted depth camera image
    Registered      // depth re-projected into the color camera (1920x1080 on v2, 640x480 on v1)
};

// How depth values are packed into the output texture
enum class depthOutputEnum {
    Normalized,   // Mono16Fixed, 0..1 across [threshMin, threshMax] (legacy behaviour)
    Millimeters,  // Mono32Float, value in mm
    Meters        // Mono32Float, value in m
    // In every mode, invalid / out-of-range pixels get the Unknown Depth Value
    // (clamped to 0..1 in Normalized)
};

// True if a depth value (mm) is a real reading inside the inclusive [min, max] depth range.
// 0, NaN and infinity are what the sensors report for "no reading".
inline bool isDepthInRange(float depthMM, float depthThreshMin, float depthThreshMax)
{
    return std::isfinite(depthMM) && depthMM > 0.0f && depthMM >= depthThreshMin && depthMM <= depthThreshMax;
}

// Nearest-neighbour resample with optional horizontal mirror, converting each value
// from TSrc to TDst. Depth / XYZ data must never be interpolated (bilinear blending
// across a depth edge invents points that float between foreground and background),
// so all depth-derived outputs go through this instead of vImageScale.
template <typename TSrc, typename TDst>
inline void resampleNearest(const TSrc* src, int srcWidth, int srcHeight, int channels,
                            TDst* dst, int dstWidth, int dstHeight, bool flipX)
{
    // Rows are independent, so spread them across cores (dispatch_apply returns when all are done)
    dispatch_apply(dstHeight, DISPATCH_APPLY_AUTO, ^(size_t row) {
        const int y = static_cast<int>(row);
        const int sy = (dstHeight == srcHeight) ? y : std::min(srcHeight - 1, static_cast<int>((y + 0.5f) * srcHeight / dstHeight));
        const TSrc* srcRow = src + static_cast<size_t>(sy) * srcWidth * channels;
        TDst* dstRow = dst + static_cast<size_t>(y) * dstWidth * channels;
        for (int x = 0; x < dstWidth; ++x) {
            int sx = (dstWidth == srcWidth) ? x : std::min(srcWidth - 1, static_cast<int>((x + 0.5f) * srcWidth / dstWidth));
            if (flipX) {
                sx = srcWidth - 1 - sx;
            }
            const TSrc* srcPixel = srcRow + static_cast<size_t>(sx) * channels;
            TDst* dstPixel = dstRow + static_cast<size_t>(x) * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<TDst>(srcPixel[c]);
            }
        }
    });
}

// Packs a millimetre depth map (0 = invalid) for the depth output: Mono16Fixed (uint16) for Normalized,
// Mono32Float otherwise. Invalid pixels get unknownDepth (clamped to 0..1 in Normalized).
inline void packDepth(const float* depthMM, size_t pixelCount, void* dst, depthOutputEnum mode,
                      float threshMin, float threshMax, float unknownDepth)
{
    if (mode == depthOutputEnum::Normalized) {
        // Legacy behaviour: 0..1 across the threshold window
        uint16_t* out = static_cast<uint16_t*>(dst);
        const float denom = std::max(threshMax - threshMin, 1.0f);
        const uint16_t unknown = static_cast<uint16_t>(std::clamp(unknownDepth, 0.0f, 1.0f) * 65535.0f + 0.5f);
        for (size_t i = 0; i < pixelCount; ++i) {
            const float d = depthMM[i];
            out[i] = (d <= 0.0f) ? unknown
                                 : static_cast<uint16_t>(std::clamp((d - threshMin) / denom, 0.0f, 1.0f) * 65535.0f + 0.5f);
        }
    } else {
        float* out = static_cast<float*>(dst);
        const float scale = (mode == depthOutputEnum::Meters) ? 0.001f : 1.0f;
        for (size_t i = 0; i < pixelCount; ++i) {
            out[i] = (depthMM[i] > 0.0f) ? depthMM[i] * scale : unknownDepth;
        }
    }
}
