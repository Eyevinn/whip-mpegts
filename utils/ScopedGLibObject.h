#pragma once

#include <gst/gst.h>

namespace utils
{

template <typename T>
class ScopedGLibObject
{
public:
    explicit ScopedGLibObject(T object) : object_(object) {}

    ~ScopedGLibObject()
    {
        if (object_)
        {
            gst_object_unref(reinterpret_cast<gpointer>(object_));
        }
    }

    [[nodiscard]] auto get() const { return object_; }

private:
    T object_;
};

} // namespace utils
