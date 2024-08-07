#include "./x11_desktop.hpp"
#include "av/codec.hpp"
#include "display/display.hpp"
#include "utils/contracts.hpp"
#include <EGL/egl.h>
#include <X11/Xlib.h>

namespace sc
{
X11Desktop::X11Desktop() noexcept
    : display_ { XOpenDisplay(nullptr) }
{
    initialized_ = !!display_;
}

X11Desktop::operator bool() const noexcept { return initialized_; }

auto X11Desktop::egl_display() const noexcept -> EGLDisplay
{
    SC_EXPECT(initialized_);

    /* FIXME:
     */
    return EGL_NO_DISPLAY;
}

auto X11Desktop::size() const noexcept -> VideoOutputSize
{
    SC_EXPECT(initialized_);

    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display_.get()));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display_.get()));

    return { .width = output_width, .height = output_height };
}
} // namespace sc
