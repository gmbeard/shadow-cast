#ifndef SHADOW_CAST_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_DESKTOP_HPP_INCLUDED

#include "wayland_desktop.hpp"
#include "x11_desktop.hpp"
#include <variant>

namespace sc
{
using SupportedDesktop = std::variant<WaylandDesktop, X11Desktop>;

auto determine_desktop() -> SupportedDesktop;
} // namespace sc

#endif // SHADOW_CAST_DESKTOP_HPP_INCLUDED
