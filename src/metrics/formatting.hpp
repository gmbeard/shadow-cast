#ifndef SHADOW_CAST_METRICS_FORMATTING_HPP_INCLUDED
#define SHADOW_CAST_METRICS_FORMATTING_HPP_INCLUDED

#include "metrics/histogram.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <string_view>

namespace sc::metrics
{

template <typename CharT, typename Traits, typename T, std::size_t N, T I>
auto format_histogram_rows(std::basic_ostream<CharT, Traits>& os,
                           Histogram<T, N, I> const& data,
                           std::size_t column_size) -> void
{
    std::for_each(
        std::begin(data), std::end(data), [&](auto const& data_point) {
            os << "| ";
            os.width(column_size);
            os << std::get<0>(data_point);
            os << " | ";

            os.width(column_size);
            os << std::get<1>(data_point) << " |\n";
        });
}

template <typename CharT, typename Traits, typename T, std::size_t N, T I>
auto format_histogram(std::basic_ostream<CharT, Traits>& os,
                      Histogram<T, N, I> const& data,
                      std::string_view series_name,
                      std::string_view title = "") -> void
{
    if (title.size()) {
        os << title << '\n';
    }

    constexpr std::size_t kColumnMargin = 1;
    auto const column_width = std::max(series_name.size(), 10ul);

    os << "| ";
    os.width(column_width);
    os << series_name << " | ";
    os.width(column_width);

    os << "Frequency"
       << " |\n";

    os.put('|').fill('-');

    os.width(column_width + kColumnMargin * 2);
    os << '-' << '|';
    os.width(column_width + kColumnMargin * 2);
    os << '-' << '|';
    os.put('\n').fill(' ');

    format_histogram_rows(os, data, column_width);
}

template <typename CharT, typename Traits, typename T, std::size_t N, T I>
auto format_histogram(std::basic_ostream<CharT, Traits>& os,
                      Histogram<T, N, I> const& data) -> void
{
    format_histogram(os, data, "Buckets");
}
} // namespace sc::metrics

#endif // SHADOW_CAST_METRICS_FORMATTING_HPP_INCLUDED
