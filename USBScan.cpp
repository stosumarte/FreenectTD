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
#include <map>
#include <mutex>
#include <thread>

namespace {

struct Scanner {
    std::mutex             usersMutex; // guards users and thread
    int                    users = 0;
    std::thread            thread;
    std::atomic<bool>      running{false};
    std::mutex             listMutex;  // guards kinects and rebooting
    std::vector<KinectUSB> kinects;
    std::map<std::string, std::chrono::steady_clock::time_point> rebooting; // id -> when to stop waiting
};
// Never freed: if TouchDesigner quits without destroying a node, a static std::thread destroyed
// while still running would abort the process on the way out
Scanner& scanner = *new Scanner;

constexpr uint16_t MICROSOFT = 0x045E;

// Same camera IDs libfreenect counts: Xbox 360 Kinect and Kinect for Windows
bool isKinect1(const libusb_device_descriptor& desc) {
    return desc.idVendor == MICROSOFT && (desc.idProduct == 0x02AE || desc.idProduct == 0x02BF);
}

// Same IDs libfreenect2 enumerates: Xbox One and Kinect for Windows v2 (see modelName)
bool isKinect2(const libusb_device_descriptor& desc) {
    return desc.idVendor == MICROSOFT && (desc.idProduct == 0x02C4 || desc.idProduct == 0x02D8);
}

// libfreenect tells a 1414 from a 1473 by the camera's bcdDevice (267).
// NOTE: the Kinect v2 models are a guess from two sensors, an Xbox One one reporting 0x02C4 and a Kinect
// for Windows one reporting 0x02D8. No source maps these IDs to models: libfreenect2 calls 0x02C4 the
// preview ID, and Microsoft calls both sensors functionally identical. Other units may report the other ID.
std::string modelName(const libusb_device_descriptor& desc) {
    switch (desc.idProduct) {
        case 0x02AE: return desc.bcdDevice == 0x010B ? "1414" : "1473";
        case 0x02BF: return "K4W";
        case 0x02C4: return "One";
        case 0x02D8: return "K4W";
        default:     return "Unknown";
    }
}

// Port numbers from the bus root, e.g. "1.1.3"
std::string portPath(libusb_device* device) {
    uint8_t ports[7]; // USB allows at most 7 tiers
    const int depth = libusb_get_port_numbers(device, ports, sizeof(ports));
    std::string path;
    for (int p = 0; p < depth; ++p) {
        path += (p ? "." : "") + std::to_string(ports[p]);
    }
    return path;
}

// Opens the device to read its serial number. Only done for a Kinect that just showed up:
// opening one while it streams can disturb it. Empty if it can't be read or is all zeros.
std::string readSerial(libusb_device* device) {
    libusb_device_descriptor desc;
    libusb_device_handle* handle = nullptr;
    if (libusb_get_device_descriptor(device, &desc) != 0 || desc.iSerialNumber == 0 || libusb_open(device, &handle) != 0) {
        return "";
    }
    unsigned char serial[256];
    const int length = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, sizeof(serial));
    libusb_close(handle);
    const std::string s = length > 0 ? std::string(reinterpret_cast<char*>(serial), length) : "";
    return s.find_first_not_of('0') == std::string::npos ? "" : s;
}

// A Kinect v1's audio device: the one on the same internal hub as its camera, as libfreenect finds it
libusb_device* findAudio(libusb_device** list, ssize_t count, libusb_device* camera) {
    libusb_device* hub = libusb_get_parent(camera);
    for (ssize_t i = 0; hub && i < count; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_parent(list[i]) == hub && libusb_get_device_descriptor(list[i], &desc) == 0
            && desc.idVendor == MICROSOFT
            && (desc.idProduct == 0x02AD || desc.idProduct == 0x02BE || desc.idProduct == 0x02C3 || desc.idProduct == 0x02BB)) {
            return list[i];
        }
    }
    return nullptr;
}

