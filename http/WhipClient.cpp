#include "http/WhipClient.h"
#include "Logger.h"
#include <algorithm>
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
    data_->soupSession_ = soup_session_new();
    if (!data_->soupSession_)
    {
        assert(false);
        return;
    }

    // Set timeout (5 seconds)
    g_object_set(data_->soupSession_, "timeout", 5, nullptr);
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

    // Set request body
    GBytes* bytes = g_bytes_new(sdp.c_str(), sdp.size());
    soup_message_set_request_body_from_bytes(soupMessage, "application/sdp", bytes);
    g_bytes_unref(bytes);

    // Set authorization header if provided
    if (!authKey_.empty())
    {
        // This is for Broadcast Box compatibility (same as OBS studio)
        auto bearer_token_header = std::string("Bearer ") + authKey_;
        SoupMessageHeaders* requestHeaders = soup_message_get_request_headers(soupMessage);
        soup_message_headers_append(requestHeaders, "Authorization", bearer_token_header.c_str());
    }

    // Send the message synchronously
    GError* error = nullptr;
    GBytes* responseBytes = soup_session_send_and_read(data_->soupSession_, soupMessage, nullptr, &error);

    auto statusCode = soup_message_get_status(soupMessage);

    if (error || statusCode != 201)
    {
        Logger::log("Failed to send offer, status code: %d", statusCode);
        if (error)
        {
            Logger::log("Error: %s", error->message);
            g_error_free(error);
        }
        if (responseBytes)
        {
            g_bytes_unref(responseBytes);
        }
        g_object_unref(soupMessage);
        return {};
    }

    // Get response headers
    std::unordered_map<std::string, std::string> headers;
    SoupMessageHeaders* responseHeaders = soup_message_get_response_headers(soupMessage);
    soup_message_headers_foreach(responseHeaders, iterateResponseHeaders, &headers);

    const auto locationItr = headers.find("location");
    if (locationItr == headers.cend())
    {
        g_bytes_unref(responseBytes);
        g_object_unref(soupMessage);
        return {};
    }

    SendOfferResult result;
    result.resource_ = locationItr->second;

    // Get response body
    gsize dataSize;
    const char* data = static_cast<const char*>(g_bytes_get_data(responseBytes, &dataSize));
    result.sdpAnswer_ = std::string(data, dataSize);

    const auto etagItr = headers.find("etag");
    if (etagItr != headers.cend())
    {
        result.etag_ = etagItr->second;
    }

    g_bytes_unref(responseBytes);
    g_object_unref(soupMessage);

    return result;
}

bool WhipClient::updateIce(const std::string& resourceUrl, const std::string& etag, std::string&& sdp)
{
    auto soupMessage = soup_message_new("PATCH", resourceUrl.c_str());
    if (!soupMessage)
    {
        return false;
    }

    // Set request body
    GBytes* bytes = g_bytes_new(sdp.c_str(), sdp.size());
    soup_message_set_request_body_from_bytes(soupMessage, "application/trickle-ice-sdpfrag", bytes);
    g_bytes_unref(bytes);

    // Set headers
    SoupMessageHeaders* requestHeaders = soup_message_get_request_headers(soupMessage);
    if (!authKey_.empty())
    {
        soup_message_headers_append(requestHeaders, "Authorization", authKey_.c_str());
    }
    if (!etag.empty())
    {
        soup_message_headers_append(requestHeaders, "ETag", etag.c_str());
    }

    // Send the message synchronously
    GError* error = nullptr;
    GBytes* responseBytes = soup_session_send_and_read(data_->soupSession_, soupMessage, nullptr, &error);

    auto statusCode = soup_message_get_status(soupMessage);

    if (responseBytes)
    {
        g_bytes_unref(responseBytes);
    }

    if (error)
    {
        g_error_free(error);
    }

    g_object_unref(soupMessage);

    if (statusCode != 204)
    {
        return false;
    }
    return true;
}

bool WhipClient::deleteSession(const std::string& resourceUrl)
{
    if (resourceUrl.empty())
    {
        Logger::log("Cannot delete session: resource URL is empty");
        return false;
    }

    // Construct full URL from base URL and resource path
    std::string fullUrl;
    if (resourceUrl.find("http://") == 0 || resourceUrl.find("https://") == 0)
    {
        // Already a full URL
        fullUrl = resourceUrl;
    }
    else
    {
        // Relative URL - need to construct full URL from base
        // Parse base URL to get scheme, host, and port
        GUri* baseUri = g_uri_parse(url_.c_str(), G_URI_FLAGS_NONE, nullptr);
        if (!baseUri)
        {
            Logger::log("Failed to parse base URL: %s", url_.c_str());
            return false;
        }

        // Build full URL
        GUri* fullUri = g_uri_parse_relative(baseUri, resourceUrl.c_str(), G_URI_FLAGS_NONE, nullptr);
        g_uri_unref(baseUri);

        if (!fullUri)
        {
            Logger::log("Failed to construct full URL from resource: %s", resourceUrl.c_str());
            return false;
        }

        gchar* fullUrlCStr = g_uri_to_string(fullUri);
        fullUrl = std::string(fullUrlCStr);
        g_free(fullUrlCStr);
        g_uri_unref(fullUri);
    }

    Logger::log("Sending DELETE request to: %s", fullUrl.c_str());

    auto soupMessage = soup_message_new("DELETE", fullUrl.c_str());
    if (!soupMessage)
    {
        Logger::log("Failed to create DELETE request");
        return false;
    }

    // Set authorization header if provided
    if (!authKey_.empty())
    {
        auto bearer_token_header = std::string("Bearer ") + authKey_;
        SoupMessageHeaders* requestHeaders = soup_message_get_request_headers(soupMessage);
        soup_message_headers_append(requestHeaders, "Authorization", bearer_token_header.c_str());
    }

    // Send the message synchronously
    GError* error = nullptr;
    GBytes* responseBytes = soup_session_send_and_read(data_->soupSession_, soupMessage, nullptr, &error);

    auto statusCode = soup_message_get_status(soupMessage);

    if (responseBytes)
    {
        g_bytes_unref(responseBytes);
    }

    if (error)
    {
        Logger::log("Error deleting session: %s", error->message);
        g_error_free(error);
        g_object_unref(soupMessage);
        return false;
    }

    g_object_unref(soupMessage);

    // RFC 9725: Server should respond with 200 OK
    if (statusCode != 200)
    {
        Logger::log("Failed to delete session, status code: %d", statusCode);
        return false;
    }

    Logger::log("Session deleted successfully");
    return true;
}

} // namespace http
