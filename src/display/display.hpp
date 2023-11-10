#ifndef SHADOW_CAST_DISPLAY_DISPLAY_HPP_INCLUDED
#define SHADOW_CAST_DISPLAY_DISPLAY_HPP_INCLUDED

#include <X11/Xlib.h>
#include <memory>

namespace sc
{
struct XDisplayDeleter
{
    auto operator()(Display* ptr) const noexcept -> void;
};

using XDisplayPtr = std::unique_ptr<Display, XDisplayDeleter>;

auto get_display() -> XDisplayPtr;
} // namespace sc

#endif // SHADOW_CAST_DISPLAY_DISPLAY_HPP_INCLUDED
