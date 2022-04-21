#pragma once

#include <gst/gst.h>

namespace utils
{

template<typename T>
class ScopedGLibMem
{
public:
    explicit ScopedGLibMem(T object) : object_(object) {}

    ~ScopedGLibMem()
    {
        if (object_)
        {
            g_free(object_);
        }
    }

    [[nodiscard]] auto get() const { return object_; }

private:
    T object_;
};

} // namespace utils
