#include "./nvfbc_capture.hpp"
#include "./cuda.hpp"
#include "nvidia/cuda.hpp"
#include <libavcodec/avcodec.h>

namespace sc
{

auto fill_frame(NvFbcGpu const&,
                NvfbcCapturedFrame value,
                AVCodecContext* codec,
                AVFrame* frame,
                std::size_t frame_number) -> void
{
    // frame->hw_frames_ctx = av_buffer_ref(codec->hw_frames_ctx);

    // auto hw_frames_ctx =
    //     reinterpret_cast<AVHWFramesContext*>(codec->hw_frames_ctx->data);

    if (auto const r = av_hwframe_get_buffer(codec->hw_frames_ctx, frame, 0);
        r < 0)
        throw std::runtime_error { "Failed to get H/W frame buffer" };

    // frame->buf[0] = av_buffer_pool_get(hw_frames_ctx->pool);

    // frame->data[0] = reinterpret_cast<uint8_t*>(value);
    // frame->linesize[0] = codec->width * sizeof(std::uint32_t);

    frame->extended_data = frame->data;
    frame->format = codec->pix_fmt;
    frame->width = codec->width;
    frame->height = codec->height;
    frame->color_range = codec->color_range;
    frame->color_primaries = codec->color_primaries;
    frame->color_trc = codec->color_trc;
    frame->colorspace = codec->colorspace;
    frame->chroma_location = codec->chroma_sample_location;

    CUDA_MEMCPY2D memcpy_struct {};

    memcpy_struct.srcXInBytes = 0;
    memcpy_struct.srcY = 0;
    memcpy_struct.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    memcpy_struct.dstXInBytes = 0;
    memcpy_struct.dstY = 0;
    memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    memcpy_struct.srcDevice = value.cuda_device;
    memcpy_struct.dstDevice = reinterpret_cast<CUdeviceptr>(frame->data[0]);
    memcpy_struct.dstPitch = frame->linesize[0];
    memcpy_struct.WidthInBytes = frame->linesize[0];
    memcpy_struct.Height = frame->height;

    if (auto const r = cuda().cuMemcpy2D_v2(&memcpy_struct);
        r != CUDA_SUCCESS) {
        char const* err = "unknown";
        cuda().cuGetErrorString(r, &err);
        throw std::runtime_error {
            std::to_string(frame_number) +
            std::string { " Failed to copy CUDA buffer: " } + err
        };
    }

    frame->pts = frame_number;
}

} // namespace sc
