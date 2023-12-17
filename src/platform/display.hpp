#ifndef SHADOW_CAST_PLATFORM_DISPLAY_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_DISPLAY_HPP_INCLUDED

#include "display/display.hpp"
#include "platform/wayland.hpp"
#include "utils/cmd_line.hpp"
#include <variant>

namespace sc
{
struct WaylandEnvironment
{
    std::unique_ptr<Wayland> platform;
};

struct X11Environment
{
    XDisplayPtr display;
};

using DisplayEnvironment = std::variant<X11Environment, WaylandEnvironment>;

auto select_display(Parameters const&) -> DisplayEnvironment;

} // namespace sc

#endif // SHADOW_CAST_PLATFORM_DISPLAY_HPP_INCLUDED
