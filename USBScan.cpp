//
//  USBScan.cpp
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#include "USBScan.h"
#include "logger.h"
#include <libusb.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace {

struct Scanner {
    std::mutex             usersMutex; // guards users and thread
    int                    users = 0;
    std::thread            thread;
    std::atomic<bool>      running{false};
    std::mutex             listMutex;  // guards kinects
    std::vector<KinectUSB> kinects;
};
// Never freed: if TouchDesigner quits without destroying a node, a static std::thread destroyed
// while still running would abort the process on the way out
Scanner& scanner = *new Scanner;

// Fills kinects from the USB device list; false if libusb failed
bool scan(std::vector<KinectUSB>& kinects) {
    libusb_context* usb = nullptr;
    if (libusb_init(&usb) != 0) {
        return false;
    }
    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usb, &list);
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0 || desc.idVendor != 0x045E) {
            continue;
        }
        // Same camera IDs libfreenect counts: Xbox 360 Kinect and Kinect for Windows
        const bool v1 = desc.idProduct == 0x02AE || desc.idProduct == 0x02BF;
        // Same IDs libfreenect2 enumerates: Kinect for Windows v2 and Xbox One Kinect
        const bool v2 = desc.idProduct == 0x02C4 || desc.idProduct == 0x02D8;
        if (!v1 && !v2) {
            continue;
        }
        KinectUSB kinect;
        kinect.bus = libusb_get_bus_number(list[i]);
        uint8_t ports[7]; // USB allows at most 7 tiers
        const int depth = libusb_get_port_numbers(list[i], ports, sizeof(ports));
        for (int p = 0; p < depth; ++p) {
            kinect.port += (p ? "." : "") + std::to_string(ports[p]);
        }
        kinect.v2 = v2;
        kinect.speed = libusb_get_device_speed(list[i]);
        kinects.push_back(kinect);
    }
    if (count >= 0) {
        libusb_free_device_list(list, 1);
    }
    libusb_exit(usb);
    return count >= 0;
}

// Keeps the last list if libusb fails
void scanOnce() {
    std::vector<KinectUSB> kinects;
    if (!scan(kinects)) {
        return;
    }
    std::lock_guard<std::mutex> lock(scanner.listMutex);
    if (kinects != scanner.kinects) {
        LOG("[USBScan] " + std::to_string(kinects.size()) + " Kinect(s)");
        for ([[maybe_unused]] const KinectUSB& k : kinects) { // LOG is empty in Release
            LOG(std::string("[USBScan]   ") + (k.v2 ? "v2" : "v1") + " bus " + std::to_string(k.bus)
                + " port " + k.port + " speed " + std::to_string(k.speed));
        }
    }
    scanner.kinects = std::move(kinects);
}

} // namespace

void startUSBScan() {
    std::lock_guard<std::mutex> lock(scanner.usersMutex);
    if (scanner.users++ > 0) {
        return;
    }
    scanOnce(); // so the first cook already sees what's plugged in
    scanner.running = true;
    scanner.thread = std::thread([]() {
        while (scanner.running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            scanOnce();
        }
    });
}

void stopUSBScan() {
    std::lock_guard<std::mutex> lock(scanner.usersMutex);
    if (--scanner.users > 0) {
        return;
    }
    scanner.running = false;
    scanner.thread.join();
    std::lock_guard<std::mutex> listLock(scanner.listMutex);
    scanner.kinects.clear(); // a Kinect unplugged before the next startUSBScan must not linger
}

std::vector<KinectUSB> usbKinects() {
    std::lock_guard<std::mutex> lock(scanner.listMutex);
    return scanner.kinects;
}
