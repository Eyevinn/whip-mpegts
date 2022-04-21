#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <memory>

namespace http
{
class WhipClient;
}

class Pipeline
{
public:
    explicit Pipeline(http::WhipClient& whipClient, std::string&& mpegTsAddress, const uint32_t mpegTsPort);
    ~Pipeline();

    void run();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
