#ifndef SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "nvidia.hpp"

namespace sc
{
struct DRMVideoFrameWriter
{
    DRMVideoFrameWriter(AVFormatContext* fmt_context,
                        AVCodecContext* codec_context,
                        AVStream* stream);

    auto operator()(CUarray, NvCuda const&) -> void;

private:
    BorrowedPtr<AVFormatContext> format_context_;
    BorrowedPtr<AVCodecContext> codec_context_;
    BorrowedPtr<AVStream> stream_;
    FramePtr frame_;
    std::size_t frame_number_ { 0 };
    PacketPtr packet_;
};

} // namespace sc

#endif // SHADOW_CAST_HANDLERS_DRM_VIDEO_FRAME_WRITER_HPP_INCLUDED
