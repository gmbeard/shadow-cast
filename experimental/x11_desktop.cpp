#include "./x11_desktop.hpp"
#include "av/codec.hpp"
#include "display/display.hpp"
#include <cinttypes>

namespace sc
{
X11Desktop::X11Desktop()
    : display_ { get_display() }
{
}

auto X11Desktop::size() const noexcept -> VideoOutputSize
{
    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display_.get()));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display_.get()));

    return { .width = output_width, .height = output_height };
}
} // namespace sc
