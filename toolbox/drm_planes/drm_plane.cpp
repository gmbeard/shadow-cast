#include "drm_plane.hpp"
#include "drm_device.hpp"
#include "drm_plane_framebuffer.hpp"
#include <utility>

drm_plane::drm_plane() noexcept
    : drm_fd_ { drm_device::kNoDevice }
    , id_ { static_cast<std::uint32_t>(-1) }
    , plane_ { nullptr }
{
}

drm_plane::drm_plane(int device, std::uint32_t id) noexcept
    : drm_fd_ { device }
    , id_ { id }
    , plane_ { drmModeGetPlane(drm_fd_, id_) }
{
    SC_EXPECT(drm_fd_ != drm_device::kNoDevice);
}

drm_plane::drm_plane(drm_plane&& other) noexcept
    : drm_fd_ { other.drm_fd_ }
    , id_ { other.id_ }
    , plane_ { std::exchange(other.plane_, nullptr) }
{
}

drm_plane::drm_plane(drm_plane const& other) noexcept
    : drm_fd_ { other.drm_fd_ }
    , id_ { other.id_ }
    , plane_ { nullptr }
{
    if (drm_fd_ != drm_device::kNoDevice) {
        plane_ = drmModeGetPlane(drm_fd_, id_);
    }
}

drm_plane::~drm_plane()
{
    if (plane_) {
        drmModeFreePlane(plane_);
    }
}

auto swap(drm_plane& lhs, drm_plane& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.drm_fd_, rhs.drm_fd_);
    swap(lhs.id_, rhs.id_);
    swap(lhs.plane_, rhs.plane_);
}

auto drm_plane::operator=(drm_plane rhs) noexcept -> drm_plane&
{
    swap(*this, rhs);
    return *this;
}

drm_plane::operator bool() const noexcept
{
    return plane_ != nullptr;
}

auto drm_plane::operator->() const noexcept -> drmModePlane const*
{
    SC_EXPECT(plane_ != nullptr);
    return plane_;
}

auto drm_plane::operator*() const noexcept -> drmModePlane const&
{
    SC_EXPECT(plane_ != nullptr);
    return *plane_;
}

auto operator==(drm_plane const& lhs, drm_plane const& rhs) noexcept -> bool
{
    return lhs.id_ == rhs.id_;
}

auto drm_plane::is_cursor_plane() const noexcept -> bool
{
    auto const type_prop = find_property(
        [](drm_plane_property const& prop) { return prop.name() == "type"; });

    if (!type_prop) {
        return false;
    }

    return type_prop->value() == DRM_PLANE_TYPE_CURSOR;
}

auto drm_plane::is_primary_plane() const noexcept -> bool
{
    auto const type_prop = find_property(
        [](drm_plane_property const& prop) { return prop.name() == "type"; });

    if (!type_prop) {
        return false;
    }

    return type_prop->value() == DRM_PLANE_TYPE_PRIMARY;
}

auto drm_plane::framebuffer() const noexcept -> drm_plane_framebuffer
{
    return drm_plane_framebuffer(drm_fd_, *this);
}
