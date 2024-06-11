#include "./x11_desktop.hpp"
#include "av/codec.hpp"
#include "display/display.hpp"
#include <cstdint>

namespace sc
{
X11Desktop::X11Desktop()
    : display_ { get_display() }
    , window_ { 0 }
{
    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display_.get()));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display_.get()));

    window_ = XCreateSimpleWindow(
        display_.get(),
        XRootWindow(display_.get(), XDefaultScreen(display_.get())),
        0,
        0,
        output_width,
        output_height,
        0,
        XBlackPixel(display_.get(), XDefaultScreen(display_.get())),
        XBlackPixel(display_.get(), XDefaultScreen(display_.get())));
}

X11Desktop::X11Desktop(X11Desktop&& other) noexcept
    : display_ { std::move(other.display_) }
    , window_ { std::exchange(other.window_, 0) }
{
}

X11Desktop::~X11Desktop()
{
    if (window_) {
        XDestroyWindow(display_.get(), window_);
    }
}

auto X11Desktop::operator=(X11Desktop&& other) noexcept -> X11Desktop&
{
    using std::swap;
    auto tmp { std::move(other) };
    swap(display_, tmp.display_);
    swap(window_, tmp.window_);
    return *this;
}

auto X11Desktop::size() const noexcept -> VideoOutputSize
{
    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display_.get()));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display_.get()));

    return { .width = output_width, .height = output_height };
}

auto X11Desktop::display() const noexcept -> X11Desktop::NativeDisplayType
{
    return display_.get();
}

auto X11Desktop::window() const noexcept -> Window { return window_; }

} // namespace sc
