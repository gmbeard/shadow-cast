#include "drm_planes.hpp"
#include "drm_device.hpp"
#include <utility>

drm_planes::drm_planes(drm_planes&& other) noexcept
    : planes_ { std::exchange(other.planes_, nullptr) }
{
}

drm_planes::~drm_planes()
{
    if (planes_) {
        drmModeFreePlaneResources(planes_);
    }
}

auto swap(drm_planes& lhs, drm_planes& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.planes_, rhs.planes_);
}

auto drm_planes::operator=(drm_planes&& rhs) noexcept -> drm_planes&
{
    drm_planes tmp { std::move(rhs) };
    swap(*this, tmp);
    return *this;
}

drm_planes::operator bool() const noexcept
{
    return planes_ != nullptr;
}

auto drm_planes::size() const noexcept -> std::size_t
{
    SC_EXPECT(planes_ != nullptr);
    return static_cast<std::size_t>(planes_->count_planes);
}

drm_planes::drm_planes_iterator::drm_planes_iterator(
    drm_planes const* p) noexcept
    : parent { p }
    , index { 0 }
    , current { p->drm_fd_, p->planes_->planes[0] }
{
}

drm_planes::drm_planes_iterator::drm_planes_iterator(drm_planes const* p,
                                                     end_iterator) noexcept
    : parent { p }
    , index { 0 }
    , current {}
{
}

auto drm_planes::drm_planes_iterator::operator*() const noexcept
    -> drm_plane const&
{
    SC_EXPECT(index < parent->size());
    return current;
}

auto drm_planes::drm_planes_iterator::operator++() noexcept
    -> drm_planes_iterator&
{
    SC_EXPECT(index < parent->size());
    index += 1;
    if (index < parent->size()) {
        current = drm_plane { parent->drm_fd_, parent->planes_->planes[index] };
    }
    else {
        current = drm_plane {};
    }

    return *this;
}

auto drm_planes::drm_planes_iterator::operator++(int) noexcept
    -> drm_planes_iterator
{
    drm_planes_iterator tmp { *this };
    ++(*this);
    return tmp;
}

auto operator==(drm_planes::drm_planes_iterator const& lhs,
                drm_planes::drm_planes_iterator const& rhs) noexcept -> bool
{
    SC_EXPECT(lhs.parent == rhs.parent);
    return lhs.index == rhs.index;
}

auto operator!=(drm_planes::drm_planes_iterator const& lhs,
                drm_planes::drm_planes_iterator const& rhs) noexcept -> bool
{
    return !(lhs.current == rhs.current);
}

auto drm_planes::begin() const noexcept -> drm_planes_iterator
{
    return drm_planes_iterator { this };
}

auto drm_planes::end() const noexcept -> drm_planes_iterator
{
    return drm_planes_iterator { this, drm_planes_iterator::end_iterator {} };
}

drm_planes::drm_planes(drm_device const& device) noexcept
    : drm_fd_ { device.native_handle() }
    , planes_ { drmModeGetPlaneResources(drm_fd_) }
{
}
