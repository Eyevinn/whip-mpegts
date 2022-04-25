#define GST_USE_UNSTABLE_API 1

#include "Pipeline.h"
#include "http/WhipClient.h"
#include "Logger.h"
#include "utils/ScopedGLibMem.h"
#include "utils/ScopedGLibObject.h"
#include "utils/ScopedGstObject.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

class Pipeline::Impl
{
public:
    Impl(http::WhipClient& whipClient, std::string&& mpegTsAddress, const uint32_t mpegTsPort);
    ~Impl();

    void run();
    void stop();

    void onPipelineMessage(GstMessage* message);
    void onDemuxPadAdded(GstPad* newPad);
    void onOfferCreated(GstPromise* promise);
    void onNegotiationNeeded();
    void onIceCandidate(guint mLineIndex, gchar* candidate);

    static gboolean pipelineBusWatch(GstBus* /*bus*/, GstMessage* message, gpointer userData);
    static void demuxPadAddedCallback(GstElement* /*src*/, GstPad* newPad, gpointer userData);
    static void onOfferCreatedCallback(GstPromise* promise, gpointer userData);
    static void onNegotiationNeededCallback(GstElement* /*webRtcBin*/, gpointer userData);
    static void onIceCandidateCallback(GstElement* /*webrtc*/, guint mLineIndex, gchar* candidate, gpointer userData);

private:
    enum class ElementLabel
    {
        UDP_SOURCE,
        UDP_QUEUE,
        TS_DEMUX,
        VIDEO_DEMUX_QUEUE,

        H264_PARSE,
        H264_DECODE,
        H264_DECODE_QUEUE,

        VIDEO_CONVERT,
        VIDEO_CONVERT_QUEUE,

        MPEG2_PARSE,
        MPEG2_DECODE,
        MPEG2_DECODE_QUEUE,

        RTP_VIDEO_ENCODE,
        RTP_VIDEO_ENCODE_QUEUE,
        RTP_VIDEO_PAYLOAD,
        RTP_VIDEO_PAYLOAD_QUEUE,
        RTP_VIDEO_FILTER,

        AUDIO_DEMUX_QUEUE,

        AAC_PARSE,
        AAC_PARSE_QUEUE,
        AAC_DECODE,
        AAC_DECODE_QUEUE,

        PCM_PARSE,

        AUDIO_CONVERT,
        AUDIO_RESAMPLE,
        AUDIO_RESAMPLE_QUEUE,

        RTP_AUDIO_ENCODE,
        RTP_AUDIO_ENCODE_QUEUE,
        RTP_AUDIO_PAYLOAD,
        RTP_AUDIO_PAYLOAD_QUEUE,
        RTP_AUDIO_FILTER,

        WEBRTC_BIN
    };

    GstBus* pipelineMessageBus_;
    GstElement* pipeline_;
    std::map<ElementLabel, GstElement*> elements_;

    http::WhipClient& whipClient_;

    std::string whipResource_;

    void makeElement(const ElementLabel elementLabel, const char* name, const char* element);
    [[nodiscard]] bool validate() const noexcept;
};

