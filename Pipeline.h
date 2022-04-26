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
    explicit Pipeline(http::WhipClient& whipClient,
        std::string&& mpegTsAddress,
        const uint32_t mpegTsPort,
        const std::chrono::milliseconds mpegTsBufferSize);
    ~Pipeline();

    void run();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
