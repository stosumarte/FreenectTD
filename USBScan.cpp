//
//  USBScan.cpp
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#include "USBScan.h"
#include <libusb.h>

USBScan scanUSB() {
    USBScan scan;
    libusb_context* usb = nullptr;
    if (libusb_init(&usb) != 0) {
        return scan;
    }
    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usb, &list);
    scan.ok = count >= 0;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != 0x045E) {
            continue;
        }
        // Same camera IDs libfreenect counts: Xbox 360 Kinect and Kinect for Windows
        if (desc.idProduct == 0x02AE || desc.idProduct == 0x02BF) {
            scan.kinect1 = true;
        }
        // Same IDs libfreenect2 enumerates: Kinect for Windows v2 and Xbox One Kinect
        if (desc.idProduct == 0x02C4 || desc.idProduct == 0x02D8) {
            scan.kinect2 = true;
            int speed = libusb_get_device_speed(list[i]);
            // LIBUSB_SPEED_UNKNOWN is let through so an unreported speed doesn't block a working setup
            if (speed != LIBUSB_SPEED_UNKNOWN && speed < LIBUSB_SPEED_SUPER) {
                scan.kinect2Slow = true;
            }
        }
    }
    if (count >= 0) {
        libusb_free_device_list(list, 1);
    }
    libusb_exit(usb);
    return scan;
}
