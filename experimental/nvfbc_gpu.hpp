#ifndef SHADOW_CAST_NVFBC_GPU_HPP_INCLUDED
#define SHADOW_CAST_NVFBC_GPU_HPP_INCLUDED

#include "av/buffer_pool.hpp"
#include "av/codec.hpp"
#include "nvidia.hpp"

namespace sc
{

struct NvFbcGpu
{
    NvFbcGpu(FrameTime const& frame_time,
             VideoOutputSize dimensions,
             std::optional<CaptureResolution> const& override_resolution);

    auto
    create_encoder(std::string const& codec_name,
                   VideoOutputSize const& dimensions,
                   std::optional<CaptureResolution> const& override_dimensions,
                   std::size_t bitrate,
                   CaptureQuality const& quality) -> CodecContextPtr;

    auto cuda_context() const noexcept -> CUcontextPtr::pointer;

    auto nvfbc_session() const noexcept -> NvFBCSessionHandlePtr::pointer;

    auto frame_time() const noexcept -> FrameTime const&;

private:
    CUcontextPtr cuda_ctx_;
    NvFBCSessionHandlePtr session_;
    FrameTime frame_time_;
    BufferPoolPtr buffer_pool_;
};

} // namespace sc

#endif // SHADOW_CAST_NVFBC_GPU_HPP_INCLUDED
