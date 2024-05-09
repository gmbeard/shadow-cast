#include "./x11_platform.hpp"
#include <X11/Xlib.h>

namespace sc
{

X11Platform::X11Platform() noexcept
    : display_ { XOpenDisplay(nullptr) }
{
}

auto X11Platform::display() const noexcept -> display_type
{
    return display_.get();
}

auto X11Platform::is_supported() noexcept -> bool
{
    return static_cast<bool>(X11Platform {});
}

X11Platform::operator bool() const noexcept { return is_supported(); }

auto X11Platform::DisplayDeleter::operator()(display_type ptr) noexcept -> void
{
    XCloseDisplay(ptr);
}

} // namespace sc
