#include "http/WhipClient.h"
#include <cassert>
#include <libsoup/soup.h>
#include <unordered_map>

namespace
{

void iterateResponseHeaders(const char* name, const char* value, gpointer userData)
{
    auto headers = reinterpret_cast<std::unordered_map<std::string, std::string>*>(userData);
    auto key = std::string(name);
    std::transform(key.cbegin(), key.cend(), key.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    headers->emplace(key, value);
}

} // namespace

namespace http
{

struct WhipClient::OpaqueSoupData
{
    OpaqueSoupData() : soupSession_(nullptr) {}

    SoupSession* soupSession_;
};

WhipClient::WhipClient(const std::string& url, const std::string& authKey)
    : data_(new OpaqueSoupData()),
      url_(url),
      authKey_(authKey)
{
    data_->soupSession_ =
        soup_session_new_with_options(SOUP_SESSION_TIMEOUT, 5, SOUP_SESSION_SSL_STRICT, FALSE, nullptr);
    if (!data_->soupSession_)
    {
        assert(false);
        return;
    }
}

WhipClient::~WhipClient()
{
    if (data_)
    {
        delete data_;
    }
}

WhipClient::SendOfferResult WhipClient::sendOffer(const std::string& sdp)
{
    auto soupMessage = soup_message_new("POST", url_.c_str());
    if (!soupMessage)
    {
        return {};
    }

    soup_message_set_request(soupMessage, "application/sdp", SOUP_MEMORY_COPY, sdp.c_str(), sdp.size());
    if (!authKey_.empty())
    {
        soup_message_headers_append(soupMessage->request_headers, "Authorization", authKey_.c_str());
    }

    auto statusCode = soup_session_send_message(data_->soupSession_, soupMessage);
    if (statusCode != 201)
    {
        return {};
    }

    std::unordered_map<std::string, std::string> headers;
    soup_message_headers_foreach(soupMessage->response_headers, iterateResponseHeaders, &headers);
    const auto locationItr = headers.find("location");
    if (locationItr == headers.cend())
    {
        return {};
    }

    SendOfferResult result;
    result.resource_ = locationItr->second;
    result.sdpAnswer_ = soupMessage->response_body->data;

    const auto etagItr = headers.find("etag");
    if (etagItr != headers.cend())
    {
        result.etag_ = etagItr->second;
    }

    return result;
}

bool WhipClient::updateIce(const std::string& resourceUrl, const std::string& etag, std::string&& sdp)
{
    auto soupMessage = soup_message_new("PATCH", resourceUrl.c_str());
    if (!soupMessage)
    {
        return false;
    }

    soup_message_set_request(soupMessage, "application/trickle-ice-sdpfrag", SOUP_MEMORY_COPY, sdp.c_str(), sdp.size());
    if (!authKey_.empty())
    {
        soup_message_headers_append(soupMessage->request_headers, "Authorization", authKey_.c_str());
    }
    if (!etag.empty())
    {
        soup_message_headers_append(soupMessage->request_headers, "ETag", etag.c_str());
    }

    auto statusCode = soup_session_send_message(data_->soupSession_, soupMessage);
    if (statusCode != 204)
    {
        return false;
    }
    return true;
}

} // namespace http
