#include "av/sample_format.hpp"
#include <algorithm>

namespace sc
{
auto find_supported_formats(sc::BorrowedPtr<AVCodec const> codec)
    -> std::vector<SampleFormat>
{
    std::vector<SampleFormat> results;
    for (auto supported_fmt = codec->sample_fmts;
         supported_fmt && *supported_fmt >= 0;
         ++supported_fmt) {
        results.push_back(convert_from_libav_format(*supported_fmt));
    }

    std::sort(rbegin(results), rend(results));

    return results;
}

auto find_supported_sample_rates(sc::BorrowedPtr<AVCodec const> codec) noexcept
    -> std::span<int const>
{
    auto const* p = codec->supported_samplerates;

    if (!p)
        return {};

    auto const* end = p;
    while (end && *end)
        end++;

    return { p, static_cast<std::size_t>(end - p) };
}

auto is_sample_rate_supported(std::uint32_t requested,
                              sc::BorrowedPtr<AVCodec const> codec) noexcept
    -> bool
{
    auto supported = find_supported_sample_rates(codec);

    auto const pos = std::find(supported.begin(), supported.end(), requested);
    if (!supported.size() || pos != supported.end())
        return true;

    return false;
}

} // namespace sc
