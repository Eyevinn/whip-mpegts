#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace http
{
class WhipClient;
}

class Pipeline
{
public:
    Pipeline(http::WhipClient& whipClient,
        std::string&& mpegTsAddress,
        const uint32_t mpegTsPort,
        const std::chrono::milliseconds mpegTsBufferSize,
        std::string&& restreamAddress,
        const uint32_t restreamPort,
        const bool showWindow,
        const bool showTimer,
        const bool srtTransport);

    Pipeline(http::WhipClient& whipClient, std::string&& fileName, const bool showWindow, const bool showTimer);

    ~Pipeline();

    void run();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
