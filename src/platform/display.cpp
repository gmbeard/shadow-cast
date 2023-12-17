#include "platform/display.hpp"

namespace sc
{

auto select_display(Parameters const&) -> DisplayEnvironment
{
    wayland::DisplayPtr wayland_display { wl_display_connect(nullptr) };
    if (wayland_display) {
        auto wayland = initialize_wayland(std::move(wayland_display));
        return WaylandEnvironment {
            std::move(wayland),
        };
    }

    XDisplayPtr x11_display { XOpenDisplay(nullptr) };
    if (!x11_display)
        throw std::runtime_error { "Failed to get a usable display" };
    return X11Environment { std::move(x11_display) };
}

} // namespace sc
