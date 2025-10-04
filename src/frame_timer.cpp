#include "frame_timer.hpp"
#include "utils/contracts.hpp"
#include <chrono>
#include <cstddef>

namespace
{

constexpr auto
to_nanoseconds(std::chrono::duration<sc::frame_timer::clock_type::rep,
                                     sc::frame_timer::clock_type::period>
                   duration) noexcept -> std::size_t
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
        .count();
}

constexpr auto kNsPerSecond = to_nanoseconds(std::chrono::seconds(1));

} // namespace

namespace sc
{
frame_timer::frame_timer(
    std::chrono::duration<clock_type::rep, clock_type::period> frame_time,
    std::chrono::time_point<clock_type> start_time) noexcept
    : frame_time_ns_(to_nanoseconds(frame_time))
    , start_time_(start_time)
    , frame_rate_(kNsPerSecond / frame_time_ns_)
    , frame_number_(0)
{
}

auto frame_timer::reset(std::chrono::time_point<clock_type> start_time) noexcept
    -> void
{
    start_time_ = start_time;
    frame_number_ = 0;
}

auto frame_timer::frame_rate() const noexcept -> std::size_t
{
    return frame_rate_;
}

auto frame_timer::frame_number() const noexcept -> std::size_t
{
    return frame_number_;
}

auto frame_timer::increment_frame_number(std::ptrdiff_t n) noexcept -> void
{
    frame_number_ += n;
}

auto frame_timer::expected_frame_number_at(
    std::chrono::time_point<clock_type> const& time_point) const noexcept
    -> std::size_t
{
    SC_EXPECT(time_point >= start_time_);
    const std::size_t diff_ns = to_nanoseconds(time_point - start_time_);
    return diff_ns / frame_time_ns_;
}

auto frame_timer::next_frame_time_from(
    std::chrono::time_point<clock_type> const& current_time) const noexcept
    -> std::chrono::time_point<clock_type>
{
    SC_EXPECT(current_time >= start_time_);
    const std::size_t diff_ns =
        to_nanoseconds(current_time - start_time_) + frame_time_ns_ - 1;
    const std::size_t frames = diff_ns / frame_time_ns_;

    return start_time_ + std::chrono::nanoseconds(frames * frame_time_ns_);
}

auto frame_timer::duration_until_next_frame_from(
    std::chrono::time_point<clock_type> const& current_time) const noexcept
    -> std::chrono::duration<clock_type::rep, clock_type::period>
{
    auto const next_frame_time = next_frame_time_from(current_time);
    return next_frame_time - current_time;
}

auto frame_timer::now() noexcept -> std::chrono::time_point<clock_type>
{
    return clock_type::now();
}

auto frame_timer::operator++() noexcept -> frame_timer&
{
    increment_frame_number();
    return *this;
}

auto frame_timer::operator++(int) noexcept -> frame_timer
{
    auto tmp = *this;
    this->operator++();
    return tmp;
}

} // namespace sc
