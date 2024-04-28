#include "./nvfbc_gpu.hpp"
#include "av/buffer.hpp"
#include "av/codec.hpp"
#include "cuda.hpp"
#include "nvfbc.hpp"
#include "nvidia.hpp"
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/rational.h>

namespace sc
{

NvFbcGpu::NvFbcGpu(FrameTime const& frame_time)
    : cuda_ctx_ { make_cuda_context() }
    , session_ { make_nvfbc_session() }
    , frame_time_ { frame_time }
    , buffer_pool_ { av_buffer_pool_init(
          1, [](auto size) { return av_buffer_alloc(size); }) }
{
    if (!buffer_pool_) {
        throw sc::CodecError { "Failed to allocate video buffer pool" };
    }

    create_nvfbc_capture_session(session_.get(), nvfbc(), frame_time_);
}

auto NvFbcGpu::frame_time() const noexcept -> FrameTime const&
{
    return frame_time_;
}

auto NvFbcGpu::create_encoder(std::string const& codec_name,
                              VideoOutputSize const& dimensions)
    -> CodecContextPtr
{
    sc::BorrowedPtr<AVCodec const> video_encoder { avcodec_find_encoder_by_name(
        codec_name.c_str()) };
    if (!video_encoder) {
        throw CodecError { "Failed to find required video codec" };
    }

    sc::CodecContextPtr video_encoder_context { avcodec_alloc_context3(
        video_encoder.get()) };
    video_encoder_context->codec_id = video_encoder->id;
    auto const timebase =
        AVRational { .num = 1, .den = static_cast<int>(frame_time_.fps()) };
    video_encoder_context->time_base = timebase;
    video_encoder_context->framerate =
        AVRational { timebase.den, timebase.num };
    video_encoder_context->sample_aspect_ratio = AVRational { 0, 1 };
    video_encoder_context->max_b_frames = 0;
    video_encoder_context->pix_fmt = AV_PIX_FMT_CUDA;
    video_encoder_context->bit_rate = 100'000;
    video_encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    video_encoder_context->width = dimensions.width;
    video_encoder_context->height = dimensions.height;

    sc::BufferPtr device_ctx { av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) };
    if (!device_ctx) {
        throw std::runtime_error { "Failed to allocate H/W device context" };
    }

    AVHWDeviceContext* hw_device_context =
        reinterpret_cast<AVHWDeviceContext*>(device_ctx->data);
    AVCUDADeviceContext* cuda_device_context =
        reinterpret_cast<AVCUDADeviceContext*>(hw_device_context->hwctx);
    cuda_device_context->cuda_ctx = cuda_ctx_.get();
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
    hw_frame_context->sw_format = AV_PIX_FMT_BGR0;
    hw_frame_context->format = video_encoder_context->pix_fmt;

    hw_frame_context->pool = buffer_pool_.get();
    hw_frame_context->initial_pool_size = 1;

    if (auto const ret = av_hwframe_ctx_init(frame_context.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W frame context: " +
                                   av_error_to_string(ret) };
    }

    video_encoder_context->hw_frames_ctx = av_buffer_ref(frame_context.get());

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

auto NvFbcGpu::cuda_context() const noexcept -> CUcontextPtr::pointer
{
    return cuda_ctx_.get();
}

auto NvFbcGpu::nvfbc_session() const noexcept -> NvFBCSessionHandlePtr::pointer
{
    return session_.get();
}

} // namespace sc