Pipeline::Impl::Impl(http::WhipClient& whipClient, std::string&& mpegTsAddress, const uint32_t mpegTsPort)
    : pipelineMessageBus_(nullptr),
      whipClient_(whipClient)
{
    gst_init(nullptr, nullptr);

    pipeline_ = gst_pipeline_new("mpeg-ts-pipeline");
    makeElement(ElementLabel::UDP_SOURCE, "UDP_SOURCE", "udpsrc");
    makeElement(ElementLabel::UDP_QUEUE, "UDP_QUEUE", "queue");
    makeElement(ElementLabel::TS_DEMUX, "TS_DEMUX", "tsdemux");

    makeElement(ElementLabel::VIDEO_DEMUX_QUEUE, "VIDEO_DEMUX_QUEUE", "queue");

    makeElement(ElementLabel::H264_PARSE, "H264_PARSE", "h264parse");
    makeElement(ElementLabel::H264_DECODE, "H264_DECODE", "avdec_h264");
    makeElement(ElementLabel::H264_DECODE_QUEUE, "H264_DECODE_QUEUE", "queue");

    makeElement(ElementLabel::VIDEO_CONVERT, "VIDEO_CONVERT", "videoconvert");
    makeElement(ElementLabel::VIDEO_CONVERT_QUEUE, "VIDEO_CONVERT_QUEUE", "queue");

    makeElement(ElementLabel::MPEG2_PARSE, "MPEG2_PARSE", "mpegvideoparse");
    makeElement(ElementLabel::MPEG2_DECODE, "MPEG2_DECODE", "avdec_mpeg2video");
    makeElement(ElementLabel::MPEG2_DECODE_QUEUE, "MPEG2_DECODE_QUEUE", "queue");

    makeElement(ElementLabel::RTP_VIDEO_ENCODE, "RTP_VIDEO_ENCODE", "vp8enc");
    makeElement(ElementLabel::RTP_VIDEO_ENCODE_QUEUE, "RTP_VIDEO_ENCODE_QUEUE", "queue");
    makeElement(ElementLabel::RTP_VIDEO_PAYLOAD, "RTP_VIDEO_PAYLOAD", "rtpvp8pay");
    makeElement(ElementLabel::RTP_VIDEO_PAYLOAD_QUEUE, "RTP_VIDEO_PAYLOAD_QUEUE", "queue");
    makeElement(ElementLabel::RTP_VIDEO_FILTER, "RTP_VIDEO_FILTER", "capsfilter");

    makeElement(ElementLabel::AUDIO_DEMUX_QUEUE, "AUDIO_DEMUX_QUEUE", "queue");
    makeElement(ElementLabel::AAC_PARSE, "AAC_PARSE", "aacparse");
    makeElement(ElementLabel::AAC_PARSE_QUEUE, "AAC_PARSE_QUEUE", "queue");
    makeElement(ElementLabel::AAC_DECODE, "AAC_DECODE", "avdec_aac");
    makeElement(ElementLabel::AAC_DECODE_QUEUE, "AAC_DECODE_QUEUE", "queue");

    makeElement(ElementLabel::PCM_PARSE, "PCM_PARSE", "rawaudioparse");

    makeElement(ElementLabel::AUDIO_CONVERT, "AUDIO_CONVERT", "audioconvert");
    makeElement(ElementLabel::AUDIO_RESAMPLE, "AUDIO_RESAMPLE", "audioresample");
    makeElement(ElementLabel::AUDIO_RESAMPLE_QUEUE, "AUDIO_RESAMPLE_QUEUE", "queue");
    makeElement(ElementLabel::RTP_AUDIO_ENCODE, "RTP_AUDIO_ENCODE", "opusenc");
    makeElement(ElementLabel::RTP_AUDIO_ENCODE_QUEUE, "RTP_AUDIO_ENCODE_QUEUE", "queue");
    makeElement(ElementLabel::RTP_AUDIO_PAYLOAD, "RTP_AUDIO_PAYLOAD", "rtpopuspay");
    makeElement(ElementLabel::RTP_AUDIO_PAYLOAD_QUEUE, "RTP_AUDIO_PAYLOAD_QUEUE", "queue");
    makeElement(ElementLabel::RTP_AUDIO_FILTER, "RTP_AUDIO_FILTER", "capsfilter");

    makeElement(ElementLabel::WEBRTC_BIN, "WEBRTC_BIN", "webrtcbin");

    if (!validate())
    {
        Logger::log("Gst elements validation failed");
        return;
    }

    for (const auto& entry : elements_)
    {
        if (!gst_bin_add(GST_BIN(pipeline_), entry.second))
        {
            Logger::log("Unable to add gst element");
            return;
        }
    }

    if (!gst_element_link_many(elements_[ElementLabel::UDP_SOURCE],
            elements_[ElementLabel::UDP_QUEUE],
            elements_[ElementLabel::TS_DEMUX],
            nullptr))
    {
        g_printerr("UPD source elements could not be linked.\n");
        return;
    }

    pipelineMessageBus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_add_watch(pipelineMessageBus_, reinterpret_cast<GstBusFunc>(pipelineBusWatch), pipeline_);

    g_signal_connect(elements_[ElementLabel::TS_DEMUX], "pad-added", G_CALLBACK(demuxPadAddedCallback), this);

    g_object_set(elements_[ElementLabel::UDP_SOURCE],
        "address",
        mpegTsAddress.c_str(),
        "port",
        mpegTsPort,
        "auto-multicast",
        true,
        "buffer-size",
        825984,
        nullptr);

    g_object_set(elements_[ElementLabel::UDP_QUEUE], "min-threshold-time", 1000000000ULL, nullptr);
    g_object_set(elements_[ElementLabel::RTP_VIDEO_ENCODE_QUEUE], "min-threshold-time", 1000000000ULL, nullptr);

    g_object_set(elements_[ElementLabel::H264_PARSE], "disable-passthrough", TRUE, nullptr);
    //g_object_set(elements_[ElementLabel::H264_DECODE], "skip-frame", 1, nullptr);

    g_object_set(elements_[ElementLabel::RTP_VIDEO_ENCODE],
        "threads",
        2,
        "target-bitrate",
        256000000,
        "error-resilient",
        1,
        "end-usage",
        0,
        "deadline",
        1,
        nullptr);

    utils::ScopedGstObject rtpAudioFilterCaps(gst_caps_new_simple("application/x-rtp",
        "media",
        G_TYPE_STRING,
        "audio",
        "payload",
        G_TYPE_INT,
        111,
        "encoding-name",
        G_TYPE_STRING,
        "OPUS",
        nullptr));

    gst_element_link_filtered(elements_[ElementLabel::RTP_AUDIO_PAYLOAD_QUEUE],
        elements_[ElementLabel::WEBRTC_BIN],
        rtpAudioFilterCaps.get());

    utils::ScopedGstObject rtpVideoFilterCaps(gst_caps_new_simple("application/x-rtp",
        "media",
        G_TYPE_STRING,
        "video",
        "payload",
        G_TYPE_INT,
        96,
        "encoding-name",
        G_TYPE_STRING,
        "VP8",
        nullptr));

    gst_element_link_filtered(elements_[ElementLabel::RTP_VIDEO_PAYLOAD_QUEUE],
        elements_[ElementLabel::WEBRTC_BIN],
        rtpVideoFilterCaps.get());

    g_object_set(elements_[ElementLabel::WEBRTC_BIN],
        "name",
        "send",
        "stun-server",
        "stun://stun.l.google.com:19302",
        "bundle-policy",
        GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
        nullptr);
    gst_element_sync_state_with_parent(elements_[ElementLabel::WEBRTC_BIN]);

    g_signal_connect(elements_[ElementLabel::WEBRTC_BIN],
        "on-negotiation-needed",
        G_CALLBACK(onNegotiationNeededCallback),
        this);
    g_signal_connect(elements_[ElementLabel::WEBRTC_BIN], "on-ice-candidate", G_CALLBACK(onIceCandidateCallback), this);
}

