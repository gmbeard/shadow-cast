#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_DEVICE_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_DEVICE_HPP_INCLUDED

#include "drm_planes.hpp"
#include <drm.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <xf86drm.h>

struct drm_device
{
    static int constexpr kNoDevice = -1;

    explicit drm_device(std::string const& path) noexcept;

    drm_device(drm_device&& other) noexcept;

    ~drm_device();

    operator bool() const noexcept;

    auto native_handle() const noexcept -> int const&;

    explicit operator int() const noexcept;

    friend auto swap(drm_device& lhs, drm_device& rhs) noexcept -> void;

    auto operator=(drm_device&& rhs) noexcept -> drm_device&;

    friend auto operator==(drm_device const& lhs,
                           drm_device const& rhs) noexcept -> bool;

    auto planes() const noexcept -> drm_planes;

private:
    int fd_;
};

auto operator!=(drm_device const& lhs, drm_device const& rhs) noexcept -> bool;

auto operator==(int lhs, drm_device const& rhs) noexcept -> bool;

auto operator!=(int lhs, drm_device const& rhs) noexcept -> bool;

auto operator==(drm_device const& lhs, int rhs) noexcept -> bool;

auto operator!=(drm_device const& lhs, int rhs) noexcept -> bool;

auto open_drm_device() -> std::optional<drm_device>;

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_DEVICE_HPP_INCLUDED
