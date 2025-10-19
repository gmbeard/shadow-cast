#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_ROLLING_AVERAGE_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_ROLLING_AVERAGE_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <concepts>
#include <optional>
#include <type_traits>
#include <vector>

template <typename T>
requires(std::is_nothrow_default_constructible_v<T> &&
         std::is_nothrow_copy_constructible_v<T> &&
         requires(T& lhs, T const& rhs) {
             // clang-format off
            { lhs + rhs } noexcept -> std::convertible_to<T>;
            { lhs = rhs } -> std::convertible_to<T>;
            { lhs += rhs } noexcept -> std::convertible_to<T>;
            { lhs - rhs } noexcept -> std::convertible_to<T>;
            { lhs / std::declval<std::size_t>() } noexcept;
             // clang-format on
         })
struct rolling_average
{
    using value_type = T;

    explicit rolling_average(std::size_t window_size)
        : data_set_(window_size * 2, value_type {})
    {
    }

    auto add(T const& value) noexcept -> T
    {
        /* NOTE:
         * We want `clear()` to be constant time, so we write a value and zero
         * the "flipped" side simultaneously. This means `clear()` is just a
         * flip of a bit to switch where the data is read from / written to.
         */

        auto const write_pos = flip_ * window_size() + pos_;
        auto const clear_pos = (flip_ ^ std::size_t(1)) * window_size() + pos_;

        sum_ += value - data_set_[write_pos];
        data_set_[write_pos] = value;
        data_set_[clear_pos] = T {};
        pos_ = (pos_ + 1) % window_size();
        items_added_ = std::min(window_size(), items_added_ + 1);

        return sum_ / items_added_;
    }

    auto clear() noexcept -> void
    {
        flip_ ^= std::size_t(1);
        pos_ = items_added_ = 0;
        sum_ = T {};

        SC_EXPECT(flip_ <= 1);
    }

    [[nodiscard]] auto window_size() const noexcept -> std::size_t
    {
        return data_set_.size() / 2;
    }

    [[nodiscard]] auto value() const noexcept -> std::optional<T>
    {
        if (items_added_ < window_size())
            return std::nullopt;

        return sum_ / window_size();
    }

    [[nodiscard]] auto has_value() const noexcept -> bool
    {
        return items_added_ == window_size();
    }

    operator bool() const noexcept
    {
        return has_value();
    }

private:
    std::vector<double> data_set_;
    T sum_ {};
    std::size_t pos_ { 0 };
    std::size_t items_added_ { 0 };
    std::size_t flip_ { 0 };
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_ROLLING_AVERAGE_HPP_INCLUDED
