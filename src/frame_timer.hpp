#ifndef SHADOW_CAST_FRAME_TIMER_HPP_INCLUDED
#define SHADOW_CAST_FRAME_TIMER_HPP_INCLUDED

#include <chrono>
#include <cstddef>

namespace sc
{

struct frame_timer
{
    using clock_type = std::chrono::high_resolution_clock;

    explicit frame_timer(
        std::chrono::duration<clock_type::rep, clock_type::period> frame_time,
        std::chrono::time_point<clock_type> start_time =
            clock_type::now()) noexcept;
    auto start_time() const noexcept
        -> std::chrono::time_point<clock_type> const&;
    auto elapsed() const noexcept
        -> std::chrono::duration<clock_type::rep, clock_type::period>;
    auto reset(std::chrono::time_point<clock_type> start_time =
                   clock_type::now()) noexcept -> void;
    auto frame_rate() const noexcept -> std::size_t;
    auto frame_number() const noexcept -> std::size_t;
    auto increment_frame_number(std::ptrdiff_t n = 1) noexcept -> void;
    auto expected_frame_number_at(
        std::chrono::time_point<clock_type> const& time_point) const noexcept
        -> std::size_t;
    auto next_frame_time_from(
        std::chrono::time_point<clock_type> const& current_time) const noexcept
        -> std::chrono::time_point<clock_type>;
    auto duration_until_next_frame_from(
        std::chrono::time_point<clock_type> const& current_time) const noexcept
        -> std::chrono::duration<clock_type::rep, clock_type::period>;

    static auto now() noexcept -> std::chrono::time_point<clock_type>;

    auto operator++() noexcept -> frame_timer&;
    auto operator++(int) noexcept -> frame_timer;

private:
    std::size_t frame_time_ns_;
    std::chrono::time_point<clock_type> start_time_;
    std::size_t frame_rate_;
    std::size_t frame_number_;
};

} // namespace sc

#endif // SHADOW_CAST_FRAME_TIMER_HPP_INCLUDED
