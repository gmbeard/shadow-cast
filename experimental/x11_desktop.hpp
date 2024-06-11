#ifndef SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED

#include "av/codec.hpp"
#include "display/display.hpp"

namespace sc
{
struct X11Desktop
{
    using NativeDisplayType = XDisplayPtr::pointer;

    X11Desktop();
    X11Desktop(X11Desktop&& other) noexcept;
    ~X11Desktop();
    auto operator=(X11Desktop&& other) noexcept -> X11Desktop&;

    auto size() const noexcept -> VideoOutputSize;

    auto display() const noexcept -> X11Desktop::NativeDisplayType;

    auto window() const noexcept -> Window;

private:
    XDisplayPtr display_;
    Window window_;
};
} // namespace sc
#endif // SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
