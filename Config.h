#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

struct Config
{
    Config()
        : whipEndpointUrl_(),
          whipEndpointAuthKey_(),
          udpSourceAddress_("0.0.0.0"),
          udpSourcePort_(0),
          udpSourceQueueMinTime_(0),
          restreamAddress_(),
          restreamPort_(0),
          showTimer_(false),
          srtTransport_(false),
          tsDemuxLatency_(0),
          jitterBufferLatency_(0),
          srtSourceLatency_(125)
    {
    }

    std::string toString()
    {
        std::array<char, 2048> result{};
        snprintf(result.data(),
            result.size(),
            "whipEndpointUrl: %s\n"
            "whipEndpointAuthKey: %s,\n"
            "udpSourceAddress: %s,\n"
            "udpSourcePort: %u,\n"
            "udpSourceQueueMinTime: %lu,\n"
            "restreamAddress: %s,\n"
            "restreamPort: %u,\n"
            "showTimer: %c,\n"
            "srtTransport %c,\n"
            "tsDemuxLatency: %u,\n"
            "jitterBufferLatency %u,\n"
            "srtSourceLatency %u\n",
            whipEndpointUrl_.c_str(),
            whipEndpointAuthKey_.empty() ? "unset" : "set",
            udpSourceAddress_.c_str(),
            udpSourcePort_,
            udpSourceQueueMinTime_.count(),
            restreamAddress_.empty() ? "unset" : restreamAddress_.c_str(),
            restreamPort_,
            showTimer_ ? 't' : 'f',
            srtTransport_ ? 't' : 'f',
            tsDemuxLatency_,
            jitterBufferLatency_,
            srtSourceLatency_);

        return result.data();
    }

    std::string whipEndpointUrl_;
    std::string whipEndpointAuthKey_;
    std::string udpSourceAddress_;
    uint32_t udpSourcePort_;
    std::chrono::milliseconds udpSourceQueueMinTime_;
    std::string restreamAddress_;
    uint32_t restreamPort_;
    bool showTimer_;
    bool srtTransport_;

    uint32_t tsDemuxLatency_;
    uint32_t jitterBufferLatency_;
    uint32_t srtSourceLatency_;
};
