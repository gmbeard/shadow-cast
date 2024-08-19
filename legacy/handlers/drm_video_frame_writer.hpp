#ifndef SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "nvidia.hpp"
#include "services/encoder.hpp"
#include <cstdint>

namespace sc
{
struct DRMVideoFrameWriter
{
    DRMVideoFrameWriter(AVCodecContext* codec_context,
                        AVStream* stream,
                        Encoder encoder);

    auto operator()(CUarray, NvCuda const&, std::uint64_t) -> void;

private:
    BorrowedPtr<AVCodecContext> codec_context_;
    BorrowedPtr<AVStream> stream_;
    Encoder encoder_;
    std::size_t frame_number_ { 0 };
};

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED
