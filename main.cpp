#include "Config.h"
#include "http/WhipClient.h"
#include "Logger.h"
#include "Pipeline.h"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <getopt.h>
#include <glib-2.0/glib.h>

namespace
{

enum : int
{
    OPT_TS_DEMUX_LATENCY = 256,
    OPT_JITTER_BUFFER_LATENCY,
    OPT_SRT_SOURCE_LATENCY,
    OPT_NO_AUDIO,
    OPT_NO_VIDEO,
    OPT_BYPASS_AUDIO,
    OPT_BYPASS_VIDEO,
    OPT_IGNORE_PCR,
    OPT_VP8,
    OPT_H264_PACKETIZATION_MODE,
    OPT_H264_PROFILE,
};

::option longOptions[] = {{"udpSourceAddress", required_argument, nullptr, 'a'},
    {"udpSourcePort", required_argument, nullptr, 'p'},
    {"whipEndpointUrl", required_argument, nullptr, 'u'},
    {"whipEndpointAuthKey", required_argument, nullptr, 'k'},
    {"udpSourceQueueMinTime", required_argument, nullptr, 'd'},
    {"restreamAddress", required_argument, nullptr, 'r'},
    {"restreamPort", required_argument, nullptr, 'o'},
    {"showTimer", no_argument, nullptr, 't'},
    {"srtTransport", no_argument, nullptr, 's'},
    {"srtMode", required_argument, nullptr, 'm'},
    {"h264EncodeBitrate", required_argument, nullptr, 'b'},
    {"tsDemuxLatency", required_argument, nullptr, OPT_TS_DEMUX_LATENCY},
    {"jitterBufferLatency", required_argument, nullptr, OPT_JITTER_BUFFER_LATENCY},
    {"srtSourceLatency", required_argument, nullptr, OPT_SRT_SOURCE_LATENCY},
    {"no-audio", no_argument, nullptr, OPT_NO_AUDIO},
    {"no-video", no_argument, nullptr, OPT_NO_VIDEO},
    {"bypass-audio", no_argument, nullptr, OPT_BYPASS_AUDIO},
    {"bypass-video", no_argument, nullptr, OPT_BYPASS_VIDEO},
    {"ignore-pcr", no_argument, nullptr, OPT_IGNORE_PCR},
    {"vp8", no_argument, nullptr, OPT_VP8},
    {"h264PacketizationMode", required_argument, nullptr, OPT_H264_PACKETIZATION_MODE},
    {"h264Profile", required_argument, nullptr, OPT_H264_PROFILE},
    {nullptr, no_argument, nullptr, 0}};

const auto shortOptions = "a:p:u:k:d:r:o:b:m:ts";

const char* usageString = "Usage: whip-mpegts [OPTION]\n"
                          "  -a, --udpSourceAddress STRING\n"
                          "  -p, --udpSourcePort INT\n"
                          "  -u, --whipEndpointUrl STRING\n"
                          "  -k, --whipEndpointAuthKey STRING\n"
                          "  -d, --udpSourceQueueMinTime INT ms\n"
                          "  -r, --restreamAddress STRING\n"
                          "  -o, --restreamPort INT\n"
                          "  -b, --h264EncodeBitrate INT Kb (video encode bitrate, applies to H264 and VP8)\n"
                          "  -t, --showTimer\n"
                          "  -s, --srtTransport\n"
                          "  -m, --srtMode INT (1=caller, 2=listener, default=2)\n"
                          "  --tsDemuxLatency INT\n"
                          "  --jitterBufferLatency INT\n"
                          "  --srtSourceLatency INT\n"
                          "  --no-audio\n"
                          "  --no-video\n"
                          "  --bypass-audio\n"
                          "  --bypass-video\n"
                          "  --ignore-pcr (can also use IGNORE_PCR env var)\n"
                          "  --vp8 (encode video as VP8 instead of H264)\n"
                          "  --h264PacketizationMode INT (H264 RTP packetization-mode, 0 or 1, default=1)\n"
                          "  --h264Profile STRING (H264 encoder profile, default=constrained-baseline)\n";

GMainLoop* mainLoop = nullptr;
std::unique_ptr<Pipeline> pipeline;
std::unique_ptr<http::WhipClient> whipClient;

void intSignalHandler(int32_t)
{
    Logger::log("Received SIGINT, shutting down gracefully...");

    if (pipeline)
    {
        const auto& whipResource = pipeline->getWhipResource();

        // Stop the pipeline first
        pipeline->stop();

        // Delete the WHIP session
        if (whipClient && !whipResource.empty())
        {
            Logger::log("Deleting WHIP session: %s", whipResource.c_str());
            whipClient->deleteSession(whipResource);
        }
    }

    g_main_loop_quit(mainLoop);
}

} // namespace

