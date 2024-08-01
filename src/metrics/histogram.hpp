#ifndef SHADOW_CAST_METRICS_HISTOGRAM_HPP_INCLUDED
#define SHADOW_CAST_METRICS_HISTOGRAM_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sc::metrics
{

// clang-format off
template<typename T>
concept HistogramBucketConcept =
    std::is_default_constructible_v<T> &&
    requires(T val) {
        val += val;
        { val < val } -> std::convertible_to<bool>;
        { val + val } -> std::convertible_to<T>;
    };
// clang-format on

template <HistogramBucketConcept T, std::size_t N, T I>
requires(N >= 1)
struct Histogram
{
    friend struct HistogramIterator;
    using value_type = std::pair<T, std::size_t>;

    struct HistogramIterator
    {
        using difference_type = std::ptrdiff_t;
        using value_type = typename Histogram::value_type;
        using category = std::output_iterator_tag;

        auto operator*() const noexcept -> value_type
        {
            return (*histogram)[n];
        }

        auto operator++() noexcept -> HistogramIterator&
        {
            n += 1;
            return *this;
        }

        auto operator++(int) noexcept -> HistogramIterator
        {
            auto tmp { *this };
            (*this).operator++();
            return tmp;
        }

        auto operator==(HistogramIterator const& lhs) noexcept -> bool
        {
            return n == lhs.n;
        }

        auto operator!=(HistogramIterator const& lhs) noexcept -> bool
        {
            return n != lhs.n;
        }

        std::size_t n;
        Histogram const* histogram;
    };

    Histogram() noexcept
    {
        T bucket_val = I;
        auto n = N;
        auto bucket_iterator = buckets_.begin();
        while (n--) {
            *bucket_iterator++ = bucket_val;
            bucket_val += I;
        }
    }

    [[nodiscard]] auto operator[](std::size_t n) const noexcept -> value_type
    {
        SC_EXPECT(n < N);
        return value_type { buckets_[n], counts_[n] };
    }

    auto add_value(T const& val) noexcept -> void
    {
        auto const n = std::distance(
            buckets_.begin(),
            std::lower_bound(
                buckets_.begin(), std::next(buckets_.begin(), N - 1), val));

        counts_[n] += 1;
    }

    [[nodiscard]] auto begin() const noexcept -> HistogramIterator
    {
        return HistogramIterator { 0, this };
    }

    [[nodiscard]] auto end() const noexcept -> HistogramIterator
    {
        return HistogramIterator { N, this };
    }

private:
    std::array<T, N> buckets_;
    std::array<std::atomic_size_t, N> counts_ {};
};

} // namespace sc::metrics
#endif // SHADOW_CAST_METRICS_HISTOGRAM_HPP_INCLUDED
