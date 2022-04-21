#include "Logger.h"

#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <array>

namespace Logger
{

void log(const char* format, ...)
{
    const auto now = std::chrono::system_clock::now();
    const auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    tm localTime = {};
    localtime_r(&nowTimeT, &localTime);

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::array<char, 2048> logString{};
    std::array<char, 64> timeString{};

    snprintf(timeString.data(),
        timeString.size(),
        "%04d-%02d-%02d %02d:%02d:%02d.%03u",
        static_cast<uint16_t>(localTime.tm_year + 1900),
        static_cast<uint8_t>(localTime.tm_mon + 1),
        static_cast<uint8_t>(localTime.tm_mday),
        static_cast<uint8_t>(localTime.tm_hour),
        static_cast<uint8_t>(localTime.tm_min),
        static_cast<uint8_t>(localTime.tm_sec),
        static_cast<uint32_t>(ms % 1000));

    va_list args;
    va_start(args, format);

    vsnprintf(logString.data(), logString.size(), format, args);
    fprintf(stdout, "[%s] %s\n", timeString.data(), logString.data());
}

}
