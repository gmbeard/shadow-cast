#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_PROPERTY_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_PROPERTY_HPP_INCLUDED

#include <cstdint>
#include <string_view>
#include <xf86drmMode.h>

struct drm_plane_property
{
    drm_plane_property() noexcept;

    drm_plane_property(int drm_fd,
                       std::uint32_t id,
                       std::uint64_t value) noexcept;

    drm_plane_property(drm_plane_property const& other) noexcept;

    drm_plane_property(drm_plane_property&& other) noexcept;

    ~drm_plane_property();

    friend auto swap(drm_plane_property& lhs, drm_plane_property& rhs) noexcept
        -> void;

    operator bool() const noexcept;

    auto operator=(drm_plane_property rhs) noexcept -> drm_plane_property&;

    auto operator*() const noexcept -> drmModePropertyRes const&;

    auto operator->() const noexcept -> drmModePropertyRes const*;

    auto name() const noexcept -> std::string_view;

    auto value() const noexcept -> std::uint64_t const&;

    auto type() const noexcept -> std::uint32_t;

    friend auto operator==(drm_plane_property const& lhs,
                           drm_plane_property const& rhs) noexcept -> bool;

private:
    int drm_fd_;
    std::uint32_t id_;
    std::uint64_t value_;
    drmModePropertyPtr prop_;
};

auto operator!=(drm_plane_property const& lhs,
                drm_plane_property const& rhs) noexcept -> bool;

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_PROPERTY_HPP_INCLUDED
