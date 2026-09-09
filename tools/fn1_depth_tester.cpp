#include "FreenectCommon.h"
#include "FreenectV1.h"
#include "logger.h"
#include "ofxKinectExtras/ofxKinectExtras.h"

#include <libfreenect/libfreenect.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

enum class TestMode {
    RawCallback,
    PluginPath
};

struct Options {
    TestMode mode = TestMode::PluginPath;
    depthFormatEnum format = depthFormatEnum::Raw;
    int durationSeconds = 20;
    int toggleEverySeconds = 0;
    int printEveryFrames = 15;
    float thresholdMin = 400.0f;
    float thresholdMax = 4500.0f;
};

struct FrameStats {
    size_t total = 0;
    size_t nonZero = 0;
    uint16_t min = 0;
    uint16_t max = 0;
};

const char* toString(TestMode mode) {
    switch (mode) {
        case TestMode::RawCallback:
            return "raw-callback";
        case TestMode::PluginPath:
            return "plugin-path";
    }
    return "unknown";
}

const char* toString(depthFormatEnum format) {
    switch (format) {
        case depthFormatEnum::Raw:
            return "raw";
        case depthFormatEnum::RawUndistorted:
            return "raw-undistorted";
        case depthFormatEnum::Registered:
            return "registered";
    }
    return "unknown";
}

FrameStats analyzeFrame(const std::vector<uint16_t>& frame) {
    FrameStats stats;
    stats.total = frame.size();
    stats.min = std::numeric_limits<uint16_t>::max();

    for (uint16_t value : frame) {
        if (value == 0) {
            continue;
        }
        stats.nonZero++;
        stats.min = std::min(stats.min, value);
        stats.max = std::max(stats.max, value);
    }

    if (stats.nonZero == 0) {
        stats.min = 0;
    }

    return stats;
}

std::string formatCoverage(const FrameStats& stats) {
    std::ostringstream out;
    const double coverage = stats.total == 0
        ? 0.0
        : (static_cast<double>(stats.nonZero) * 100.0 / static_cast<double>(stats.total));
    out << std::fixed << std::setprecision(2) << coverage;
    return out.str();
}

void printUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Modes:\n"
        << "  --mode plugin-path   Mirrors the current plugin path via getDepthFrame() (default)\n"
        << "  --mode raw-callback  Reads raw depth callbacks without the plugin conversion path\n\n"
        << "Options:\n"
        << "  --format raw|registered      Initial depth format (default: raw)\n"
        << "  --duration <seconds>         Total test duration (default: 20)\n"
        << "  --toggle-every <seconds>     Alternate raw/registered every N seconds (default: off)\n"
        << "  --print-every <frames>       Print stats every N received frames (default: 15)\n"
        << "  --threshold-min <mm>         Min depth threshold for plugin-path mode (default: 400)\n"
        << "  --threshold-max <mm>         Max depth threshold for plugin-path mode (default: 4500)\n"
        << "  --help                       Show this help\n";
}

