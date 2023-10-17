#include "handlers/video_frame_writer.hpp"
#include "handlers/audio_chunk_writer.hpp"
#include "utils/elapsed.hpp"

auto constexpr kCudaFrameAlignment = 256;
namespace sc
{

VideoFrameWriter::VideoFrameWriter(AVFormatContext* fmt_context,
                                   AVCodecContext* codec_context)
    : format_context_ { fmt_context }
    , codec_context_ { codec_context }
    , frame_ { av_frame_alloc() }
{
}

auto VideoFrameWriter::operator()(CUdeviceptr cu_device_ptr,
                                  NVFBC_FRAME_GRAB_INFO) -> void
{
    auto aligned_width = FFALIGN(codec_context_->width, kCudaFrameAlignment);

    sc::AVFrameUnrefGuard unref_guard { frame_.get() };

    frame_->hw_frames_ctx = av_buffer_ref(codec_context_->hw_frames_ctx);

    auto hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(
        codec_context_->hw_frames_ctx->data);
    frame_->buf[0] = av_buffer_pool_get(hw_frames_ctx->pool);

    /* Layout for yuv444p...
     */
    frame_->data[0] = reinterpret_cast<uint8_t*>(cu_device_ptr);
    frame_->data[1] = frame_->data[0] + aligned_width * codec_context_->height;
    frame_->data[2] = frame_->data[1] + aligned_width * codec_context_->height;
    frame_->linesize[0] = aligned_width;
    frame_->linesize[1] = aligned_width;
    frame_->linesize[2] = aligned_width;

    frame_->extended_data = frame_->data;
    frame_->format = codec_context_->pix_fmt;
    frame_->width = codec_context_->width;
    frame_->height = codec_context_->height;
    frame_->color_range = codec_context_->color_range;
    frame_->color_primaries = codec_context_->color_primaries;
    frame_->color_trc = codec_context_->color_trc;
    frame_->colorspace = codec_context_->colorspace;
    frame_->chroma_location = codec_context_->chroma_sample_location;

    frame_->pts = sc::global_elapsed.value();

    sc::send_frame(
        frame_.get(), codec_context_.get(), format_context_.get(), 1);
}

} // namespace sc
