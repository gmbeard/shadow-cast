#include "handlers/drm_video_frame_writer.hpp"
#include "services/encoder.hpp"
#include "utils/elapsed.hpp"

namespace sc
{

DRMVideoFrameWriter::DRMVideoFrameWriter(AVCodecContext* codec_context,
                                         AVStream* stream,
                                         Encoder encoder)
    : codec_context_ { codec_context }
    , stream_ { stream }
    , encoder_ { encoder }
{
}

auto DRMVideoFrameWriter::operator()(CUarray data,
                                     NvCuda const& cuda,
                                     std::uint64_t /*frame_time*/) -> void
{
    SC_EXPECT(data);

    auto encoder_frame =
        encoder_.prepare_frame(codec_context_.get(), stream_.get());
    auto* frame = encoder_frame->frame.get();

    frame->format = codec_context_->pix_fmt;
    frame->width = codec_context_->width;
    frame->height = codec_context_->height;
    frame->color_range = codec_context_->color_range;
    frame->color_primaries = codec_context_->color_primaries;
    frame->color_trc = codec_context_->color_trc;
    frame->colorspace = codec_context_->colorspace;
    frame->chroma_location = codec_context_->chroma_sample_location;
    if (auto const r =
            av_hwframe_get_buffer(codec_context_->hw_frames_ctx, frame, 0);
        r < 0)
        throw std::runtime_error { "Failed to get H/W frame buffer" };

    SC_EXPECT(frame->linesize[0]);
    SC_EXPECT(frame->height);
    SC_EXPECT(frame->data[0]);

    CUDA_MEMCPY2D memcpy_struct {};

    memcpy_struct.srcXInBytes = 0;
    memcpy_struct.srcY = 0;
    memcpy_struct.srcMemoryType = CU_MEMORYTYPE_ARRAY;
    memcpy_struct.dstXInBytes = 0;
    memcpy_struct.dstY = 0;
    memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    memcpy_struct.srcArray = data;
    memcpy_struct.dstDevice = reinterpret_cast<CUdeviceptr>(frame->data[0]);
    memcpy_struct.dstPitch = frame->linesize[0];
    memcpy_struct.WidthInBytes = frame->linesize[0];
    memcpy_struct.Height = frame->height;

    if (auto const r = cuda.cuMemcpy2D_v2(&memcpy_struct); r != CUDA_SUCCESS) {
        char const* err = "unknown";
        cuda.cuGetErrorString(r, &err);
        throw std::runtime_error {
            std::to_string(frame_number_) +
            std::string { " Failed to copy CUDA buffer: " } + err
        };
    }

    frame->pts = frame_number_++;

    encoder_.write_frame(std::move(encoder_frame));
}

} // namespace sc
