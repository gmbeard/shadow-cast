#include "drm_plane_property.hpp"
#include "drm_device.hpp"
#include "utils/contracts.hpp"
#include <utility>

drm_plane_property::drm_plane_property() noexcept
    : drm_fd_ { drm_device::kNoDevice }
    , id_ { static_cast<std::uint32_t>(-1) }
    , value_ { 0 }
    , prop_ { nullptr }
{
}

drm_plane_property::drm_plane_property(int drm_fd,
                                       std::uint32_t id,
                                       std::uint64_t value) noexcept
    : drm_fd_ { drm_fd }
    , id_ { id }
    , value_ { value }
    , prop_ { drmModeGetProperty(drm_fd_, id_) }
{
    SC_EXPECT(drm_fd_ != drm_device::kNoDevice);
}

drm_plane_property::drm_plane_property(drm_plane_property const& other) noexcept
    : drm_fd_ { other.drm_fd_ }
    , id_ { other.id_ }
    , value_ { other.value_ }
    , prop_ { nullptr }
{
    if (drm_fd_ != drm_device::kNoDevice) {
        prop_ = drmModeGetProperty(drm_fd_, id_);
    }
}

drm_plane_property::drm_plane_property(drm_plane_property&& other) noexcept
    : drm_fd_ { other.drm_fd_ }
    , id_ { other.id_ }
    , value_ { other.value_ }
    , prop_ { std::exchange(other.prop_, nullptr) }
{
}

drm_plane_property::~drm_plane_property()
{
    if (prop_) {
        drmModeFreeProperty(prop_);
    }
}

auto swap(drm_plane_property& lhs, drm_plane_property& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.drm_fd_, rhs.drm_fd_);
    swap(lhs.id_, rhs.id_);
    swap(lhs.value_, rhs.value_);
    swap(lhs.prop_, rhs.prop_);
}

drm_plane_property::operator bool() const noexcept
{
    return prop_ != nullptr;
}

auto drm_plane_property::operator=(drm_plane_property rhs) noexcept
    -> drm_plane_property&
{
    swap(*this, rhs);
    return *this;
}

auto drm_plane_property::operator*() const noexcept -> drmModePropertyRes const&
{
    SC_EXPECT(prop_);
    return *prop_;
}

auto drm_plane_property::operator->() const noexcept
    -> drmModePropertyRes const*
{
    SC_EXPECT(prop_);
    return prop_;
}

auto drm_plane_property::name() const noexcept -> std::string_view
{
    return std::string_view { prop_->name };
}

auto drm_plane_property::value() const noexcept -> std::uint64_t const&
{
    return value_;
}

auto drm_plane_property::type() const noexcept -> std::uint32_t
{
    SC_EXPECT(prop_);
    return drmModeGetPropertyType(prop_);
}

auto operator==(drm_plane_property const& lhs,
                drm_plane_property const& rhs) noexcept -> bool
{
    return lhs.id_ == rhs.id_;
}

auto operator!=(drm_plane const& lhs, drm_plane const& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}