Pipeline::Impl::~Impl()
{
    gst_element_set_state(pipeline_, GST_STATE_NULL);

    if (pipeline_)
    {
        gst_object_unref(pipeline_);
    }
    if (pipelineMessageBus_)
    {
        gst_object_unref(pipelineMessageBus_);
    }

    gst_bus_remove_watch(pipelineMessageBus_);
    gst_deinit();
}

void Pipeline::Impl::onPipelineMessage(GstMessage* message)
{
    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_ELEMENT(message->src) == pipeline_)
        {
            GstState oldState;
            GstState newState;
            GstState pendingState;

            gst_message_parse_state_changed(message, &oldState, &newState, &pendingState);

            {
                auto dumpName = g_strconcat("state_changed-",
                    gst_element_state_get_name(oldState),
                    "_",
                    gst_element_state_get_name(newState),
                    nullptr);
                GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(message->src), GST_DEBUG_GRAPH_SHOW_ALL, dumpName);
                g_free(dumpName);
            }
        }
        break;

    case GST_MESSAGE_ERROR:
    {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipe), GST_DEBUG_GRAPH_SHOW_ALL, "error");

        GError* err = nullptr;
        gchar* dbgInfo = nullptr;

        gst_message_parse_error(message, &err, &dbgInfo);
        Logger::log("ERROR from element %s: %s", GST_OBJECT_NAME(message->src), err->message);
        Logger::log("Debugging info: %s", dbgInfo ? dbgInfo : "none");
        g_error_free(err);
        g_free(dbgInfo);
    }
    break;

    case GST_MESSAGE_EOS:
    {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipe), GST_DEBUG_GRAPH_SHOW_ALL, "eos");
        Logger::log("EOS received");
    }
    break;

    case GST_MESSAGE_NEW_CLOCK:
    {
        Logger::log("New pipeline clock");
    }
    break;

    case GST_MESSAGE_CLOCK_LOST:
        Logger::log("Clock lost, restarting pipeline");
        if (gst_element_set_state(pipeline_, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE ||
            gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        {
            Logger::log("Unable to restart the pipeline.");
            return;
        }
        break;

    default:
        break;
    }
}

