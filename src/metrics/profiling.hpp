#ifndef SHADOW_CAST_METRICS_PROFILING_HPP_INCLUDED
#define SHADOW_CAST_METRICS_PROFILING_HPP_INCLUDED

#include "config.hpp"
#include "utils/elapsed.hpp"
#include <cstdint>
#include <unordered_map>

namespace sc::metrics
{

struct ProfileSection
{
    std::uint64_t sample_count;
    std::uint64_t average_duration;
    std::uint64_t highest_duration;
    std::uint64_t lowest_duration;
};

enum class ProfileSectionId : std::size_t
{
    wayland_fetch_drm_data = 1,
    opengl_color_conversion,
    cuda_copy_frame,

    END_OF_PROFILE_SECTION_IDS,
};

auto get_profile_section_name(ProfileSectionId id) noexcept -> char const*;

auto get_profile_table() noexcept
    -> std::unordered_map<ProfileSectionId, ProfileSection> const&;

auto add_profile_sample(ProfileSectionId id, std::uint64_t time) -> void;

template <typename F>
decltype(auto) with_profile(ProfileSectionId id, F fn) noexcept(noexcept(fn()))
{
    sc::Elapsed timer;
    if constexpr (std::is_same_v<decltype(fn()), void>) {
        fn();
        add_profile_sample(id, timer.nanosecond_value());
        return;
    }
    else {
        auto result = fn();
        add_profile_sample(id, timer.nanosecond_value());
        return result;
    }
}

#ifdef SHADOW_CAST_ENABLE_PROFILING
#define WITH_PROFILE(id, fn) ::sc::metrics::with_profile(id, fn)
#else
#define WITH_PROFILE(id, fn) fn()
#endif

} // namespace sc::metrics

#endif // SHADOW_CAST_METRICS_PROFILING_HPP_INCLUDED
