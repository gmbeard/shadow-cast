#include "handlers/drm_video_frame_writer.hpp"
#include "utils/elapsed.hpp"

namespace sc
{

DRMVideoFrameWriter::DRMVideoFrameWriter(AVFormatContext* fmt_context,
                                         AVCodecContext* codec_context,
                                         AVStream* stream)
    : format_context_ { fmt_context }
    , codec_context_ { codec_context }
    , stream_ { stream }
    , frame_ { av_frame_alloc() }
    , packet_ { av_packet_alloc() }
{
    frame_->format = codec_context_->pix_fmt;
    frame_->width = codec_context_->width;
    frame_->height = codec_context_->height;
    frame_->color_range = codec_context_->color_range;
    frame_->color_primaries = codec_context_->color_primaries;
    frame_->color_trc = codec_context_->color_trc;
    frame_->colorspace = codec_context_->colorspace;
    frame_->chroma_location = codec_context_->chroma_sample_location;
    if (auto const r = av_hwframe_get_buffer(
            codec_context_->hw_frames_ctx, frame_.get(), 0);
        r < 0)
        throw std::runtime_error { "Failed to get H/W frame buffer" };
}

auto DRMVideoFrameWriter::operator()(CUarray data, NvCuda const& cuda) -> void
{
    SC_EXPECT(data);
    SC_EXPECT(frame_->linesize[0]);
    SC_EXPECT(frame_->height);
    SC_EXPECT(frame_->data[0]);

    CUDA_MEMCPY2D memcpy_struct {};

    memcpy_struct.srcXInBytes = 0;
    memcpy_struct.srcY = 0;
    memcpy_struct.srcMemoryType = CU_MEMORYTYPE_ARRAY;
    memcpy_struct.dstXInBytes = 0;
    memcpy_struct.dstY = 0;
    memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    memcpy_struct.srcArray = data;
    memcpy_struct.dstDevice = reinterpret_cast<CUdeviceptr>(frame_->data[0]);
    memcpy_struct.dstPitch = frame_->linesize[0];
    memcpy_struct.WidthInBytes = frame_->linesize[0];
    memcpy_struct.Height = frame_->height;

    if (auto const r = cuda.cuMemcpy2D_v2(&memcpy_struct); r != CUDA_SUCCESS) {
        char const* err = "unknown";
        cuda.cuGetErrorString(r, &err);
        throw std::runtime_error {
            std::to_string(frame_number_) +
            std::string { " Failed to copy CUDA buffer: " } + err
        };
    }

    frame_->pts = frame_number_++;

    send_frame(frame_.get(),
               codec_context_.get(),
               format_context_.get(),
               stream_.get(),
               packet_.get());
}

} // namespace sc
