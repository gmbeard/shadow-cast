#include "av/codec.hpp"
#include "av/buffer.hpp"
#include "error.hpp"
#include "nvidia.hpp"
#include <X11/Xlib.h>
#include <algorithm>
#include <cstdio>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/rational.h>

namespace
{
/* Converts input quality in the closed range [1,10] to
 * a nvenc cq value in the closed range [18,40]. The conversion is
 * reversed when converted, so a quality value of 1 will equaly a
 * cq value of 40.
 */
auto quality_to_cq_value(std::uint32_t quality) -> std::uint32_t
{
    constexpr std::uint32_t const max_cq = 51;
    constexpr std::uint32_t const min_cq = 18;

    constexpr std::uint32_t const max_quality = 10;
    constexpr std::uint32_t const min_quality = 1;

    return static_cast<std::uint32_t>(
        max_cq -
        ((static_cast<float>(std::clamp(quality, min_quality, max_quality)) -
          min_quality) /
         (static_cast<float>(max_quality) - min_quality)) *
            (static_cast<float>(max_cq) - min_cq));
}
} // namespace

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
                          FrameTime const& ft,
                          AVPixelFormat pixel_format,
                          std::uint32_t video_quality) -> sc::CodecContextPtr
{
    sc::BorrowedPtr<AVCodec const> video_encoder { avcodec_find_encoder_by_name(
        encoder_name.c_str()) };
    if (!video_encoder) {
        throw CodecError { "Failed to find required video codec" };
    }

    sc::CodecContextPtr video_encoder_context { avcodec_alloc_context3(
        video_encoder.get()) };
    video_encoder_context->codec_id = video_encoder->id;
    auto const timebase = ft.fps_ratio();
    video_encoder_context->time_base = timebase;
    video_encoder_context->framerate.num = timebase.den;
    video_encoder_context->framerate.den = timebase.num;
    video_encoder_context->sample_aspect_ratio = AVRational { 1, 1 };
    video_encoder_context->max_b_frames = 0;
    video_encoder_context->pix_fmt = AV_PIX_FMT_CUDA;
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

    hw_frame_context->pool = pool;
    hw_frame_context->initial_pool_size = 1;

    if (auto const ret = av_hwframe_ctx_init(frame_context.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W frame context: " +
                                   av_error_to_string(ret) };
    }

    video_encoder_context->hw_frames_ctx = av_buffer_ref(frame_context.get());

    std::fprintf(stderr, "Video quality specified %u\n", video_quality);
    std::fprintf(stderr,
                 "Using nvenc cq value %u\n",
                 quality_to_cq_value(video_quality));
    AVDictionary* options = nullptr;
    av_dict_set(&options, "preset", "p5", 0);
    av_dict_set(&options, "rc", "vbr", 0);
    av_dict_set_int(&options, "cq", quality_to_cq_value(video_quality), 0);

    if (auto const ret = avcodec_open2(
            video_encoder_context.get(), video_encoder.get(), &options);
        ret < 0) {
        throw CodecError { "Failed to open video codec: " +
                           av_error_to_string(ret) };
    }

    return video_encoder_context;
}

} // namespace sc
