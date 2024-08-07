#ifndef SHADOW_CAST_METRICS_METRICS_HPP_INCLUDED
#define SHADOW_CAST_METRICS_METRICS_HPP_INCLUDED

#include "./histogram.hpp"
#include <chrono>
#include <cstdint>

namespace sc::metrics
{

// clang-format off
struct AudioMetricsTag { };
struct VideoMetricsTag { };
// clang-format on

constexpr AudioMetricsTag audio_metrics {};
constexpr VideoMetricsTag video_metrics {};

constexpr std::uint64_t kBucketSize =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(1))
        .count() *
    0.5;

constexpr std::size_t kBucketCount = 20;

using AudioFrameTimeHistogram =
    Histogram<std::uint64_t, kBucketCount, kBucketSize>;
using VideoFrameTimeHistogram =
    Histogram<std::uint64_t, kBucketCount, kBucketSize>;

auto add_frame_time(AudioMetricsTag, std::uint64_t value) noexcept -> void;
auto add_frame_time(VideoMetricsTag, std::uint64_t value) noexcept -> void;
[[nodiscard]] auto get_histogram(AudioMetricsTag) noexcept
    -> AudioFrameTimeHistogram const&;
[[nodiscard]] auto get_histogram(VideoMetricsTag) noexcept
    -> VideoFrameTimeHistogram const&;

} // namespace sc::metrics

#endif // SHADOW_CAST_METRICS_METRICS_HPP_INCLUDED
