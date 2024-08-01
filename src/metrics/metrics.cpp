#include "metrics/metrics.hpp"

namespace
{

auto audio_histogram() noexcept -> sc::metrics::AudioFrameTimeHistogram&
{
    static sc::metrics::AudioFrameTimeHistogram histogram {};
    return histogram;
}

auto video_histogram() noexcept -> sc::metrics::VideoFrameTimeHistogram&
{
    static sc::metrics::VideoFrameTimeHistogram histogram {};
    return histogram;
}

} // namespace

namespace sc::metrics
{
auto add_frame_time(AudioMetricsTag, std::uint64_t value) noexcept -> void
{
    audio_histogram().add_value(value);
}

auto add_frame_time(VideoMetricsTag, std::uint64_t value) noexcept -> void
{
    video_histogram().add_value(value);
}

auto get_histogram(AudioMetricsTag) noexcept -> AudioFrameTimeHistogram const&
{
    return audio_histogram();
}

auto get_histogram(VideoMetricsTag) noexcept -> VideoFrameTimeHistogram const&
{
    return video_histogram();
}

} // namespace sc::metrics
