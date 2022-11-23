#include "http/WhipClient.h"
#include "Pipeline.h"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <glib-2.0/glib.h>
#include <unistd.h>

namespace
{

const char* usageString =
    "Usage: whip-mpegts -a [MPEG-TS address] -p [MPEG-TS port] -f [mp4 input file] -u <WHIP endpoint URL> -k "
    "[WHIP auth key] -d [MPEG-TS buffer size ms] "
    "-r [restream address] -o [restream port] "
    "[-t] [-w] [-s]";

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
    std::string restreamAddress = "";
    uint32_t mpegTsPort = 0;
    uint32_t restreamPort = 0;
    int32_t getOptResult;
    std::chrono::milliseconds mpegTsBufferSize(1000);
    std::string fileName;
    auto showTimer = false;
    auto showWindow = false;
    auto srtTransport = false;

    while ((getOptResult = getopt(argc, argv, "a:p:u:k:d:f:r:o:tws")) != -1)
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
        case 'f':
            fileName = optarg;
            break;
        case 'r':
            restreamAddress = optarg;
            break;
        case 'o':
            restreamPort = std::strtoul(optarg, nullptr, 10);
            break;
        case 't':
            showTimer = true;
            break;
        case 'w':
            showWindow = true;
            break;
        case 's':
            srtTransport = true;
            break;
        default:
            printf("%s\n", usageString);
            return 1;
        }
    }

    if (url.empty() || ((mpegTsAddress.empty() || mpegTsPort == 0) && fileName.empty()))
    {
        printf("%s\n", usageString);
        return 1;
    }
    if (!restreamAddress.empty() && restreamPort == 0)
    {
        printf("Missing restream port.\n%s\n", usageString);
        return 1;
    }

    mainLoop = g_main_loop_new(nullptr, FALSE);

    http::WhipClient whipClient(std::move(url), std::move(authKey));
    if (!fileName.empty())
    {
        pipeline = std::make_unique<Pipeline>(whipClient, std::move(fileName), showWindow, showTimer);
    }
    else
    {
        pipeline = std::make_unique<Pipeline>(whipClient,
            std::move(mpegTsAddress),
            mpegTsPort,
            mpegTsBufferSize,
            std::move(restreamAddress),
            restreamPort,
            showWindow,
            showTimer,
            srtTransport);
    }
    pipeline->run();

    g_main_loop_run(mainLoop);

    return 0;
}
