#include "drm/planes.hpp"

namespace sc
{

auto PlaneDescriptor::set_flag(plane_flags::PlaneFlags flag) noexcept -> void
{
    flags |= flag;
}

auto PlaneDescriptor::is_flag_set(plane_flags::PlaneFlags flag) const noexcept
    -> bool
{
    return (flags & flag) != 0;
}

} // namespace sc
