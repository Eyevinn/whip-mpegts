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
          srtMode_(2),
          tsDemuxLatency_(0),
          jitterBufferLatency_(0),
          srtSourceLatency_(125),
          h264encodeBitrate(2000),
          audio_(true),
          video_(true),
          bypass_audio_(false),
          bypass_video_(false)
    {
    }

    std::string toString()
    {
        std::string result;
        result.append("whipEndpointUrl: ");
        result.append(whipEndpointUrl_);
        result.append("\n");
        result.append("whipEndpointAuthKey: ");
        result.append(whipEndpointAuthKey_.empty() ? "unset" : "set");
        result.append("\n");
        result.append("udpSourceAddress: ");
        result.append(udpSourceAddress_);
        result.append("\n");
        result.append("udpSourcePort: ");
        result.append(std::to_string(udpSourcePort_));
        result.append("\n");
        result.append("udpSourceQueueMinTime: ");
        result.append(std::to_string(udpSourceQueueMinTime_.count()));
        result.append("\n");
        result.append("restreamAddress: ");
        result.append(restreamAddress_.empty() ? "unset" : restreamAddress_);
        result.append("\n");
        result.append("restreamPort: ");
        result.append(std::to_string(restreamPort_));
        result.append("\n");
        result.append("h264encodeBitrate: ");
        result.append(std::to_string(h264encodeBitrate));
        result.append("\n");
        result.append("showTimer: ");
        result.append(showTimer_ ? "true" : "false");
        result.append("\n");
        result.append("srtTransport: ");
        result.append(srtTransport_ ? "true" : "false");
        result.append("\n");
        result.append("srtMode: ");
        result.append(std::to_string(srtMode_));
        result.append(" (1=caller, 2=listener)\n");
        result.append("tsDemuxLatency: ");
        result.append(std::to_string(tsDemuxLatency_));
        result.append("\n");
        result.append("jitterBufferLatency: ");
        result.append(std::to_string(jitterBufferLatency_));
        result.append("\n");
        result.append("srtSourceLatency: ");
        result.append(std::to_string(srtSourceLatency_));
        result.append("\n");
        result.append("audio: ");
        result.append(audio_ ? "true" : "false");
        result.append("\n");
        result.append("video: ");
        result.append(video_ ? "true" : "false");
        result.append("\n");
        result.append("bypass audio: ");
        result.append(bypass_audio_ ? "true" : "false");
        result.append("\n");
        result.append("bypass video: ");
        result.append(bypass_video_ ? "true" : "false");

        return result;
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
    uint32_t srtMode_;

    uint32_t tsDemuxLatency_;
    uint32_t jitterBufferLatency_;
    uint32_t srtSourceLatency_;
    uint32_t h264encodeBitrate;

    bool audio_;
    bool video_;

    bool bypass_audio_;
    bool bypass_video_;
};
