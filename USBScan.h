//
//  USBScan.h
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#pragma once

// Looks for Kinects in the USB device list without opening them, so a streaming device isn't disturbed.
// kinect2Slow = a Kinect v2 attached at less than USB 3 speed; libfreenect2 crashes the process opening it there.
// NOTE: looks at every Kinect, not just the one the libraries pick; one shared scan listing devices
// by bus+port when multi-Kinect setups matter
struct USBScan { bool ok = false, kinect1 = false, kinect2 = false, kinect2Slow = false; };
USBScan scanUSB();
