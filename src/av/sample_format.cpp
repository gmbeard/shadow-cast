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

auto sample_format_name(SampleFormat fmt) noexcept -> char const*
{
    switch (fmt) {
    case SampleFormat::u8_interleaved:
        return "u8_interleaved";
    case SampleFormat::s16_interleaved:
        return "s16_interleaved";
    case SampleFormat::s32_interleaved:
        return "s32_interleaved";
    case SampleFormat::float_interleaved:
        return "float_interleaved";
    case SampleFormat::double_interleaved:
        return "double_interleaved";
    case SampleFormat::u8_planar:
        return "u8_planar";
    case SampleFormat::s16_planar:
        return "s16_planar";
    case SampleFormat::s32_planar:
        return "s32_planar";
    case SampleFormat::float_planar:
        return "float_planar";
    case SampleFormat::double_planar:
        return "double_planar";
    case SampleFormat::s64_interleaved:
        return "s64_interleaved";
    case SampleFormat::s64_planar:
        return "s64_planar";
    default:
        return "unknown";
    }
}

} // namespace sc
