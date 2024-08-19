#ifndef SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED

#include "av/codec.hpp"
#include "display/display.hpp"
#include "wayland_desktop.hpp"
#include <EGL/egl.h>
#include <X11/X.h>
#include <string_view>

namespace sc
{
struct X11Desktop
{
    X11Desktop() noexcept;

    operator bool() const noexcept;
    auto size() const noexcept -> VideoOutputSize;
    auto egl_display() const noexcept -> EGLDisplay;
    auto gpu_vendor() const noexcept -> std::string_view;
    auto gpu_id() const noexcept -> std::string_view;

private:
    bool initialized_ { false };
    XDisplayPtr display_;
    Window window_ { 0 };
    detail::EGL x11egl_ {};
};
} // namespace sc
#endif // SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
