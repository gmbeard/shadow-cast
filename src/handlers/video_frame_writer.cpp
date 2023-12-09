#include "handlers/video_frame_writer.hpp"
#include "handlers/audio_chunk_writer.hpp"
#include "services/encoder.hpp"
#include "utils/elapsed.hpp"

namespace sc
{

VideoFrameWriter::VideoFrameWriter(AVCodecContext* codec_context,
                                   AVStream* stream,
                                   Encoder encoder)
    : codec_context_ { codec_context }
    , stream_ { stream }
    , encoder_ { encoder }
{
}

auto VideoFrameWriter::operator()(CUdeviceptr cu_device_ptr,
                                  NVFBC_FRAME_GRAB_INFO,
                                  std::uint64_t frame_time) -> void
{
    auto encoder_frame =
        encoder_.prepare_frame(codec_context_.get(), stream_.get());
    auto* frame = encoder_frame->frame.get();

    frame->hw_frames_ctx = av_buffer_ref(codec_context_->hw_frames_ctx);

    auto hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(
        codec_context_->hw_frames_ctx->data);
    frame->buf[0] = av_buffer_pool_get(hw_frames_ctx->pool);

    frame->data[0] = reinterpret_cast<uint8_t*>(cu_device_ptr);
    frame->linesize[0] = codec_context_->width * sizeof(std::uint32_t);

    frame->extended_data = frame->data;
    frame->format = codec_context_->pix_fmt;
    frame->width = codec_context_->width;
    frame->height = codec_context_->height;
    frame->color_range = codec_context_->color_range;
    frame->color_primaries = codec_context_->color_primaries;
    frame->color_trc = codec_context_->color_trc;
    frame->colorspace = codec_context_->colorspace;
    frame->chroma_location = codec_context_->chroma_sample_location;

    frame->pts = frame_time * frame_number_++;

    encoder_.write_frame(std::move(encoder_frame));
}

} // namespace sc
