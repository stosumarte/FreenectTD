#pragma once

// Microsoft's firmware for the audio chip of the Kinect v1 models 1473 and Kinect for Windows, as byte arrays.
// On those models the motor (tilt) and LED are controlled through the audio chip, which only works once this
// firmware is uploaded: fn1_initDevice hands both images to libfreenect (freenect_set_fw_address_nui / _k4w),
// which uploads them when needed. The camera streams without it. From openFrameworks' ofxKinect addon.
#include <iostream>

#define OFX_KINECT_EXTRA_FW 

class ofxKinectExtras{

    public:
        static int getFWSize1473();
        static unsigned char * getFWData1473();
    
        static int getFWSizek4w();
        static unsigned char * getFWDatak4w();
};