int32_t main(int32_t argc, char** argv)
{
    {
        struct sigaction sigactionData = {};
        sigactionData.sa_handler = intSignalHandler;
        sigactionData.sa_flags = 0;
        sigemptyset(&sigactionData.sa_mask);
        sigaction(SIGINT, &sigactionData, nullptr);
    }

    Config config;
    int32_t getOptResult;

    while ((getOptResult = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1)
    {
        switch (getOptResult)
        {
        case 'a':
            config.udpSourceAddress_ = optarg;
            break;
        case 'p':
            config.udpSourcePort_ = std::strtoul(optarg, nullptr, 10);
            break;
        case 'u':
            config.whipEndpointUrl_ = optarg;
            break;
        case 'k':
            config.whipEndpointAuthKey_ = optarg;
            break;
        case 'd':
            config.udpSourceQueueMinTime_ = std::chrono::milliseconds(std::strtoull(optarg, nullptr, 10));
            break;
        case 'r':
            config.restreamAddress_ = optarg;
            break;
        case 'o':
            config.restreamPort_ = std::strtoul(optarg, nullptr, 10);
            break;
        case 'b':
            config.videoEncodeBitrate = std::strtoul(optarg, nullptr, 10);
            break;
        case 't':
            config.showTimer_ = true;
            break;
        case 's':
            config.srtTransport_ = true;
            break;
        case 'm':
            config.srtMode_ = std::strtoul(optarg, nullptr, 10);
            break;
        case OPT_TS_DEMUX_LATENCY:
            config.tsDemuxLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case OPT_JITTER_BUFFER_LATENCY:
            config.jitterBufferLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case OPT_SRT_SOURCE_LATENCY:
            config.srtSourceLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case OPT_NO_AUDIO:
            config.audio_ = false;
            break;
        case OPT_NO_VIDEO:
            config.video_ = false;
            break;
        case OPT_BYPASS_AUDIO:
            config.bypass_audio_ = true;
            break;
        case OPT_BYPASS_VIDEO:
            config.bypass_video_ = true;
            break;
        case OPT_IGNORE_PCR:
            config.ignorePcr_ = true;
            break;
        case OPT_VP8:
            config.vp8_ = true;
            break;
        case OPT_H264_PACKETIZATION_MODE:
            config.h264PacketizationMode_ = std::strtoul(optarg, nullptr, 10);
            break;
        case OPT_H264_PROFILE:
            config.h264Profile_ = optarg;
            break;
        default:
            break;
        }
    }

    if (config.whipEndpointUrl_.empty() || config.udpSourcePort_ == 0 ||
        (!config.restreamAddress_.empty() && config.restreamPort_ == 0))
    {
        printf("%s\n", usageString);
        return 1;
    }

    if (config.bypass_video_ && config.vp8_)
    {
        fprintf(stderr, "Error: --bypass-video and --vp8 cannot be used together\n");
        return 1;
    }

    if (config.srtMode_ != 1 && config.srtMode_ != 2)
    {
        fprintf(stderr, "Error: --srtMode must be 1 (caller) or 2 (listener)\n");
        return 1;
    }
    Logger::log("Config:\n%s", config.toString().c_str());

    mainLoop = g_main_loop_new(nullptr, FALSE);

    whipClient = std::make_unique<http::WhipClient>(config.whipEndpointUrl_, config.whipEndpointAuthKey_);
    pipeline = std::make_unique<Pipeline>(*whipClient, config);
    pipeline->run();

    g_main_loop_run(mainLoop);

    // Clean up
    pipeline.reset();
    whipClient.reset();

    return 0;
}
