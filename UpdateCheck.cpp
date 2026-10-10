//
//  UpdateCheck.cpp
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#include "UpdateCheck.h"
#include "logger.h"
#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

// "v1.2.3" or "1.2.3" -> {1, 2, 3}; missing parts are 0
static std::array<int, 3> parseVersion(const std::string& version) {
    std::array<int, 3> parts{0, 0, 0};
    const char* text = version.c_str();
    if (*text == 'v') ++text;
    std::sscanf(text, "%d.%d.%d", &parts[0], &parts[1], &parts[2]);
    return parts;
}

// Asks GitHub for the latest release tag. Runs macOS's own curl, so the plugin needs no HTTP library.
static std::string fetchLatestReleaseTag(std::string& error) {
    FILE* pipe = popen("/usr/bin/curl -fsS --max-time 5 https://api.github.com/repos/stosumarte/FreenectTD/releases/latest 2>&1", "r");
    if (!pipe) {
        error = "couldn't run curl";
        return "";
    }
    std::string reply;
    char buffer[4096];
    while (size_t n = std::fread(buffer, 1, sizeof(buffer), pipe)) {
        reply.append(buffer, n);
    }
    if (pclose(pipe) != 0) {
        error = reply.substr(0, reply.find('\n')); // curl's error message
        if (error.empty()) error = "curl failed";
        return "";
    }
    const size_t key = reply.find("\"tag_name\"");
    const size_t start = key == std::string::npos ? key : reply.find('"', reply.find(':', key) + 1);
    const size_t end = start == std::string::npos ? start : reply.find('"', start + 1);
    if (end == std::string::npos) {
        error = "unexpected reply from GitHub";
        return "";
    }
    return reply.substr(start + 1, end - start - 1);
}

// Shows a macOS alert and waits for it to be closed. Returns true if the first button was pressed.
static bool showAlert(const std::string& header, const std::string& message,
                      const char* defaultButton, const char* otherButton = nullptr) {
    auto cf = [](const char* text) {
        return text ? CFStringCreateWithCString(nullptr, text, kCFStringEncodingUTF8) : nullptr;
    };
    CFStringRef cfHeader = cf(header.c_str());
    CFStringRef cfMessage = cf(message.c_str());
    CFStringRef cfDefault = cf(defaultButton);
    CFStringRef cfOther = cf(otherButton);
    CFOptionFlags response = 0;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, nullptr, nullptr, nullptr,
                                   cfHeader, cfMessage, cfDefault, cfOther, nullptr, &response);
    for (CFStringRef s : {cfHeader, cfMessage, cfDefault, cfOther}) {
        if (s) CFRelease(s);
    }
    return (response & 0x3) == kCFUserNotificationDefaultResponse;
}

void openReleasesPage() {
    std::system("open https://github.com/stosumarte/FreenectTD/releases/latest");
}

// NOTE: the thread is detached and waits for the dialog to be closed; if the plugin were unloaded
// while a dialog is open, the thread would return into unloaded code.
void checkForUpdates(const char* installedVersion) {
    static std::atomic<bool> running{false};
    if (running.exchange(true)) {
        return;
    }
    std::thread([installed = std::string(installedVersion)] {
        std::string error;
        const std::string latest = fetchLatestReleaseTag(error);
        LOG("[UpdateCheck] latest release: " + (latest.empty() ? "(failed: " + error + ")" : latest));
        if (latest.empty()) {
            showAlert("Couldn't check for FreenectTD updates", error, "OK");
        } else if (parseVersion(latest) > parseVersion(installed)) {
            if (showAlert("FreenectTD " + latest + " is available",
                          "You have v" + installed + ".", "Open Releases Page", "Later")) {
                openReleasesPage();
            }
        } else {
            showAlert("FreenectTD is up to date",
                      "You have v" + installed + ". The latest release is " + latest + ".", "OK");
        }
        running = false;
    }).detach();
}
