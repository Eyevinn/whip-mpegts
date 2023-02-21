#pragma once

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
        std::string etag_;
        std::vector<std::string> extensions_;
        std::string sdpAnswer_;
    };

    WhipClient(const std::string& url, const std::string& authKey);
    ~WhipClient();
    SendOfferResult sendOffer(const std::string& sdp);
    bool updateIce(const std::string& resourceUrl, const std::string& etag, std::string&& sdp);

private:
    struct OpaqueSoupData;

    OpaqueSoupData* data_;
    std::string url_;
    std::string authKey_;
};

} // namespace http
