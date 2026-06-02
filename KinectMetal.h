//
//  KinectMetal.h
//  FreenectTD
//
//  Pure C++ header (no ObjC types exposed) — uses pImpl to hide Metal internals.
//

#pragma once

#include <cstdint>

class KinectMetal {
public:
    KinectMetal();
    ~KinectMetal();

    // Returns false if Metal is unavailable on this system.
    bool available() const;

    // Convert BGRX (4-byte, Kinect V2 color) to RGBA with horizontal flip + optional scale.
    // Returns false on failure (caller should fall back to vImage).
    bool processColor(const uint8_t* bgrx, int srcW, int srcH,
                      uint8_t* rgba,       int dstW, int dstH);

    // Normalize float depth to uint16 with horizontal flip + optional scale.
    // Returns false on failure.
    bool processDepth(const float*   depth, int srcW, int srcH,
                      uint16_t*      out,   int dstW, int dstH,
                      float minD, float maxD);

    // Async API: submit GPU work non-blocking, collect results in a later call.
    // Overlap pattern: submitAsync() → [CPU/Kinect work] → collectAsync()
    // Pass nullptr for streams to skip. Returns false if Metal unavailable.
    bool submitAsync(const uint8_t* bgrx,     int cSrcW, int cSrcH, int cDstW, int cDstH,
                     const float*   depth,    int dSrcW, int dSrcH, int dDstW, int dDstH,
                     float minD, float maxD);
    bool collectAsync(uint8_t*  rgba,     int cDstW, int cDstH,
                      uint16_t* depthOut, int dDstW, int dDstH);

private:
    struct Impl;
    Impl* impl_;
};
