#ifndef SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED

#include "av/codec.hpp"
#include "display/display.hpp"
#include <EGL/egl.h>

namespace sc
{
struct X11Desktop
{
    X11Desktop() noexcept;

    operator bool() const noexcept;
    auto size() const noexcept -> VideoOutputSize;
    auto egl_display() const noexcept -> EGLDisplay;

private:
    bool initialized_ { false };
    XDisplayPtr display_;
};
} // namespace sc
#endif // SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
