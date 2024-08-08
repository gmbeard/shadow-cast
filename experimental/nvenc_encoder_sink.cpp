#include "./nvenc_encoder_sink.hpp"
#include "av/buffer.hpp"
#include "av/codec.hpp"
#include "cuda.hpp"
#include "logging.hpp"
#include "utils/cmd_line.hpp"
#include <cstdint>
#include <libavcodec/codec.h>
#include <libavutil/dict.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>

namespace
{

auto convert_quality_to_cq(std::int32_t setting) -> int
{
    constexpr auto const transposed_range = 51 - 18;
    auto const transposed =
        ((static_cast<float>(setting) - 1) / (10 - 1)) * transposed_range;

    return transposed_range - static_cast<int>(transposed) + 18;
}

/**
 * Creates the encoding context
 *
 * There are two modes of operation:
 * - Constant rate
 * - Constant quality
 *
 * Constant rate mode is determined by `params.rate > 0`. In this mode we insert
 * 2 B-frames either side of every P frame (i.e. `IBBPBBPBBPBB...`), and insert
 * an I-frame every 2 seconds (GOP size = FPS * 2). The implication of using
 * this mode is that it is for live streaming purposes. Streaming services
 * typically prefer this, and put upper limits on the bit rate.
 *
 * If `params.rate == 0` then this select CQ mode. There are several quality
 * settings in this mode, with high being the default. This mode implies a
 * variable bit rate operation.
 *
 * Both these modes use NVENC's `P5` preset.
 */
auto create_encoder_context(sc::Parameters const& params,
                            sc::VideoOutputSize desktop_resolution,
                            CUcontext cuda_context,
                            sc::NvencEncoderSink::PixelFormat pixel_format)
    -> sc::CodecContextPtr
{
    // TODO:
    sc::BorrowedPtr<AVCodec const> video_encoder { avcodec_find_encoder_by_name(
        params.video_encoder.c_str()) };
    if (!video_encoder) {
        throw sc::CodecError { "Failed to find required video codec" };
    }

    sc::CodecContextPtr video_encoder_context { avcodec_alloc_context3(
        video_encoder.get()) };
    video_encoder_context->codec_id = video_encoder->id;
    auto const framerate =
        av_make_q(static_cast<int>(params.frame_time.fps()), 1);
    video_encoder_context->framerate = framerate;
    video_encoder_context->time_base = av_inv_q(framerate);
    video_encoder_context->sample_aspect_ratio = av_make_q(0, 1);
    video_encoder_context->pix_fmt = AV_PIX_FMT_CUDA;
    video_encoder_context->bit_rate = params.bitrate;
    if (params.bitrate) {
        video_encoder_context->max_b_frames = 2;
        video_encoder_context->gop_size = framerate.num * 2;
    }

    video_encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (params.resolution) {
        video_encoder_context->width = params.resolution->width;
        video_encoder_context->height = params.resolution->height;
    }
    else {

        video_encoder_context->width = desktop_resolution.width;
        video_encoder_context->height = desktop_resolution.height;
    }

    sc::BufferPtr device_ctx { av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) };
    if (!device_ctx) {
        throw std::runtime_error { "Failed to allocate H/W device context" };
    }

    AVHWDeviceContext* hw_device_context =
        reinterpret_cast<AVHWDeviceContext*>(device_ctx->data);
    AVCUDADeviceContext* cuda_device_context =
        reinterpret_cast<AVCUDADeviceContext*>(hw_device_context->hwctx);

    // NOTE: Do we actually _need_ to supply our own CUDA context here,
    //  or will libav create one for us?
    cuda_device_context->cuda_ctx = cuda_context;

    if (auto const ret = av_hwdevice_ctx_init(device_ctx.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W device context: " +
                                   sc::av_error_to_string(ret) };
    }

    sc::BufferPtr frame_context { av_hwframe_ctx_alloc(device_ctx.get()) };
    if (!frame_context) {
        throw std::runtime_error { "Failed to allocate H/W frame context" };
    }

    AVHWFramesContext* hw_frame_context =
        reinterpret_cast<AVHWFramesContext*>(frame_context->data);
    hw_frame_context->width = video_encoder_context->width;
    hw_frame_context->height = video_encoder_context->height;
    hw_frame_context->sw_format =
        pixel_format == sc::NvencEncoderSink::PixelFormat::bgra
            ? AV_PIX_FMT_BGR0
            : AV_PIX_FMT_RGB0;
    hw_frame_context->format = video_encoder_context->pix_fmt;

    hw_frame_context->pool = nullptr;
    hw_frame_context->initial_pool_size = 1;

    if (auto const ret = av_hwframe_ctx_init(frame_context.get()); ret < 0) {
        throw std::runtime_error { "Failed to initialize H/W frame context: " +
                                   sc::av_error_to_string(ret) };
    }

    video_encoder_context->hw_frames_ctx = av_buffer_ref(frame_context.get());

    AVDictionary* options = nullptr;
    av_dict_set(&options, "preset", "p5", 0);
    if (video_encoder->id == AV_CODEC_ID_H264) {
        av_dict_set(&options, "profile", "high", 0);
        av_dict_set(&options, "coder", "cavlc", 0);
    }
    if (params.bitrate == 0) {
        av_dict_set(&options, "rc", "vbr", 0);
        auto const cq = convert_quality_to_cq(params.quality);
        sc::log(sc::LogLevel::info, "NVENC using VBR, cq value %i", cq);
        av_dict_set_int(&options, "cq", cq, 0);
    }
    else {
        sc::log(sc::LogLevel::info,
                "NVENC using CBR, bitrate %llu",
                params.bitrate);
        av_dict_set(&options, "rc", "cbr", 0);
    }

    if (auto const ret = avcodec_open2(
            video_encoder_context.get(), video_encoder.get(), &options);
        ret < 0) {
        throw sc::CodecError { "Failed to open video codec: " +
                               sc::av_error_to_string(ret) };
    }

    return video_encoder_context;
}

} // namespace

namespace sc
{
NvencEncoderSink::NvencEncoderSink(exios::Context ctx,
                                   CUcontext cuda_ctx,
                                   MediaContainer& container,
                                   Parameters const& params,
                                   VideoOutputSize desktop_resolution,
                                   PixelFormat pixel_format)
    : ctx_ { ctx }
    , container_ { container }
    , encoder_context_ { create_encoder_context(
          params, desktop_resolution, cuda_ctx, pixel_format) }
    , frame_ { av_frame_alloc() }
{
    container_.add_stream(encoder_context_.get());
}

auto NvencEncoderSink::prepare() -> input_type
{
    if (auto const r = av_hwframe_get_buffer(
            encoder_context_->hw_frames_ctx, frame_.get(), 0);
        r < 0)
        throw std::runtime_error { "Failed to get H/W frame buffer" };

    frame_->extended_data = frame_->data;
    frame_->format = encoder_context_->pix_fmt;
    frame_->width = encoder_context_->width;
    frame_->height = encoder_context_->height;
    frame_->color_range = encoder_context_->color_range;
    frame_->color_primaries = encoder_context_->color_primaries;
    frame_->color_trc = encoder_context_->color_trc;
    frame_->colorspace = encoder_context_->colorspace;
    frame_->chroma_location = encoder_context_->chroma_sample_location;

    /* FIX: We need to return some sort of RAII type instead of a
     * raw frame here. If we throw between the call to `prepare()` and
     * `write()` then we leak this memory...
     */
    return frame_.get();
}

} // namespace sc
