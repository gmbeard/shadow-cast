#ifndef SHADOW_CAST_CODEC_HPP_INCLUDED
#define SHADOW_CAST_CODEC_HPP_INCLUDED

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace sc
{
struct CodecContextDeleter
{
    auto operator()(AVCodecContext* ptr) noexcept -> void;
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
} // namespace sc

#endif // SHADOW_CAST_CODEC_HPP_INCLUDED
