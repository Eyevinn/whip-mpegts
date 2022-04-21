#include "http/WhipClient.h"
#include "Logger.h"
#include "Pipeline.h"
#include <csignal>
#include <cstdint>
#include <glib-2.0/glib.h>

namespace
{

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
    std::string mpegTsAddress;
    uint32_t mpegTsPort = 0;

    if (argc < 4)
    {
        Logger::log("Usage: mpeg-ts-client <MPEG-TS address> <MPEG-TS port> <WHIP endpoint URL> [WHIP auth key]");
        return -1;
    }
    else if (argc == 4)
    {
        mpegTsAddress = argv[1];
        mpegTsPort = std::strtoul(argv[2], nullptr, 10);
        url = argv[3];
    }
    else if (argc == 5)
    {
        mpegTsAddress = argv[1];
        mpegTsPort = std::strtoul(argv[2], nullptr, 10);
        url = argv[3];
        authKey = argv[4];
    }

    mainLoop = g_main_loop_new(nullptr, FALSE);

    http::WhipClient whipClient(std::move(url), std::move(authKey));
    pipeline = std::make_unique<Pipeline>(whipClient, std::move(mpegTsAddress), mpegTsPort);
    pipeline->run();

    g_main_loop_run(mainLoop);

    return 0;
}
