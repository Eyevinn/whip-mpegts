#include "http/WhipClient.h"
#include "Pipeline.h"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <glib-2.0/glib.h>
#include <unistd.h>

namespace
{

const char* usageString = "Usage: mpeg-ts-client -a [MPEG-TS address] -p <MPEG-TS port> -u <WHIP endpoint URL> -k "
                          "[WHIP auth key] -d [MPEG-TS buffer size ms]";

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

    std::string url;
    std::string authKey;
    std::string mpegTsAddress = "0.0.0.0";
    uint32_t mpegTsPort = 0;
    int32_t getOptResult;
    std::chrono::milliseconds mpegTsBufferSize(1000);

    while ((getOptResult = getopt(argc, argv, "a:p:u:k:d:")) != -1)
    {
        switch (getOptResult)
        {
        case 'a':
            mpegTsAddress = optarg;
            break;
        case 'p':
            mpegTsPort = std::strtoul(optarg, nullptr, 10);
            break;
        case 'u':
            url = optarg;
            break;
        case 'k':
            authKey = optarg;
            break;
        case 'd':
            mpegTsBufferSize = std::chrono::milliseconds(std::strtoull(optarg, nullptr, 10));
            break;
        default:
            printf("%s\n", usageString);
            return 1;
        }
    }

    if (mpegTsAddress.empty() || mpegTsPort == 0 || url.empty())
    {
        printf("%s\n", usageString);
        return 1;
    }

    mainLoop = g_main_loop_new(nullptr, FALSE);

    http::WhipClient whipClient(std::move(url), std::move(authKey));
    pipeline = std::make_unique<Pipeline>(whipClient, std::move(mpegTsAddress), mpegTsPort, mpegTsBufferSize);
    pipeline->run();

    g_main_loop_run(mainLoop);

    return 0;
}