bool parseIntArg(const char* value, int& out) {
    try {
        out = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseFloatArg(const char* value, float& out) {
    try {
        out = std::stof(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return false;
        }

        if (arg == "--mode" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "plugin-path") {
                options.mode = TestMode::PluginPath;
            } else if (value == "raw-callback") {
                options.mode = TestMode::RawCallback;
            } else {
                std::cerr << "Invalid mode: " << value << '\n';
                return false;
            }
            continue;
        }

        if (arg == "--format" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "raw") {
                options.format = depthFormatEnum::Raw;
            } else if (value == "registered") {
                options.format = depthFormatEnum::Registered;
            } else {
                std::cerr << "Invalid format: " << value << '\n';
                return false;
            }
            continue;
        }

        if (arg == "--duration" && i + 1 < argc) {
            if (!parseIntArg(argv[++i], options.durationSeconds) || options.durationSeconds <= 0) {
                std::cerr << "Invalid duration\n";
                return false;
            }
            continue;
        }

        if (arg == "--toggle-every" && i + 1 < argc) {
            if (!parseIntArg(argv[++i], options.toggleEverySeconds) || options.toggleEverySeconds < 0) {
                std::cerr << "Invalid toggle interval\n";
                return false;
            }
            continue;
        }

        if (arg == "--print-every" && i + 1 < argc) {
            if (!parseIntArg(argv[++i], options.printEveryFrames) || options.printEveryFrames <= 0) {
                std::cerr << "Invalid print interval\n";
                return false;
            }
            continue;
        }

        if (arg == "--threshold-min" && i + 1 < argc) {
            if (!parseFloatArg(argv[++i], options.thresholdMin)) {
                std::cerr << "Invalid threshold min\n";
                return false;
            }
            continue;
        }

        if (arg == "--threshold-max" && i + 1 < argc) {
            if (!parseFloatArg(argv[++i], options.thresholdMax)) {
                std::cerr << "Invalid threshold max\n";
                return false;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        return false;
    }

    if (options.thresholdMax <= options.thresholdMin) {
        std::cerr << "threshold-max must be greater than threshold-min\n";
        return false;
    }

    return true;
}

freenect_depth_format toFreenectDepthFormat(depthFormatEnum format) {
    return format == depthFormatEnum::Registered
        ? FREENECT_DEPTH_REGISTERED
        : FREENECT_DEPTH_MM;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        return 1;
    }

    std::cout << "Freenect V1 depth tester\n";
    std::cout << "mode=" << toString(options.mode)
              << " format=" << toString(options.format)
              << " duration=" << options.durationSeconds << "s"
              << " toggleEvery=" << options.toggleEverySeconds << "s"
              << " thresholds=[" << options.thresholdMin << ", " << options.thresholdMax << "]\n";

    freenect_context* ctx = nullptr;
    std::unique_ptr<MyFreenectDevice> device;
    std::atomic<bool> rgbReady{false};
    std::atomic<bool> depthReady{false};
    std::atomic<bool> runEvents{false};
    std::thread eventThread;

    try {
        if (freenect_init(&ctx, nullptr) < 0) {
            std::cerr << "freenect_init failed\n";
            return 2;
        }

        freenect_set_log_level(ctx, FREENECT_LOG_WARNING);
        freenect_select_subdevices(ctx, static_cast<freenect_device_flags>(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

        freenect_set_fw_address_nui(ctx, ofxKinectExtras::getFWData1473(), ofxKinectExtras::getFWSize1473());
        freenect_set_fw_address_k4w(ctx, ofxKinectExtras::getFWDatak4w(), ofxKinectExtras::getFWSizek4w());

        const int numDevices = freenect_num_devices(ctx);
        if (numDevices <= 0) {
            std::cerr << "No Kinect v1 devices found\n";
            freenect_shutdown(ctx);
            return 3;
        }

        device = std::make_unique<MyFreenectDevice>(ctx, 0, rgbReady, depthReady);
        device->setResolutions(
            MyFreenectDevice::WIDTH,
            MyFreenectDevice::HEIGHT,
            MyFreenectDevice::WIDTH,
            MyFreenectDevice::HEIGHT,
            MyFreenectDevice::WIDTH,
            MyFreenectDevice::HEIGHT
        );

        device->startVideo();
        device->startDepth();
        device->setDepthFormat(toFreenectDepthFormat(options.format));

        runEvents = true;
        eventThread = std::thread([&]() {
            timeval timeout = {0, 10000};
            while (runEvents.load()) {
                const int err = freenect_process_events_timeout(ctx, &timeout);
                if (err < 0) {
                    std::cerr << "freenect_process_events_timeout failed with code " << err << '\n';
                    break;
                }
            }
        });

        const auto startTime = std::chrono::steady_clock::now();
        auto nextToggleTime = startTime + std::chrono::seconds(options.toggleEverySeconds > 0 ? options.toggleEverySeconds : options.durationSeconds + 1);
        auto lastFrameTime = startTime;
        depthFormatEnum currentFormat = options.format;

        int framesReceived = 0;
        int misses = 0;
        int suspiciousFrames = 0;

        while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(options.durationSeconds)) {
            bool gotFrame = false;
            std::vector<uint16_t> frame;

            if (options.mode == TestMode::RawCallback) {
                if (device->getDepth(frame)) {
                    gotFrame = true;
                }
            } else {
                if (device->getDepthFrame(frame, currentFormat, options.thresholdMin, options.thresholdMax)) {
                    gotFrame = true;
                }
            }

            if (gotFrame) {
                framesReceived++;
                misses = 0;
                lastFrameTime = std::chrono::steady_clock::now();

                const FrameStats stats = analyzeFrame(frame);
                if (stats.nonZero < 1000) {
                    suspiciousFrames++;
                }

                if (framesReceived == 1 || framesReceived % options.printEveryFrames == 0) {
                    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(lastFrameTime - startTime).count();
                    std::cout << "[frame " << framesReceived << "]"
                              << " t=" << elapsedMs << "ms"
                              << " format=" << toString(currentFormat)
                              << " nonZero=" << stats.nonZero << "/" << stats.total
                              << " coverage=" << formatCoverage(stats) << "%"
                              << " min=" << stats.min
                              << " max=" << stats.max
                              << '\n';
                }
            } else {
                misses++;
                const auto noFrameForMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - lastFrameTime
                ).count();
                if (misses % 50 == 0) {
                    std::cout << "[wait] no new frame for " << noFrameForMs << "ms"
                              << " format=" << toString(currentFormat) << '\n';
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            if (options.toggleEverySeconds > 0 && std::chrono::steady_clock::now() >= nextToggleTime) {
                currentFormat = currentFormat == depthFormatEnum::Raw
                    ? depthFormatEnum::Registered
                    : depthFormatEnum::Raw;

                std::cout << "[toggle] switching to " << toString(currentFormat) << '\n';

                if (options.mode == TestMode::RawCallback) {
                    device->setDepthFormat(toFreenectDepthFormat(currentFormat));
                }

                nextToggleTime += std::chrono::seconds(options.toggleEverySeconds);
            }
        }

        std::cout << "Summary: framesReceived=" << framesReceived
                  << " suspiciousFrames=" << suspiciousFrames
                  << " finalFormat=" << toString(currentFormat)
                  << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << '\n';
        runEvents = false;
        if (eventThread.joinable()) {
            eventThread.join();
        }
        device.reset();
        if (ctx) {
            freenect_shutdown(ctx);
        }
        return 4;
    }

    runEvents = false;
    if (eventThread.joinable()) {
        eventThread.join();
    }
    device.reset();
    if (ctx) {
        freenect_shutdown(ctx);
    }

    return 0;
}
