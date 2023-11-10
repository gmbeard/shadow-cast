#ifndef SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED
#define SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED

#include "av.hpp"
#include "nvidia.hpp"

namespace sc
{
struct VideoFrameWriter
{
    VideoFrameWriter(AVFormatContext* fmt_context,
                     AVCodecContext* codec_context,
                     AVStream* stream);

    auto operator()(CUdeviceptr cu_device_ptr, NVFBC_FRAME_GRAB_INFO) -> void;

private:
    BorrowedPtr<AVFormatContext> format_context_;
    BorrowedPtr<AVCodecContext> codec_context_;
    BorrowedPtr<AVStream> stream_;
    FramePtr frame_;
    std::size_t frame_number_ { 0 };
    PacketPtr packet_;
};

} // namespace sc
#endif // SHADOW_CAST_HANDLERS_VIDEO_FRAME_WRITER_HPP_INCLUDED
