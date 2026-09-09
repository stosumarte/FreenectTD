//
//  FreenectTypes.h
//  FreenectTD
//
//  Shared enums used by the TOP and both device backends.
//

#pragma once

// How the depth map is generated
enum class depthFormatEnum {
    Raw,            // native depth camera image (512x424 on v2, 640x480 on v1)
    RawUndistorted, // v2 only: lens-undistorted depth camera image
    Registered      // depth re-projected into the color camera (1920x1080 on v2, 640x480 on v1)
};

// How depth values are packed into the output texture
enum class depthOutputEnum {
    Normalized,   // Mono16Fixed, 0..1 across [threshMin, threshMax] (legacy behaviour)
    Millimeters,  // Mono32Float, value in mm, 0 = invalid / outside threshold
    Meters        // Mono32Float, value in m,  0 = invalid / outside threshold
};

// Point Cloud Format parameter (v2 only): which camera the point cloud is expressed in
enum class pcSpaceEnum {
    DepthCamera,  // "Raw": 512x424, XYZ (m) relative to the depth/IR camera; aligned with Registered Color / UV outputs
    ColorCamera   // "Registered": 1920x1080, XYZ (m) relative to the color camera, pixel-aligned to the RGB output
};
