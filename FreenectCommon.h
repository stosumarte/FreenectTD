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

// Which camera the v2 point cloud is expressed in; derived from the Format menu
// (Registered -> ColorCamera, Raw / Raw undistorted -> DepthCamera)
enum class pcSpaceEnum {
    DepthCamera,  // "Raw": 512x424, XYZ (m) relative to the depth/IR camera; aligned with Registered Color / UV outputs
    ColorCamera   // "Registered": 1920x1080, XYZ (m) relative to the color camera, pixel-aligned to the RGB output
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
    for (int y = 0; y < dstHeight; ++y) {
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
    }
}
