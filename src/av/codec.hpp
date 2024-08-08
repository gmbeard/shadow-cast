#ifndef SHADOW_CAST_AV_CODEC_HPP_INCLUDED
#define SHADOW_CAST_AV_CODEC_HPP_INCLUDED

#include "av/fwd.hpp"
#include "display/display.hpp"
#include "nvidia.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace sc
{
struct CodecContextDeleter
{
    auto operator()(AVCodecContext* ptr) noexcept -> void;
};

template <typename T>
struct VideoOutputSizeBase
{
    T width;
    T height;
};

using VideoOutputSize = VideoOutputSizeBase<std::uint32_t>;
using VideoOutputScale = VideoOutputSizeBase<float>;

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
auto create_video_encoder(std::string const& encoder_name,
                          CUcontext cuda_ctx,
                          AVBufferPool* pool,
                          VideoOutputSize size,
                          FrameTime const& ft,
                          AVPixelFormat pixel_format) -> sc::CodecContextPtr;
} // namespace sc

#endif // SHADOW_CAST_AV_CODEC_HPP_INCLUDED
