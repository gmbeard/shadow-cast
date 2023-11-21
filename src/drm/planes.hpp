#ifndef SHADOW_CAST_DRM_PLANES_HPP_INCLUDED
#define SHADOW_CAST_DRM_PLANES_HPP_INCLUDED

#include <cinttypes>
#include <cstddef>

namespace sc
{

namespace plane_flags
{

enum PlaneFlags : std::uint32_t
{
    IS_CURSOR = (1 << 0),
    IS_COMBINED = (1 << 1)
};

}

struct PlaneDescriptor
{
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t offset;
    uint32_t pixel_format;
    uint64_t modifier;
    uint32_t connector_id;
    uint32_t flags;
    // bool is_combined_plane;
    // bool is_cursor;
    int x;
    int y;
    int src_w;
    int src_h;

    auto set_flag(plane_flags::PlaneFlags /* flag */) noexcept -> void;
    auto is_flag_set(plane_flags::PlaneFlags /* flag */) const noexcept -> bool;
};
} // namespace sc

#endif // SHADOW_CAST_DRM_PLANES_HPP_INCLUDED
