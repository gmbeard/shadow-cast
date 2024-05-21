#ifndef SHADOW_CAST_UTILS_FRAME_TIME_HPP_INCLUDED
#define SHADOW_CAST_UTILS_FRAME_TIME_HPP_INCLUDED

#include <cstdint>
#include <libavutil/rational.h>

namespace sc
{
struct FrameTime
{
    explicit FrameTime(std::uint64_t) noexcept;
    auto value() const noexcept -> std::uint64_t;
    auto value_in_milliseconds(bool round_up = false) const noexcept
        -> std::uint64_t;
    auto fps() const noexcept -> float;
    auto fps_ratio() const noexcept -> AVRational;

private:
    std::uint64_t frame_time_nanoseconds_;
};

auto truncate_to_millisecond(FrameTime const&) noexcept -> FrameTime;
auto from_fps(std::uint32_t) noexcept -> FrameTime;

} // namespace sc

#endif // SHADOW_CAST_UTILS_FRAME_TIME_HPP_INCLUDED
