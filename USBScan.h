//
//  USBScan.h
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#pragma once

#include <string>
#include <vector>

struct libusb_context;

// One Kinect camera in the USB device list
struct KinectUSB {
    int bus = 0;
    std::string port; // port numbers from the bus root, e.g. "1.3": the same socket gives the same path after a replug
    bool v2 = false;
    std::string model; // "1414", "1473" or "K4W" for v1; "K4W" or "One" for v2
    int speed = 0;    // libusb_speed; libfreenect2 crashes the process opening a Kinect v2 below LIBUSB_SPEED_SUPER
    std::string serial; // read once when the Kinect first shows up; empty if it couldn't be read or isn't unique
    bool operator==(const KinectUSB&) const = default;
    // Device menu value: version, model and serial, or bus+port without one, e.g. "v2-One:504521442542",
    // "v1-1473:usb0-1.1.2". The model is in it so the label of an unplugged picked Kinect stays the same.
    std::string id() const {
        return (v2 ? "v2-" : "v1-") + model + ":" + (serial.empty() ? "usb" + std::to_string(bus) + "-" + port : serial);
    }
};

// One shared background thread lists every Kinect in the USB device list every 100 ms. A Kinect is
// only opened once, when it first shows up, to read its serial, so a streaming device isn't disturbed.
// It runs while any Freenect TOP exists: each node calls startUSBScan once and stopUSBScan once.
void startUSBScan();
void stopUSBScan();
std::vector<KinectUSB> usbKinects(); // result of the last successful scan, sorted by id (version, then serial)

// Position of a Kinect v1 camera in usb's device list, counted like libfreenect's open by index; -1 if
// it isn't there. Pass the context libfreenect runs on, so both count in the same list.
int kinect1Index(libusb_context* usb, const KinectUSB& kinect);
