#include "./nvfbc_capture.hpp"
#include <libavcodec/avcodec.h>

namespace sc
{

auto fill_frame(NvFbcGpu const&,
                CUdeviceptr value,
                AVCodecContext* codec,
                AVFrame* frame,
                std::size_t frame_number) -> void
{
    frame->hw_frames_ctx = av_buffer_ref(codec->hw_frames_ctx);

    auto hw_frames_ctx =
        reinterpret_cast<AVHWFramesContext*>(codec->hw_frames_ctx->data);
    frame->buf[0] = av_buffer_pool_get(hw_frames_ctx->pool);

    frame->data[0] = reinterpret_cast<uint8_t*>(value);
    frame->linesize[0] = codec->width * sizeof(std::uint32_t);

    frame->extended_data = frame->data;
    frame->format = codec->pix_fmt;
    frame->width = codec->width;
    frame->height = codec->height;
    frame->color_range = codec->color_range;
    frame->color_primaries = codec->color_primaries;
    frame->color_trc = codec->color_trc;
    frame->colorspace = codec->colorspace;
    frame->chroma_location = codec->chroma_sample_location;

    frame->pts = frame_number;
}

} // namespace sc
