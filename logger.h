//
//  logger.h
//  FreenectTD
//
//  Created by marte on 29/08/2025.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <fstream>

inline std::string currentTimeMillis() {
    auto now = std::chrono::system_clock::now();
    auto ms = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S") << '-' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

namespace {
    static std::mutex logMutex;
    static std::ofstream logFile("/tmp/FreenectTOP_" + currentTimeMillis() + ".log");
}

#if FNTD_DEBUG == 1
#define LOG(msg) { \
    std::lock_guard<std::mutex> lock(logMutex); std::cout << "[FNTD_DEBUG] " << msg << std::endl; \
}

#elif FNTD_DEBUG == 2
#define LOG(msg) { \
    std::lock_guard<std::mutex> lock(logMutex); std::cout << "[FNTD_DEBUG] " << msg << std::endl; \
    if (logFile.is_open()) { logFile << "[" << currentTimeMillis() << "] " << msg << std::endl; } \
}

#else
#define LOG(msg)
#endif

#if FNTD_PROFILE == 1
#define PROFILE(msg) { std::lock_guard<std::mutex> lock(logMutex); std::cout << "[" << currentTimeMillis() << "][PROFILE] " << msg << std::endl; }
#else
#define PROFILE(msg)
#endif

#endif
