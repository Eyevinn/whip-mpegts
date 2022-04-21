#pragma once

#include <memory>
#include <string>
#include <vector>

namespace http
{

class WhipClient
{
public:
    struct SendOfferResult
    {
        std::string resource_;
        std::vector<std::string> extensions_;
        std::string sdpAnswer_;
    };

    WhipClient(std::string&& url, std::string&& authKey);
    ~WhipClient();
    SendOfferResult sendOffer(const std::string& sdp);
    bool updateIce(const std::string& resourceUrl, std::string&& sdp);

private:
    struct OpaqueSoupData;

    std::unique_ptr<OpaqueSoupData> data_;
    std::string url_;
    std::string authKey_;
};

} // namespace http
