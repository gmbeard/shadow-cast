#include "./audio_service.hpp"
#include "./borrowed_ptr.hpp"
#include "./codec.hpp"
#include "./context.hpp"
#include "./elapsed.hpp"
#include "./format.hpp"
#include "./frame.hpp"
#include "./receiver.hpp"
#include "./result.hpp"
#include "./service.hpp"
#include "./service_registry.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

struct ChunkPrinter
{
    auto operator()(sc::MediaChunk chunk) -> void
    {
        auto const n_channels = chunk.channel_buffers().size();
        auto const n_samples =
            chunk.channel_buffers().front().size() / sizeof(float);

        /* move cursor up */
        if (move)
            std::fprintf(stdout, "%c[%luA", 0x1b, n_channels + 1);

        std::fprintf(stdout,
                     "Elapsed %lums. captured %lu samples\n",
                     sc::global_elapsed.value(),
                     n_samples);

        std::size_t c = 0;
        for (auto const& ch_buffer : chunk.channel_buffers()) {
            auto const sample_bytes = ch_buffer.data();
            std::span sample_data { reinterpret_cast<float const*>(
                                        sample_bytes.data()),
                                    sample_bytes.size() / sizeof(float) };

            float max = 0.f;
            for (auto const& sample : sample_data)
                max = std::max(max, std::fabs(sample));

            auto const peak = std::uint32_t(max * 39);

            std::fprintf(stdout,
                         "channel %lu: |%.*s*%*s| peak: %f\n",
                         c,
                         peak,
                         "---------------------------------------",
                         40 - peak,
                         "",
                         max);
            c += 1;
        }
        move = true;
        std::fflush(stdout);
    }

private:
    bool move { false };
};

auto send_frame(AVFrame* frame, AVCodecContext* ctx, AVFormatContext* fmt)
    -> void
{
    using PacketPtr = std::unique_ptr<AVPacket, auto(*)(AVPacket*)->void>;
    PacketPtr packet { av_packet_alloc(), [](auto pkt) {
                          av_packet_unref(pkt);
                          av_packet_free(&pkt);
                      } };

    auto response = avcodec_send_frame(ctx, frame);

    while (response >= 0) {
        response = avcodec_receive_packet(ctx, packet.get());
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }

        if (response < 0) {
            throw std::runtime_error { "receive packet error" };
        }

        packet->stream_index = 0;

        response = av_interleaved_write_frame(fmt, packet.get());
        if (response < 0) {
            throw std::runtime_error { "write packet error" };
        }
    }
}

struct ChunkWriter
{
    explicit ChunkWriter(AVFormatContext* format_context,
                         AVCodecContext* codec_context,
                         AVStream* stream) noexcept
        : format_context_ { format_context }
        , codec_context_ { codec_context }
        , stream_ { stream }
    {
    }

    auto operator()(sc::MediaChunk chunk) -> void
    {
        auto samples_remaining = chunk.sample_count;
        auto samples_written = 0;

        while (samples_remaining) {
            sc::FramePtr frame { av_frame_alloc() };

            /* TODO: We need to ensure we're filling the entire
             * buffer of a frame. If `chunk`'s size isn't fully
             * divisible by `codec_context_->frame_size` then we
             * must queue any remaning chunk bytes until we
             * have enough to fill a frame...
             */
            frame->nb_samples =
                codec_context_->frame_size
                    ? std::min(codec_context_->frame_size,
                               static_cast<int>(samples_remaining))
                    : samples_remaining;

            frame->format = codec_context_->sample_fmt;
            frame->sample_rate = codec_context_->sample_rate;
            frame->channels = chunk.channel_buffers().size();
            frame->channel_layout = AV_CH_LAYOUT_STEREO;
            frame->pts =
                chunk.timestamp_ms +
                samples_written / (codec_context_->sample_rate / 1'000);

            char error_buffer[AV_ERROR_MAX_STRING_SIZE];
            if (auto averr = av_frame_get_buffer(frame.get(), 0); averr < 0) {
                av_make_error_string(
                    error_buffer, AV_ERROR_MAX_STRING_SIZE, averr);
                throw std::runtime_error { error_buffer };
            }
            if (av_frame_make_writable(frame.get()) < 0)
                throw std::runtime_error { "Couldn't make frame writable" };

            auto n = 0;
            for (auto& channel_buffer : chunk.channel_buffers()) {
                std::span source = { reinterpret_cast<float const*>(
                                         channel_buffer.data().data()),
                                     static_cast<std::size_t>(
                                         frame->nb_samples) };

                std::span target { reinterpret_cast<float*>(frame->data[n++]),
                                   static_cast<std::size_t>(
                                       frame->nb_samples) };

                std::copy(begin(source), end(source), begin(target));
                channel_buffer.consume(frame->nb_samples * sizeof(float));
            }

            samples_written += frame->nb_samples;
            samples_remaining -= frame->nb_samples;

            send_frame(
                frame.get(), codec_context_.get(), format_context_.get());
        }
    }

private:
    sc::BorrowedPtr<AVFormatContext> format_context_;
    sc::BorrowedPtr<AVCodecContext> codec_context_;
    sc::BorrowedPtr<AVStream> stream_;
};

template <typename F>
auto set_audio_chunk_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::AudioService>()->set_chunk_listener(
        std::forward<F>(handler));
}

