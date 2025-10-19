#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_HPP_INCLUDED

#include "drm_plane_framebuffer.hpp"
#include "drm_plane_property.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <cstdint>
#include <optional>
#include <xf86drmMode.h>

struct drm_plane
{
    drm_plane() noexcept;

    explicit drm_plane(int device, std::uint32_t id) noexcept;

    drm_plane(drm_plane&& other) noexcept;

    drm_plane(drm_plane const& other) noexcept;

    ~drm_plane();

    friend auto swap(drm_plane& lhs, drm_plane& rhs) noexcept -> void;

    auto operator=(drm_plane rhs) noexcept -> drm_plane&;

    operator bool() const noexcept;

    auto operator->() const noexcept -> drmModePlane const*;

    auto operator*() const noexcept -> drmModePlane const&;

    friend auto operator==(drm_plane const& lhs, drm_plane const& rhs) noexcept
        -> bool;

    template <typename F>
    auto for_each_property(F f) const noexcept -> void;

    template <typename F>
    auto find_property(F f) const noexcept -> std::optional<drm_plane_property>;

    auto is_cursor_plane() const noexcept -> bool;

    auto is_primary_plane() const noexcept -> bool;

    auto framebuffer() const noexcept -> drm_plane_framebuffer;

private:
    int drm_fd_;
    std::uint32_t id_;
    drmModePlanePtr plane_ { nullptr };
};

template <typename F>
auto drm_plane::for_each_property(F f) const noexcept -> void
{
    SC_EXPECT(*this);
    auto const props =
        drmModeObjectGetProperties(drm_fd_, id_, DRM_MODE_OBJECT_PLANE);

    if (!props)
        return;

    SC_SCOPE_GUARD([&] { drmModeFreeObjectProperties(props); });

    auto pos = props->props;
    auto value_pos = props->prop_values;
    auto const last = props->props + props->count_props;

    for (; pos != last; ++pos) {
        f(drm_plane_property(drm_fd_, *pos, *value_pos++));
    }
}

template <typename F>
auto drm_plane::find_property(F f) const noexcept
    -> std::optional<drm_plane_property>
{
    std::optional<drm_plane_property> result;

    for_each_property([&](drm_plane_property prop) {
        if (!result && f(prop)) {
            result.emplace(std::move(prop));
        }
    });

    return result;
}

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_HPP_INCLUDED
