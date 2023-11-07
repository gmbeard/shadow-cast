#include "handlers/audio_chunk_writer.hpp"
#include "av/sample_format.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>

namespace sc
{
auto send_frame(AVFrame* frame,
                AVCodecContext* ctx,
                AVFormatContext* fmt,
                AVStream* stream) -> void
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

        packet->stream_index = stream->index;
        packet->pts =
            av_rescale_q(packet->pts, ctx->time_base, stream->time_base);
        packet->dts =
            av_rescale_q(packet->dts, ctx->time_base, stream->time_base);

        response = av_interleaved_write_frame(fmt, packet.get());
        if (response < 0) {
            throw std::runtime_error { "write packet error" };
        }
    }
}

ChunkWriter::ChunkWriter(AVFormatContext* format_context,
                         AVCodecContext* codec_context,
                         AVStream* stream) noexcept
    : format_context_ { format_context }
    , codec_context_ { codec_context }
    , stream_ { stream }
    , frame_ { av_frame_alloc() }
    , total_samples_written_ { 0 }
{
}

auto ChunkWriter::operator()(MediaChunk const& chunk) -> void
{
    assert(static_cast<int>(chunk.sample_count) == codec_context_->frame_size);
    sc::SampleFormat const sample_format =
        sc::convert_from_libav_format(codec_context_->sample_fmt);

    auto const sample_size = sc::sample_format_size(sample_format);
    auto const interleaved = sc::is_interleaved_format(sample_format);

    sc::BorrowedPtr<AVFrame> frame = frame_.get();

    frame->nb_samples = chunk.sample_count;
    frame->format = codec_context_->sample_fmt;
    frame->sample_rate = codec_context_->sample_rate;
    frame->channels = interleaved ? 2 : chunk.channel_buffers().size();

    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    frame->pts = total_samples_written_;
    total_samples_written_ += frame->nb_samples;

    sc::initialize_writable_buffer(frame.get());

    AVFrameUnrefGuard unref_guard { frame };

    auto n = 0;
    for (auto const& channel_buffer : chunk.channel_buffers()) {
        std::size_t const num_bytes = interleaved
                                          ? frame->nb_samples * sample_size * 2
                                          : frame->nb_samples * sample_size;

        std::span source = channel_buffer.data().subspan(0, num_bytes);
        std::span target { reinterpret_cast<std::uint8_t*>(frame->data[n++]),
                           num_bytes };

        std::copy(begin(source), end(source), begin(target));
    }

    send_frame(frame.get(),
               codec_context_.get(),
               format_context_.get(),
               stream_.get());
}

} // namespace sc
