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

namespace {
    static std::mutex logMutex;
}

inline std::string currentTimeMillis() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

#if FNTD_DEBUG == 1
#define LOG(msg) { std::lock_guard<std::mutex> lock(logMutex); std::cout << "[FNTD_DEBUG] " << msg << std::endl; }
#else
#define LOG(msg)
#endif

#if FNTD_PROFILE == 1
#define PROFILE(msg) { std::lock_guard<std::mutex> lock(logMutex); std::cout << "[" << currentTimeMillis() << "][PROFILE] " << msg << std::endl; }
#else
#define PROFILE(msg)
#endif

#endif
