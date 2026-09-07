#pragma once
#define GST_USE_UNSTABLE_API 1

#include <chrono>
#include <cstdint>
#include <gst/gst.h>
#include <map>
#include <memory>
#include <string>

struct Config;

namespace http
{
class WhipClient;
}

class Pipeline
{
public:
    Pipeline(http::WhipClient& whipClient, const Config& config);
    ~Pipeline();

    void run();
    void stop();
    const std::string& getWhipResource() const { return whipResource_; }

    void onDemuxPadAdded(GstPad* newPad);
    void onDemuxNoMorePads();
    void onOfferCreated(GstPromise* promise);
    void onNegotiationNeeded();
    void onIceCandidate(guint mLineIndex, gchar* candidate);

    static gboolean pipelineBusWatch(GstBus* /*bus*/, GstMessage* message, gpointer userData);
    static void demuxPadAddedCallback(GstElement* /*src*/, GstPad* newPad, gpointer userData);
    static void demuxNoMorePadsCallback(GstElement* /*src*/, gpointer userData);
    static void onOfferCreatedCallback(GstPromise* promise, gpointer userData);
    static void onNegotiationNeededCallback(GstElement* /*webRtcBin*/, gpointer userData);
    static void onIceCandidateCallback(GstElement* /*webrtc*/, guint mLineIndex, gchar* candidate, gpointer userData);
    static gboolean signalHandlerCallback(gpointer userData);

    // GCC bandwidth estimator (issue #44): webrtcbin requests an aux sender for each send
    // transport; we return an rtpgccbwe element seeded from the configured video bitrate and
    // observe its estimate. The estimate is only logged for now (issue #45 will act on it).
    static GstElement* requestAuxSenderCallback(GstElement* webRtcBin,
        guint sessionId,
        gpointer userData);
    static void estimatedBitrateNotifyCallback(GstObject* gccBwe, GParamSpec* pspec, gpointer userData);

    GstElement* onRequestAuxSender(guint sessionId);
    void onEstimatedBitrate(GstObject* gccBwe);

private:
private:
    enum class ElementLabel
    {
        UDP_SOURCE,
        UDP_QUEUE,
        TS_DEMUX,

        SRT_SOURCE,

        TEE,
        RESTREAM_QUEUE,
        UDP_DEST,
        SRT_DEST,

        H264_PARSE,
        H264_DECODE,

        H265_PARSE,
        H265_DECODE,

        VIDEO_CONVERT,

        CLOCK_OVERLAY,

        MPEG2_PARSE,
        MPEG2_DECODE,

        RTP_VIDEO_ENCODE,
        RTP_VIDEO_ENCODE_CAPS,
        RTP_VIDEO_PAYLOAD,
        RTP_VIDEO_PAYLOAD_QUEUE,
        RTP_VIDEO_FILTER,

        AAC_PARSE,
        AAC_DECODE,

        PCM_PARSE,

        OPUS_PARSE,
        OPUS_DECODE,

        AUDIO_CONVERT,
        AUDIO_RESAMPLE,

        RTP_AUDIO_ENCODE,
        RTP_AUDIO_PAYLOAD,
        RTP_AUDIO_PAYLOAD_QUEUE,
        RTP_AUDIO_FILTER,

        WEBRTC_BIN
    };

    http::WhipClient& whipClient_;
    const Config& config_;

    GstBus* pipelineMessageBus_;
    GstElement* pipeline_;
    std::map<ElementLabel, GstElement*> elements_;

    std::string whipResource_;
    std::string etag_;

    // Throttle for the GCC estimated-bitrate log (issue #44). The notify fires on the
    // estimator's streaming thread, so this is only touched from that handler.
    std::chrono::steady_clock::time_point lastEstimateLog_;

    void makeElement(const ElementLabel elementLabel, const char* element);
    void onH264SinkPadAdded(GstPad* newPad);
    void onH265SinkPadAdded(GstPad* newPad);
    void onMpeg2SinkPadAdded(GstPad* newPad);
    void onAacSinkPadAdded(GstPad* newPad);
    void onPcmSinkPadAdded(GstPad* newPad);
    void onOpusSinkPadAdded(GstPad* newPad);

    GstElement* addClockOverlay(GstElement* lastElement);
};
