#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANES_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANES_HPP_INCLUDED

#include "drm_plane.hpp"

struct drm_device;

struct drm_planes
{
    friend struct drm_device;

    drm_planes(drm_planes&& other) noexcept;

    ~drm_planes();

    friend auto swap(drm_planes& lhs, drm_planes& rhs) noexcept -> void;

    auto operator=(drm_planes&& rhs) noexcept -> drm_planes&;

    operator bool() const noexcept;

    auto size() const noexcept -> std::size_t;

    struct drm_planes_iterator
    {
        using value_type = drm_plane;
        using different_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        drm_planes const* parent { nullptr };
        std::size_t index { 0 };
        drm_plane current;

        drm_planes_iterator() = default;

        struct end_iterator
        {
        };

        explicit drm_planes_iterator(drm_planes const* p) noexcept;

        explicit drm_planes_iterator(drm_planes const* p,
                                     end_iterator) noexcept;

        auto operator*() const noexcept -> drm_plane const&;

        auto operator++() noexcept -> drm_planes_iterator&;

        auto operator++(int) noexcept -> drm_planes_iterator;

        friend auto operator==(drm_planes_iterator const& lhs,
                               drm_planes_iterator const& rhs) noexcept -> bool;

        friend auto operator!=(drm_planes_iterator const& lhs,
                               drm_planes_iterator const& rhs) noexcept -> bool;
    };

    auto begin() const noexcept -> drm_planes_iterator;

    auto end() const noexcept -> drm_planes_iterator;

    using iterator = drm_planes_iterator;
    using const_iterator = drm_planes_iterator;
    using value_type = iterator::value_type;
    using reference = value_type&;
    using const_reference = value_type const&;

private:
    friend struct drm_planes_iterator;

    explicit drm_planes(drm_device const& device) noexcept;

    int drm_fd_;
    drmModePlaneResPtr planes_;
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DRM_PLANES_HPP_INCLUDED
