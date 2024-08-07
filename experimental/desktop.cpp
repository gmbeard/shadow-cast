#include "desktop.hpp"
#include <stdexcept>

namespace sc
{
auto determine_desktop() -> SupportedDesktop
{
    WaylandDesktop wayland;
    if (wayland)
        return wayland;

    X11Desktop x11;
    if (x11)
        return x11;

    throw std::runtime_error { "Couldn't determine display platform" };
}
} // namespace sc
