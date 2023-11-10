#ifndef SHADOW_CAST_AV_CODEC_HPP_INCLUDED
#define SHADOW_CAST_AV_CODEC_HPP_INCLUDED

#include "av/fwd.hpp"
#include "display/display.hpp"
#include "nvidia.hpp"
#include <memory>
#include <string>

namespace sc
{
struct CodecContextDeleter
{
    auto operator()(AVCodecContext* ptr) noexcept -> void;
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
auto create_video_encoder(std::string const& encoder_name,
                          CUcontext cuda_ctx,
                          AVBufferPool* pool,
                          Display* display) -> sc::CodecContextPtr;
} // namespace sc

#endif // SHADOW_CAST_AV_CODEC_HPP_INCLUDED
