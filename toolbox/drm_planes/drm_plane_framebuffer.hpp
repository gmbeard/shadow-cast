#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_FRAMEBUFFER_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_FRAMEBUFFER_HPP_INCLUDED

#include "dmabuf_handle.hpp"
#include <cstdint>
#include <utility>
#include <xf86drmMode.h>

struct drm_plane;

struct drm_plane_framebuffer
{
    using value_type = drmModeFB2;
    using pointer = value_type*;
    using const_pointer = value_type const*;

    drm_plane_framebuffer(int drm_fd, std::uint32_t fb_id) noexcept;
    drm_plane_framebuffer(int drm_fd, drm_plane const& plane) noexcept;
    drm_plane_framebuffer(drm_plane_framebuffer&&) noexcept;
    ~drm_plane_framebuffer();

    auto operator=(drm_plane_framebuffer) noexcept -> drm_plane_framebuffer&;
    friend auto swap(drm_plane_framebuffer&, drm_plane_framebuffer&) noexcept
        -> void;

    operator bool() const noexcept;
    auto operator->() const noexcept -> const_pointer;
    auto get_dmabuf_handle(std::size_t n = 0) const noexcept
        -> std::pair<bool, dmabuf_handle>;

private:
    int drm_fd_;
    std::uint32_t fb_id_;
    pointer frame_buffer_;
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANE_FRAMEBUFFER_HPP_INCLUDED
