#include "metrics/formatting.hpp"
#include "metrics/histogram.hpp"
#include "testing.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>

using sc::metrics::Histogram;

namespace
{
template <typename T, std::size_t N, T I>
auto count_histogram_samples(Histogram<T, N, I> const& data) noexcept -> T
{
    return std::accumulate(
        std::begin(data), std::end(data), T {}, [](auto&& a, auto&& b) {
            return a + std::get<1>(b);
        });
}

} // namespace

auto should_construct_empty() -> void
{
    /* NOTE:
     *  This creates a histogram with `[1, 2, 3, 4, 5]`, which is
     *  effectively `[ 0<1, 1<2, 2<3, 3<4, 4<5 ]`...
     */
    Histogram<std::uint32_t, 5, 1> histogram;

    EXPECT(count_histogram_samples(histogram) == 0);
}

auto should_add_value() -> void
{
    Histogram<std::uint32_t, 5, 1> histogram;

    histogram.add_value(3);

    std::for_each(histogram.begin(), histogram.end(), [](auto&& val) {
        std::cerr << std::get<0>(val) << ": " << std::get<1>(val) << '\n';
    });

    EXPECT(std::get<1>(histogram[2]) == 1);
}

auto should_add_multiple_values() -> void
{
    Histogram<std::uint32_t, 5, 1> histogram;

    histogram.add_value(3);
    histogram.add_value(3);
    histogram.add_value(3);
    histogram.add_value(3);

    histogram.add_value(10);

    std::for_each(histogram.begin(), histogram.end(), [](auto&& val) {
        std::cerr << std::get<0>(val) << ": " << std::get<1>(val) << '\n';
    });

    EXPECT(std::get<1>(histogram[2]) == 4);
    EXPECT(std::get<1>(histogram[4]) == 1);
}

auto should_contain_the_correc_total_samples() -> void
{
    Histogram<std::uint32_t, 5, 1> histogram;

    constexpr std::size_t N = 10;

    EXPECT(count_histogram_samples(histogram) == 0);

    for ([[maybe_unused]] auto&& _ : std::array<int, N> {}) {
        histogram.add_value(42);
    }

    std::for_each(histogram.begin(), histogram.end(), [](auto&& val) {
        std::cerr << std::get<0>(val) << ": " << std::get<1>(val) << '\n';
    });

    EXPECT(count_histogram_samples(histogram) == N);
}

auto should_print_histogram() -> void
{
    Histogram<std::uint32_t, 5, 1> histogram;

    sc::metrics::format_histogram(
        std::cerr, histogram, "Frame-time Nanoseconds", "Test Histogram");

    EXPECT(false);
}

auto main() -> int
{
    return testing::run({ TEST(should_add_value),
                          TEST(should_add_multiple_values),
                          TEST(should_construct_empty),
                          TEST(should_contain_the_correc_total_samples),
                          TEST(should_print_histogram) });
}