// Fills kinects from the USB device list; false if libusb failed. A Kinect present in known
// (same bus, port and version) keeps its serial; only new ones are opened to read it. One that just
// left isn't trusted: another Kinect may have been plugged into its socket.
bool scan(std::vector<KinectUSB>& kinects, const std::vector<KinectUSB>& known) {
    libusb_context* usb = nullptr;
    if (libusb_init(&usb) != 0) {
        return false;
    }
    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usb, &list);
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        const bool v2 = isKinect2(desc);
        if (!v2 && !isKinect1(desc)) {
            continue;
        }
        KinectUSB kinect;
        kinect.bus = libusb_get_bus_number(list[i]);
        kinect.port = portPath(list[i]);
        kinect.v2 = v2;
        kinect.model = modelName(desc);
        kinect.speed = libusb_get_device_speed(list[i]);
        const auto same = std::find_if(known.begin(), known.end(), [&](const KinectUSB& k) {
            return k.present && k.bus == kinect.bus && k.port == kinect.port && k.v2 == kinect.v2;
        });
        if (same != known.end()) {
            kinect.serial = same->serial;
        } else if (v2 || kinect.model == "1414") {
            // Kinect v2 and model 1414 have their own serial
            kinect.serial = readSerial(list[i]);
        } else if (libusb_device* audio = findAudio(list, count, list[i])) {
            // Model 1473 and Kinect for Windows cameras report all zeros, so like libfreenect use the audio
            // device's serial. It survives libfreenect's firmware upload: libfreenect finds the audio device
            // again by that serial afterwards.
            kinect.serial = readSerial(audio);
        } else {
            continue; // still showing up: listed once its audio device is there too
        }
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
    const std::vector<KinectUSB> known = usbKinects(); // only this thread (or startUSBScan before it) writes the list
    std::vector<KinectUSB> kinects;
    if (!scan(kinects, known)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(scanner.listMutex);
    for (KinectUSB kinect : known) {
        if (std::any_of(kinects.begin(), kinects.end(), [&](const KinectUSB& k) { return k.id() == kinect.id(); })) {
            if (!kinect.present) {
                scanner.rebooting.erase(kinect.id()); // back from its reboot
            }
            continue;
        }
        // Gone: a Kinect expected to reboot stays listed, anything else was unplugged
        const auto reboot = scanner.rebooting.find(kinect.id());
        if (reboot != scanner.rebooting.end() && now < reboot->second) {
            kinect.present = false;
            kinects.push_back(kinect);
        }
    }
    std::erase_if(scanner.rebooting, [&](const auto& reboot) { return now >= reboot.second; });
    std::sort(kinects.begin(), kinects.end(), [](const KinectUSB& a, const KinectUSB& b) { return a.id() < b.id(); });
    if (kinects != scanner.kinects) {
        LOG("[USBScan] " + std::to_string(kinects.size()) + " Kinect(s)");
        for ([[maybe_unused]] const KinectUSB& k : kinects) { // LOG is empty in Release
            LOG(std::string("[USBScan]   ") + k.id() + " bus " + std::to_string(k.bus)
                + " port " + k.port + " speed " + std::to_string(k.speed) + " serial " + k.serial
                + (k.present ? "" : " (rebooting)"));
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

void expectReboot(const std::string& id) {
    std::lock_guard<std::mutex> lock(scanner.listMutex);
    scanner.rebooting[id] = std::chrono::steady_clock::now() + std::chrono::seconds(10);
}

int kinect1Index(libusb_context* usb, const KinectUSB& kinect) {
    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(usb, &list);
    int index = -1;
    for (ssize_t i = 0, cameras = 0; i < count && index < 0; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0 || !isKinect1(desc)) {
            continue;
        }
        if (libusb_get_bus_number(list[i]) == kinect.bus && portPath(list[i]) == kinect.port) {
            index = static_cast<int>(cameras);
        }
        ++cameras;
    }
    if (count >= 0) {
        libusb_free_device_list(list, 1);
    }
    return index;
}