void Pipeline::Impl::onDemuxPadAdded(GstPad* newPad)
{
    utils::ScopedGstObject newPadCaps(gst_pad_get_current_caps(newPad));
    auto newPadStruct = gst_caps_get_structure(newPadCaps.get(), 0);
    auto newPadType = gst_structure_get_name(newPadStruct);

    Logger::log("Dynamic pad created, type %s", newPadType);

    if (g_str_has_prefix(newPadType, "video/x-h264"))
    {
        const auto& findResult = elements_.find(ElementLabel::VIDEO_DEMUX_QUEUE);
        if (findResult == elements_.cend())
        {
            return;
        }

        if (!gst_element_link_many(elements_[ElementLabel::VIDEO_DEMUX_QUEUE],
                elements_[ElementLabel::H264_PARSE],
                elements_[ElementLabel::H264_DECODE],
                elements_[ElementLabel::H264_DECODE_QUEUE],
                elements_[ElementLabel::VIDEO_CONVERT],
                elements_[ElementLabel::VIDEO_CONVERT_QUEUE],
                elements_[ElementLabel::RTP_VIDEO_ENCODE],
                elements_[ElementLabel::RTP_VIDEO_ENCODE_QUEUE],
                elements_[ElementLabel::RTP_VIDEO_PAYLOAD],
                elements_[ElementLabel::RTP_VIDEO_PAYLOAD_QUEUE],
                nullptr))
        {
            Logger::log("Video elements could not be linked.");
            return;
        }

        utils::ScopedGLibObject sinkPad(gst_element_get_static_pad(findResult->second, "sink"));
        if (gst_pad_is_linked(sinkPad.get()))
        {
            return;
        }

        gst_pad_link(newPad, sinkPad.get());
    }
    else if (g_str_has_prefix(newPadType, "video/mpeg"))
    {
        const auto& findResult = elements_.find(ElementLabel::VIDEO_DEMUX_QUEUE);
        if (findResult == elements_.cend())
        {
            return;
        }

        if (!gst_element_link_many(elements_[ElementLabel::VIDEO_DEMUX_QUEUE],
                elements_[ElementLabel::MPEG2_PARSE],
                elements_[ElementLabel::MPEG2_DECODE],
                elements_[ElementLabel::MPEG2_DECODE_QUEUE],
                elements_[ElementLabel::VIDEO_CONVERT],
                elements_[ElementLabel::VIDEO_CONVERT_QUEUE],
                elements_[ElementLabel::RTP_VIDEO_ENCODE],
                elements_[ElementLabel::RTP_VIDEO_ENCODE_QUEUE],
                elements_[ElementLabel::RTP_VIDEO_PAYLOAD],
                elements_[ElementLabel::RTP_VIDEO_PAYLOAD_QUEUE],
                nullptr))
        {
            Logger::log("Video elements could not be linked.");
            return;
        }

        utils::ScopedGLibObject sinkPad(gst_element_get_static_pad(findResult->second, "sink"));
        if (gst_pad_is_linked(sinkPad.get()))
        {
            return;
        }

        gst_pad_link(newPad, sinkPad.get());
    }
    else if (g_str_has_prefix(newPadType, "audio/mpeg"))
    {
        const auto& findResult = elements_.find(ElementLabel::AUDIO_DEMUX_QUEUE);
        if (findResult == elements_.cend())
        {
            return;
        }

        if (!gst_element_link_many(elements_[ElementLabel::AUDIO_DEMUX_QUEUE],
                elements_[ElementLabel::AAC_PARSE],
                elements_[ElementLabel::AAC_PARSE_QUEUE],
                elements_[ElementLabel::AAC_DECODE],
                elements_[ElementLabel::AAC_DECODE_QUEUE],
                elements_[ElementLabel::AUDIO_CONVERT],
                elements_[ElementLabel::AUDIO_RESAMPLE],
                elements_[ElementLabel::AUDIO_RESAMPLE_QUEUE],
                elements_[ElementLabel::RTP_AUDIO_ENCODE],
                elements_[ElementLabel::RTP_AUDIO_ENCODE_QUEUE],
                elements_[ElementLabel::RTP_AUDIO_PAYLOAD],
                elements_[ElementLabel::RTP_AUDIO_PAYLOAD_QUEUE],
                nullptr))
        {
            Logger::log("Audio elements could not be linked.");
            return;
        }

        utils::ScopedGLibObject sinkPad(gst_element_get_static_pad(findResult->second, "sink"));
        if (gst_pad_is_linked(sinkPad.get()))
        {
            return;
        }

        gst_pad_link(newPad, sinkPad.get());
    }
    else if (g_str_has_prefix(newPadType, "audio/x-raw"))
    {
        const auto& findResult = elements_.find(ElementLabel::AUDIO_DEMUX_QUEUE);
        if (findResult == elements_.cend())
        {
            return;
        }

        if (!gst_element_link_many(elements_[ElementLabel::AUDIO_DEMUX_QUEUE],
                elements_[ElementLabel::PCM_PARSE],
                elements_[ElementLabel::AUDIO_CONVERT],
                elements_[ElementLabel::AUDIO_RESAMPLE],
                elements_[ElementLabel::AUDIO_RESAMPLE_QUEUE],
                elements_[ElementLabel::RTP_AUDIO_ENCODE],
                elements_[ElementLabel::RTP_AUDIO_ENCODE_QUEUE],
                elements_[ElementLabel::RTP_AUDIO_PAYLOAD],
                elements_[ElementLabel::RTP_AUDIO_PAYLOAD_QUEUE],
                nullptr))
        {
            Logger::log("Audio elements could not be linked.");
            return;
        }

        utils::ScopedGLibObject sinkPad(gst_element_get_static_pad(findResult->second, "sink"));
        if (gst_pad_is_linked(sinkPad.get()))
        {
            return;
        }

        gst_pad_link(newPad, sinkPad.get());
    }
}

