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

::option longOptions[] = {{"udpSourceAddress", required_argument, nullptr, 'a'},
    {"udpSourcePort", required_argument, nullptr, 'p'},
    {"whipEndpointUrl", required_argument, nullptr, 'u'},
    {"whipEndpointAuthKey", required_argument, nullptr, 'k'},
    {"udpSourceQueueMinTime", required_argument, nullptr, 'd'},
    {"restreamAddress", required_argument, nullptr, 'r'},
    {"restreamPort", required_argument, nullptr, 'o'},
    {"showTimer", no_argument, nullptr, 't'},
    {"srtTransport", no_argument, nullptr, 's'},
    {"tsDemuxLatency", required_argument, nullptr, 0},
    {"jitterBufferLatency", required_argument, nullptr, 0},
    {"srtSourceLatency", required_argument, nullptr, 0},
    {"h264EncodeBitrate", required_argument, nullptr, 'b'},
    {"no-audio", no_argument, nullptr, 0},
    {"no-video", no_argument, nullptr, 0},
    {"bypass-audio", no_argument, nullptr, 0},
    {"bypass-video", no_argument, nullptr, 0},
    {nullptr, no_argument, nullptr, 0}};

const auto shortOptions = "a:p:u:k:d:r:o:b:ts";

const char* usageString = "Usage: whip-mpegts [OPTION]\n"
                          "  -a, --udpSourceAddress STRING\n"
                          "  -p, --udpSourcePort INT\n"
                          "  -u, --whipEndpointUrl STRING\n"
                          "  -k, --whipEndpointAuthKey STRING\n"
                          "  -d, --udpSourceQueueMinTime INT ms\n"
                          "  -r, --restreamAddress STRING\n"
                          "  -o, --restreamPort INT\n"
                          "  -b, --h264EncodeBitrate INT (Kb)\n"
                          "  -t, --showTimer\n"
                          "  -s, --srtTransport\n"
                          "  --tsDemuxLatency INT\n"
                          "  --jitterBufferLatency INT\n"
                          "  --srtSourceLatency INT\n"
                          "  --no-audio\n"
                          "  --no-video\n"
                          "  --bypass-audio\n"
                          "  --bypass-video\n";

GMainLoop* mainLoop = nullptr;
std::unique_ptr<Pipeline> pipeline;

void intSignalHandler(int32_t)
{
    if (pipeline)
    {
        pipeline->stop();
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
    int32_t optIndex;

    while ((getOptResult = getopt_long(argc, argv, shortOptions, longOptions, &optIndex)) != -1)
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
            config.h264encodeBitrate = std::strtoul(optarg, nullptr, 10);
            break;
        case 't':
            config.showTimer_ = true;
            break;
        case 's':
            config.srtTransport_ = true;
            break;
        case 0:
            break;
        default:
            break;
        }

        switch (optIndex)
        {
        case 9:
            config.tsDemuxLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case 10:
            config.jitterBufferLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case 11:
            config.srtSourceLatency_ = std::strtoul(optarg, nullptr, 10);
            break;
        case 12:
            config.h264encodeBitrate = std::strtoul(optarg, nullptr, 10);
            break;
        case 13:
            config.audio_ = false;
            break;
        case 14:
            config.video_ = false;
            break;
        case 15:
            config.bypass_audio_ = true;
            break;
        case 16:
            config.bypass_video_ = true;
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
    Logger::log("Config:\n%s", config.toString().c_str());

    mainLoop = g_main_loop_new(nullptr, FALSE);

    http::WhipClient whipClient(config.whipEndpointUrl_, config.whipEndpointAuthKey_);
    pipeline = std::make_unique<Pipeline>(whipClient, config);
    pipeline->run();

    g_main_loop_run(mainLoop);

    return 0;
}
