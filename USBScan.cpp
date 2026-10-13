//
//  USBScan.cpp
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#include "USBScan.h"
#include "logger.h"
#include <libusb.h>
#include <algorithm>
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

// Opens the device to read its serial number, the same string libfreenect and libfreenect2 match on.
// Only done for a Kinect that just showed up: opening one while it streams can disturb it.
std::string readSerial(libusb_device* device, uint8_t serialIndex) {
    libusb_device_handle* handle = nullptr;
    if (serialIndex == 0 || libusb_open(device, &handle) != 0) {
        return "";
    }
    unsigned char serial[256];
    const int length = libusb_get_string_descriptor_ascii(handle, serialIndex, serial, sizeof(serial));
    libusb_close(handle);
    const std::string s = length > 0 ? std::string(reinterpret_cast<char*>(serial), length) : "";
    // NOTE: Kinect for Windows and model 1473 cameras report all zeros. libfreenect uses the audio
    // device's serial for those instead; here they're left empty and told apart by bus+port.
    return s.find_first_not_of('0') == std::string::npos ? "" : s;
}

// Fills kinects from the USB device list; false if libusb failed. A Kinect already in known
// (same bus, port and version) keeps its serial; only new ones are opened to read it.
bool scan(std::vector<KinectUSB>& kinects, const std::vector<KinectUSB>& known) {
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
        const auto same = std::find_if(known.begin(), known.end(), [&](const KinectUSB& k) {
            return k.bus == kinect.bus && k.port == kinect.port && k.v2 == kinect.v2;
        });
        kinect.serial = same != known.end() ? same->serial : readSerial(list[i], desc.iSerialNumber);
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
    if (!scan(kinects, usbKinects())) { // only this thread (or startUSBScan before it) writes the list
        return;
    }
    std::lock_guard<std::mutex> lock(scanner.listMutex);
    if (kinects != scanner.kinects) {
        LOG("[USBScan] " + std::to_string(kinects.size()) + " Kinect(s)");
        for ([[maybe_unused]] const KinectUSB& k : kinects) { // LOG is empty in Release
            LOG(std::string("[USBScan]   ") + (k.v2 ? "v2" : "v1") + " bus " + std::to_string(k.bus)
                + " port " + k.port + " speed " + std::to_string(k.speed) + " serial " + k.serial);
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