void Pipeline::Impl::onNegotiationNeeded()
{
    Logger::log("onNegotiationNeeded");

    GArray* transceivers;
    g_signal_emit_by_name(elements_[ElementLabel::WEBRTC_BIN], "get-transceivers", &transceivers);

    for (uint32_t i = 0; i < transceivers->len; ++i)
    {
        auto transceiver = g_array_index(transceivers, GstWebRTCRTPTransceiver*, i);
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(transceiver), "direction") != nullptr)
        {
            g_object_set(transceiver, "direction", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, nullptr);
        }
        g_object_set(transceiver, "fec-type", GST_WEBRTC_FEC_TYPE_NONE, nullptr);
    }

    if (transceivers->len == 2)
    {
        auto transceiver = g_array_index(transceivers, GstWebRTCRTPTransceiver*, 1);
        g_object_set(transceiver, "do-nack", TRUE, nullptr);
    }
    else
    {
        Logger::log("Expected 2 trancievers, but there are %u available. Not enabling NACK/RTX.", transceivers->len);
    }

    auto promise = gst_promise_new_with_change_func(onOfferCreatedCallback, this, nullptr);
    g_signal_emit_by_name(elements_[ElementLabel::WEBRTC_BIN], "create-offer", nullptr, promise);
}

void Pipeline::Impl::onOfferCreated(GstPromise* promise)
{
    Logger::log("onOfferCreated");

    utils::ScopedGstObject<GstWebRTCSessionDescription> offer;
    {
        GstWebRTCSessionDescription* offerPointer = nullptr;
        const auto reply = gst_promise_get_reply(promise);
        gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offerPointer, nullptr);
        offer.set(offerPointer);
    }
    gst_promise_unref(promise);

    utils::ScopedGLibMem offerGChar(gst_sdp_message_as_text(offer.get()->sdp));
    const auto offerString = std::string(offerGChar.get());

    auto sendOfferReply = whipClient_.sendOffer(offerString);
    if (sendOfferReply.resource_.empty())
    {
        Logger::log("Unable to send offer to server");
        return;
    }
    whipResource_ = std::move(sendOfferReply.resource_);
    Logger::log("Server responded with resource %s", whipResource_.c_str());

    Logger::log("Setting local SDP");
    g_signal_emit_by_name(elements_[ElementLabel::WEBRTC_BIN], "set-local-description", offer.get(), nullptr);

    {
        GstSDPMessage* answerMessage = nullptr;

        // Implicitly deallocated by answer object below
        if (gst_sdp_message_new_from_text(sendOfferReply.sdpAnswer_.c_str(), &answerMessage) != GST_SDP_OK)
        {
            Logger::log("Unable to create SDP object from answer");
            return;
        }

        utils::ScopedGstObject answer(gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, answerMessage));
        if (!answer.get())
        {
            Logger::log("Unable to create SDP object from answer");
            return;
        }

        Logger::log("Setting remote SDP");
        g_signal_emit_by_name(elements_[ElementLabel::WEBRTC_BIN], "set-remote-description", answer.get(), nullptr);
    }
}

