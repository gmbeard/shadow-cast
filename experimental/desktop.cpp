#include "desktop.hpp"
#include "logging.hpp"
#include <stdexcept>

namespace sc
{
auto determine_desktop() -> SupportedDesktop
{
    WaylandDesktop wayland;
    if (wayland) {
        log(LogLevel::info, "Wayland display detected");
        return wayland;
    }

    X11Desktop x11;
    if (x11) {
        log(LogLevel::info, "X11 display detected");
        return x11;
    }

    throw std::runtime_error { "Couldn't determine display platform" };
}
} // namespace sc
