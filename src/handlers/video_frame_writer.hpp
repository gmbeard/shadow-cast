#ifndef SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "nvidia.hpp"

namespace sc
{
struct VideoFrameWriter
{
    VideoFrameWriter(AVFormatContext* fmt_context,
                     AVCodecContext* codec_context);

    auto operator()(CUdeviceptr cu_device_ptr, NVFBC_FRAME_GRAB_INFO) -> void;

private:
    sc::BorrowedPtr<AVFormatContext> format_context_;
    sc::BorrowedPtr<AVCodecContext> codec_context_;
    sc::FramePtr frame_;
};

} // namespace sc
#endif // SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED
