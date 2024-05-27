#include "audio_encoder_sink.hpp"
#include "av/sample_format.hpp"
#include "utils/cmd_line.hpp"
#include <libavutil/frame.h>

namespace
{

constexpr int kDefaultAudioFrameSize = 1024;
constexpr char kDefaultAudioEncoder[] = "aac";

auto create_encoder_context(sc::Parameters const& params) -> sc::CodecContextPtr
{
    /* FIXME:
     *  Ensure we can support more audio encoders...
     */
    sc::BorrowedPtr<AVCodec const> encoder { avcodec_find_encoder_by_name(
        kDefaultAudioEncoder) };
    // sc::BorrowedPtr<AVCodec const> encoder { avcodec_find_encoder_by_name(
    //     params.audio_encoder.c_str()) };
    if (!encoder) {
        throw sc::CodecError { "Failed to find required audio codec" };
    }

    if (!sc::is_sample_rate_supported(params.sample_rate, encoder))
        throw std::runtime_error { "Sample rate not supported by codec: " +
                                   std::to_string(params.sample_rate) };

    auto const supported_formats = find_supported_formats(encoder);
    if (!supported_formats.size())
        throw std::runtime_error { "No supported sample formats found" };

    sc::CodecContextPtr audio_encoder_context { avcodec_alloc_context3(
        encoder.get()) };
    if (!audio_encoder_context)
        throw sc::CodecError { "Failed to allocate audio codec context" };

#if LIBAVCODEC_VERSION_MAJOR < 60
    audio_encoder_context->channels = 2;
    audio_encoder_context->channel_layout = av_get_default_channel_layout(2);
#else
    av_channel_layout_default(&audio_encoder_context->ch_layout, 2);
#endif
    audio_encoder_context->sample_rate = params.sample_rate;
    audio_encoder_context->sample_fmt =
        sc::convert_to_libav_format(supported_formats.front());
    audio_encoder_context->bit_rate =
        params.quality == sc::CaptureQuality::minimum ? 64'000
        : params.quality == sc::CaptureQuality::low   ? 96'000
                                                      : 128'000;
    audio_encoder_context->time_base = av_make_q(1, params.sample_rate);
    audio_encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (auto const ret =
            avcodec_open2(audio_encoder_context.get(), encoder.get(), nullptr);
        ret < 0) {
        throw sc::CodecError { "Failed to open audio codec: " +
                               sc::av_error_to_string(ret) };
    }

    return audio_encoder_context;
}

} // namespace
namespace sc
{

AudioEncoderSink::AudioEncoderSink(exios::Context ctx,
                                   MediaContainer& container,
                                   Parameters const& params)
    : ctx_ { ctx }
    , container_ { container }
    , encoder_context_ { create_encoder_context(params) }
    , frame_ { av_frame_alloc() }
{
    frame_->format = encoder_context_->sample_fmt;
    frame_->nb_samples = encoder_context_->frame_size
                             ? encoder_context_->frame_size
                             : kDefaultAudioFrameSize;
#if LIBAVCODEC_VERSION_MAJOR < 60
    frame_->channel_layout = encoder_context_->channel_layout;
    frame_->channels = encoder_context_->channels;
#else
    av_channel_layout_copy(&frame->ch_layout, &codec_context_->ch_layout);
#endif
    frame_->sample_rate = encoder_context_->sample_rate;

    char error_buffer[AV_ERROR_MAX_STRING_SIZE];
    if (auto averr = av_frame_get_buffer(frame_.get(), 0); averr < 0) {
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, averr);
        throw std::runtime_error { error_buffer };
    }
    container_.add_stream(encoder_context_.get());
}

auto AudioEncoderSink::prepare() -> input_type
{
    if (av_frame_make_writable(frame_.get()) < 0)
        throw std::runtime_error { "Couldn't make frame writable" };

    return frame_.get();
}

auto AudioEncoderSink::frame_size() const noexcept -> std::size_t
{
    return encoder_context_->frame_size;
}
} // namespace sc
