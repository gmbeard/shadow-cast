#include "utils/frame_time.hpp"

namespace
{
std::uint64_t constexpr kNsPerSec = 1'000'000'000;
std::uint64_t constexpr kNsPerMs = 1'000'000;
auto constexpr kRound = kNsPerMs / 2;

} // namespace
namespace sc
{

FrameTime::FrameTime(std::uint64_t frame_time_nanoseconds) noexcept
    : frame_time_nanoseconds_ { frame_time_nanoseconds }
{
}

auto FrameTime::value() const noexcept -> std::uint64_t
{
    return frame_time_nanoseconds_;
}

auto FrameTime::value_in_milliseconds(bool round_up) const noexcept
    -> std::uint64_t
{
    return (frame_time_nanoseconds_ + (round_up ? kRound : 0)) / kNsPerMs;
}

auto FrameTime::fps() const noexcept -> float
{
    return static_cast<float>(kNsPerSec) / frame_time_nanoseconds_;
}

auto FrameTime::fps_ratio() const noexcept -> AVRational
{
    return av_make_q(1, static_cast<int>(fps()));
}

auto truncate_to_millisecond(FrameTime const& ft) noexcept -> FrameTime
{
    auto const in_ms = ft.value() / kNsPerMs;
    return FrameTime { in_ms * kNsPerMs };
}
auto from_fps(std::uint32_t fps) noexcept -> FrameTime
{
    return FrameTime { kNsPerSec / fps };
}

} // namespace sc