void Pipeline::Impl::onIceCandidate(guint mLineIndex, gchar* candidate)
{
    Logger::log("onIceCandidate %u %s", mLineIndex, candidate);

    if (whipResource_.empty())
    {
        Logger::log("Resource string empty");
        return;
    }

    std::array<char, 256> candidateString{};
    snprintf(candidateString.data(), candidateString.size(), "m=audio 9 RTP/AVP 0\r\na=mid:0\r\na=%s\r\n", candidate);
    if (!whipClient_.updateIce(whipResource_, candidateString.data()))
    {
        Logger::log("Unable to send ICE candidate to server");
    }
}

void Pipeline::Impl::makeElement(const ElementLabel elementLabel, const char* name, const char* element)
{
    const auto& result = elements_.emplace(elementLabel, gst_element_factory_make(element, name));
    if (!result.first->second)
    {
        Logger::log("Unable to make gst element %s", element);
        return;
    }

    if (strncmp(element, "queue", 5) == 0) {
        g_object_set(result.first->second, "max-size-buffers", 0, nullptr);
        g_object_set(result.first->second, "max-size-bytes", 0, nullptr);
        g_object_set(result.first->second, "max-size-time", 0, nullptr);
    }
}

bool Pipeline::Impl::validate() const noexcept
{
    if (!pipeline_)
    {
        return false;
    }

    return std::all_of(elements_.begin(), elements_.end(), [](const auto& element) {
        return element.second != nullptr;
    });
}

void Pipeline::Impl::run()
{
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        Logger::log("Unable to set the pipeline to the playing state.");
        return;
    }
}

void Pipeline::Impl::stop() {}

Pipeline::Pipeline(http::WhipClient& whipClient, std::string&& mpegTsAddress, const uint32_t mpegTsPort)
    : impl_(std::make_unique<Pipeline::Impl>(whipClient, std::move(mpegTsAddress), mpegTsPort))
{
}

Pipeline::~Pipeline() // NOLINT(modernize-use-equals-default)
{
}

void Pipeline::run()
{
    impl_->run();
}

void Pipeline::stop()
{
    impl_->stop();
}

gboolean Pipeline::Impl::pipelineBusWatch(GstBus* /*bus*/, GstMessage* message, gpointer userData)
{
    auto impl = reinterpret_cast<Pipeline::Impl*>(userData);
    impl->onPipelineMessage(message);
    return TRUE;
}

void Pipeline::Impl::demuxPadAddedCallback(GstElement* /*src*/, GstPad* newPad, gpointer userData)
{
    auto impl = reinterpret_cast<Pipeline::Impl*>(userData);
    impl->onDemuxPadAdded(newPad);
}

void Pipeline::Impl::onOfferCreatedCallback(GstPromise* promise, gpointer userData)
{
    auto pipelineImpl = reinterpret_cast<Pipeline::Impl*>(userData);
    pipelineImpl->onOfferCreated(promise);
}

void Pipeline::Impl::onNegotiationNeededCallback(GstElement* /*webRtcBin*/, gpointer userData)
{
    auto pipelineImpl = reinterpret_cast<Pipeline::Impl*>(userData);
    pipelineImpl->onNegotiationNeeded();
}

void Pipeline::Impl::onIceCandidateCallback(GstElement* /*webrtc*/,
    guint mLineIndex,
    gchar* candidate,
    gpointer userData)
{
    auto pipelineImpl = reinterpret_cast<Pipeline::Impl*>(userData);
    pipelineImpl->onIceCandidate(mLineIndex, candidate);
}
