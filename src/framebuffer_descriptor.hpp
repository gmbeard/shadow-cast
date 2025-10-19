#ifndef SHADOWCAST_TOOLBOX_FRAMEBUFFER_DESCRIPTOR_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_FRAMEBUFFER_DESCRIPTOR_HPP_INCLUDED

#include "frame_timer.hpp"
#include <atomic>
#include <chrono>

namespace sc
{
struct drm_plane_framebuffer;

template <typename T>
struct sequence_lock
{
    using value_type = T;

    std::atomic_size_t sequence { 0 };
    T value;
};

struct framebuffer_descriptor
{
    std::uint32_t fb_id { 0 };
    std::uint32_t width { 0 };
    std::uint32_t height { 0 };
    std::uint32_t pixel_format { 0 };
    std::uint64_t modifier { 0 };
    std::uint32_t flags { 0 };
    std::uint32_t pitch { 0 };
    std::uint32_t offset { 0 };
    std::chrono::time_point<frame_timer::clock_type> ts {};
};

using framebuffer_descriptor_sequence = sequence_lock<framebuffer_descriptor>;

auto write_next_sequence(framebuffer_descriptor const& source,
                         framebuffer_descriptor_sequence& target) noexcept
    -> void;

auto read_next_sequence(framebuffer_descriptor_sequence& source,
                        long timeout_milliseconds = 500) noexcept
    -> framebuffer_descriptor;
} // namespace sc
#endif // SHADOWCAST_TOOLBOX_FRAMEBUFFER_DESCRIPTOR_HPP_INCLUDED
