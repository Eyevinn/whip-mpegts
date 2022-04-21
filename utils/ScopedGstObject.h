#pragma once

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>

namespace utils
{

template <typename T>
class ScopedGstObject
{
public:
    ScopedGstObject() : value_(nullptr) {}
    explicit ScopedGstObject(T* value) : value_(value) {}

    ~ScopedGstObject();

    void set(T* value) noexcept { value_ = value; }
    [[nodiscard]] auto get() const noexcept { return value_; }

private:
    T* value_;
};

template <>
ScopedGstObject<GstCaps>::~ScopedGstObject()
{
    gst_caps_unref(value_);
}

template <>
ScopedGstObject<GstMessage>::~ScopedGstObject()
{
    gst_message_unref(value_);
}

template <>
ScopedGstObject<GstBus>::~ScopedGstObject()
{
    gst_object_unref(value_);
}

template <>
ScopedGstObject<GstWebRTCSessionDescription>::~ScopedGstObject()
{
    gst_webrtc_session_description_free(value_);
}

} // namespace utils
