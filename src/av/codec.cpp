#include "av/codec.hpp"
#include "av/buffer.hpp"
#include "error.hpp"
#include "nvidia.hpp"
#include <X11/Xlib.h>
extern "C" {
#include <libavutil/hwcontext_cuda.h>
}

namespace sc
{
auto CodecContextDeleter::operator()(AVCodecContext* ptr) noexcept -> void
{
    avcodec_free_context(&ptr);
}

auto create_video_encoder(std::string const& encoder_name,
                          CUcontext cuda_ctx,
                          AVBufferPool* pool,
                          VideoOutputSize size,
                          std::uint32_t fps,
                          AVPixelFormat pixel_format) -> sc::CodecContextPtr
{
    sc::BorrowedPtr<AVCodec const> video_encoder { avcodec_find_encoder_by_name(
        encoder_name.c_str()) };
    if (!video_encoder) {
        throw CodecError { "Failed to find required video codec" };
    }

    sc::CodecContextPtr video_encoder_context { avcodec_alloc_context3(
        video_encoder.get()) };
    video_encoder_context->codec_id = video_encoder->id;
    video_encoder_context->time_base.num = 1;
    video_encoder_context->time_base.den = fps;
    video_encoder_context->framerate.num = fps;
    video_encoder_context->framerate.den = 1;
    video_encoder_context->sample_aspect_ratio.num = 0;
    video_encoder_context->sample_aspect_ratio.den = 0;
    video_encoder_context->max_b_frames = 0;
    video_encoder_context->pix_fmt = AV_PIX_FMT_CUDA;
    video_encoder_context->bit_rate = 100'000;
    video_encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    video_encoder_context->width = size.width;
    video_encoder_context->height = size.height;

    sc::BufferPtr device_ctx { av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) };
    if (!device_ctx) {
        throw std::runtime_error { "Failed to allocate H/W device context" };
    }

    AVHWDeviceContext* hw_device_context =
        reinterpret_cast<AVHWDeviceContext*>(device_ctx->data);
    AVCUDADeviceContext* cuda_device_context =
        reinterpret_cast<AVCUDADeviceContext*>(hw_device_context->hwctx);
    cuda_device_context->cuda_ctx = cuda_ctx;
    if (auto const ret = av_hwdevice_ctx_init(device_ctx.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W device context: " +
                                   av_error_to_string(ret) };
    }

    sc::BufferPtr frame_context { av_hwframe_ctx_alloc(device_ctx.get()) };
    if (!frame_context) {
        throw std::runtime_error { "Failed to allocate H/W frame context" };
    }

    AVHWFramesContext* hw_frame_context =
        reinterpret_cast<AVHWFramesContext*>(frame_context->data);
    hw_frame_context->width = video_encoder_context->width;
    hw_frame_context->height = video_encoder_context->height;
    hw_frame_context->sw_format = pixel_format;
    hw_frame_context->format = video_encoder_context->pix_fmt;
    hw_frame_context->device_ref = av_buffer_ref(device_ctx.get());
    hw_frame_context->device_ctx =
        reinterpret_cast<AVHWDeviceContext*>(device_ctx->data);

    hw_frame_context->pool = pool;
    hw_frame_context->initial_pool_size = 1;

    if (auto const ret = av_hwframe_ctx_init(frame_context.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W frame context: " +
                                   av_error_to_string(ret) };
    }

    /* TODO: Are we doing this correctly? It seems to be the
     * source of memory leak...
     */
    video_encoder_context->hw_device_ctx = device_ctx.release();
    video_encoder_context->hw_frames_ctx = frame_context.release();

    AVDictionary* options = nullptr;
    av_dict_set_int(&options, "qp", 21, 0);
    av_dict_set(&options, "preset", "p5", 0);

    if (auto const ret = avcodec_open2(
            video_encoder_context.get(), video_encoder.get(), &options);
        ret < 0) {
        throw CodecError { "Failed to open video codec: " +
                           av_error_to_string(ret) };
    }

    return video_encoder_context;
}

} // namespace sc