template <typename F>
auto set_audio_stream_end_handler(sc::Context& ctx, F&& handler)
{
    ctx.services().use_if<sc::AudioService>()->set_stream_end_listener(
        std::forward<F>(handler));
}

struct CodecError final : std::runtime_error
{
    CodecError(std::string const& msg)
        : std::runtime_error { msg }
    {
    }
};

struct FormatError final : std::runtime_error
{
    FormatError(std::string const& msg)
        : std::runtime_error { msg }
    {
    }
};

struct IOError final : std::runtime_error
{
    IOError(std::string const& msg)
        : std::runtime_error { msg }
    {
    }
};

auto find_supported_format(sc::BorrowedPtr<AVCodec const> codec,
                           AVSampleFormat required_fmt) noexcept
    -> sc::Result<AVSampleFormat, CodecError>
{
    for (auto supported_fmt = codec->sample_fmts;
         supported_fmt && *supported_fmt >= 0;
         ++supported_fmt)
        if (*supported_fmt == required_fmt)
            return *supported_fmt;

    return CodecError { "Codec doesn't support required sample format" };
}

auto main(int argc, char** argv) -> int
{
    pw_init(&argc, &argv);
    auto constexpr kFilename = "/tmp/shadow-cast.mkv";
    auto constexpr kAudioEncoderName = "aac";
    pw_init(&argc, &argv);

    AVFormatContext* fc_tmp;
    if (avformat_alloc_output_context2(&fc_tmp, nullptr, nullptr, kFilename) <
        0) {
        throw FormatError { "Couldn't allocate output context\n" };
    }

    sc::FormatContextPtr format_context { fc_tmp };
    sc::BorrowedPtr<AVCodec> encoder { avcodec_find_encoder_by_name(
        kAudioEncoderName) };
    if (!encoder) {
        throw CodecError { "Couldn't find required codec" };
    }

    sc::BorrowedPtr<AVStream> stream { avformat_new_stream(format_context.get(),
                                                           encoder.get()) };
    auto const sample_format_result =
        find_supported_format(encoder, AV_SAMPLE_FMT_FLTP);
    if (!sample_format_result)
        throw sc::get_error(sample_format_result);

    sc::CodecContextPtr encoder_context { avcodec_alloc_context3(
        encoder.get()) };

    encoder_context->channels = 2;
    encoder_context->channel_layout = av_get_default_channel_layout(2);
    encoder_context->sample_rate = 48'000;
    encoder_context->sample_fmt = *sample_format_result;
    encoder_context->bit_rate = 196'000;

    if (avcodec_open2(encoder_context.get(), encoder.get(), nullptr) < 0) {
        throw CodecError { "Couldn't open codec" };
    }

    if (avcodec_parameters_from_context(stream->codecpar,
                                        encoder_context.get()) < 0) {
        throw CodecError { "Couldn't copy codec parameters from context\n" };
    }

    if (avio_open(&format_context->pb, kFilename, AVIO_FLAG_WRITE) < 0) {
        throw IOError { "Couldn't open output file" };
    }

    if (avformat_write_header(format_context.get(), nullptr) < 0) {
        throw IOError { "Couldn't write header" };
    }

    sc::Context ctx;
    ctx.services().add_from_factory<sc::AudioService>(
        [] { return std::make_unique<sc::AudioService>(); });

    set_audio_chunk_handler(ctx,
                            ChunkWriter { format_context.get(),
                                          encoder_context.get(),
                                          stream.get() });
    set_audio_stream_end_handler(ctx, [&] {
        send_frame(nullptr, encoder_context.get(), format_context.get());
        if (av_write_trailer(format_context.get()) < 0)
            throw std::runtime_error { "Couldn't write trailer" };
    });

    ctx.run();
    pw_deinit();

    // TODO:
    return 0;
}
