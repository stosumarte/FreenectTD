//
//  USBScan.h
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#pragma once

#include <string>
#include <vector>

// One Kinect camera in the USB device list
struct KinectUSB {
    int bus = 0;
    std::string port; // port numbers from the bus root, e.g. "1.3": the same socket gives the same path after a replug
    bool v2 = false;
    int speed = 0;    // libusb_speed; libfreenect2 crashes the process opening a Kinect v2 below LIBUSB_SPEED_SUPER
    bool operator==(const KinectUSB&) const = default;
};

// One shared background thread lists every Kinect in the USB device list every 100 ms, without opening
// them, so a streaming device isn't disturbed. It runs while any Freenect TOP exists: each node calls
// startUSBScan once and stopUSBScan once.
void startUSBScan();
void stopUSBScan();
std::vector<KinectUSB> usbKinects(); // result of the last successful scan
