#ifndef SHADOW_CAST_AV_SAMPLE_FORMAT_HPP_INCLUDED
#define SHADOW_CAST_AV_SAMPLE_FORMAT_HPP_INCLUDED

#include "av/fwd.hpp"
#include "utils/borrowed_ptr.hpp"
#include <spa/param/audio/format-utils.h>
#include <span>
#include <stdexcept>
#include <vector>

namespace sc
{
enum struct SampleFormat
{
    /* Interleaved...
     */
    u8_interleaved = 1,
    s16_interleaved,
    s32_interleaved,
    float_interleaved,
    double_interleaved,
    s64_interleaved,

    /* Planar...
     */
    u8_planar,
    s16_planar,
    s32_planar,
    float_planar,
    double_planar,
    s64_planar,
};

auto constexpr is_interleaved_format(SampleFormat fmt) noexcept -> bool
{
    switch (fmt) {
    case SampleFormat::u8_interleaved:
    case SampleFormat::s16_interleaved:
    case SampleFormat::s32_interleaved:
    case SampleFormat::float_interleaved:
    case SampleFormat::double_interleaved:
    case SampleFormat::s64_interleaved:
        return true;
    case SampleFormat::u8_planar:
    case SampleFormat::s16_planar:
    case SampleFormat::s32_planar:
    case SampleFormat::float_planar:
    case SampleFormat::double_planar:
    case SampleFormat::s64_planar:
        return false;
    default:
        break;
    }
    return false;
}

auto constexpr sample_format_size(SampleFormat fmt) noexcept -> std::size_t
{
    switch (fmt) {
    case SampleFormat::u8_interleaved:
    case SampleFormat::u8_planar:
        return 1;
    case SampleFormat::s16_interleaved:
    case SampleFormat::s16_planar:
        return 2;
    case SampleFormat::s32_interleaved:
    case SampleFormat::s32_planar:
        return 4;
    case SampleFormat::float_interleaved:
    case SampleFormat::float_planar:
        return 4;
    case SampleFormat::double_interleaved:
    case SampleFormat::double_planar:
    case SampleFormat::s64_interleaved:
    case SampleFormat::s64_planar:
    default:
        break;
    }
    return 8;
}

auto constexpr is_planar_format(SampleFormat fmt) noexcept -> bool
{
    return !is_interleaved_format(fmt);
}

auto constexpr convert_to_libav_format(SampleFormat fmt) -> AVSampleFormat
{
    switch (fmt) {
    case SampleFormat::u8_interleaved:
        return AV_SAMPLE_FMT_U8;
    case SampleFormat::s16_interleaved:
        return AV_SAMPLE_FMT_S16;
    case SampleFormat::s32_interleaved:
        return AV_SAMPLE_FMT_S32;
    case SampleFormat::float_interleaved:
        return AV_SAMPLE_FMT_FLT;
    case SampleFormat::double_interleaved:
        return AV_SAMPLE_FMT_DBL;
    case SampleFormat::s64_interleaved:
        return AV_SAMPLE_FMT_S64;
    case SampleFormat::u8_planar:
        return AV_SAMPLE_FMT_U8P;
    case SampleFormat::s16_planar:
        return AV_SAMPLE_FMT_S16P;
    case SampleFormat::s32_planar:
        return AV_SAMPLE_FMT_S32P;
    case SampleFormat::float_planar:
        return AV_SAMPLE_FMT_FLTP;
    case SampleFormat::double_planar:
        return AV_SAMPLE_FMT_DBLP;
    case SampleFormat::s64_planar:
        return AV_SAMPLE_FMT_S64P;
    default:
        break;
    }

    throw std::runtime_error { "No viable conversion to AVSampleFormat" };
}

auto constexpr convert_from_libav_format(AVSampleFormat fmt) -> SampleFormat
{
    switch (fmt) {
    case AV_SAMPLE_FMT_U8:
        return SampleFormat::u8_interleaved;
    case AV_SAMPLE_FMT_S16:
        return SampleFormat::s16_interleaved;
    case AV_SAMPLE_FMT_S32:
        return SampleFormat::s32_interleaved;
    case AV_SAMPLE_FMT_FLT:
        return SampleFormat::float_interleaved;
    case AV_SAMPLE_FMT_DBL:
        return SampleFormat::double_interleaved;
    case AV_SAMPLE_FMT_S64:
        return SampleFormat::s64_interleaved;
    case AV_SAMPLE_FMT_U8P:
        return SampleFormat::u8_planar;
    case AV_SAMPLE_FMT_S16P:
        return SampleFormat::s16_planar;
    case AV_SAMPLE_FMT_S32P:
        return SampleFormat::s32_planar;
    case AV_SAMPLE_FMT_FLTP:
        return SampleFormat::float_planar;
    case AV_SAMPLE_FMT_DBLP:
        return SampleFormat::double_planar;
    case AV_SAMPLE_FMT_S64P:
        return SampleFormat::s64_planar;
    default:
        throw std::runtime_error { "Unsupported libav sample format" };
    }
}

auto constexpr convert_to_pipewire_format(SampleFormat fmt) -> spa_audio_format
{
    switch (fmt) {
    case SampleFormat::u8_interleaved:
        return SPA_AUDIO_FORMAT_U8;
    case SampleFormat::s16_interleaved:
        return SPA_AUDIO_FORMAT_S16;
    case SampleFormat::s32_interleaved:
        return SPA_AUDIO_FORMAT_S32;
    case SampleFormat::float_interleaved:
        return SPA_AUDIO_FORMAT_F32;
    case SampleFormat::double_interleaved:
        return SPA_AUDIO_FORMAT_F64;
    case SampleFormat::u8_planar:
        return SPA_AUDIO_FORMAT_U8P;
    case SampleFormat::s16_planar:
        return SPA_AUDIO_FORMAT_S16P;
    case SampleFormat::s32_planar:
        return SPA_AUDIO_FORMAT_S32P;
    case SampleFormat::float_planar:
        return SPA_AUDIO_FORMAT_F32P;
    case SampleFormat::double_planar:
        return SPA_AUDIO_FORMAT_F64P;
    case SampleFormat::s64_interleaved:
    case SampleFormat::s64_planar:
        break;
    }

    throw std::runtime_error { "No viable Pipewire sample format conversion " };
}

auto find_supported_formats(sc::BorrowedPtr<AVCodec const> codec)
    -> std::vector<SampleFormat>;

auto find_supported_sample_rates(sc::BorrowedPtr<AVCodec const> codec) noexcept
    -> std::span<int const>;

auto is_sample_rate_supported(std::uint32_t requested,
                              sc::BorrowedPtr<AVCodec const> codec) noexcept
    -> bool;
} // namespace sc
#endif // SHADOW_CAST_AV_SAMPLE_FORMAT_HPP_INCLUDED
