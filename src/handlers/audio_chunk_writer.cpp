#include "handlers/audio_chunk_writer.hpp"
#include "av/sample_format.hpp"
#include <algorithm>
#include <memory>
#include <numeric>

namespace
{

auto append_chunk(sc::MediaChunk const& source, sc::MediaChunk& dest) -> void
{
    dest.sample_count += source.sample_count;
    auto channels_required =
        std::max(0,
                 static_cast<int>(source.channel_buffers().size() -
                                  dest.channel_buffers().size()));

    while (channels_required--)
        dest.channel_buffers().push_back(sc::DynamicBuffer {});

    auto it = source.channel_buffers().begin();
    auto out_it = dest.channel_buffers().begin();

    for (; it != source.channel_buffers().end(); ++it) {
        auto& src_buf = *it;
        auto& dst_buf = *out_it++;

        auto dst_data = dst_buf.prepare(src_buf.size());
        std::copy(
            src_buf.data().begin(), src_buf.data().end(), dst_data.begin());
        dst_buf.commit(dst_data.size());
    }
}

} // namespace

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

auto ChunkWriter::operator()(MediaChunk chnk) -> void
{
    /* Append this chunk to our buffered data...
     */
    append_chunk(chnk, buffer_);

    sc::SampleFormat const sample_format =
        sc::convert_from_libav_format(codec_context_->sample_fmt);

    auto const sample_size = sc::sample_format_size(sample_format);
    auto const interleaved = sc::is_interleaved_format(sample_format);

    auto samples_remaining = buffer_.sample_count;

    while (samples_remaining) {
        sc::BorrowedPtr<AVFrame> frame = frame_.get();

        /* We need to ensure we fill an output frame completely, otherwise
         * the audio/video sync will be off. If we don't have enough samples
         * then we just don't emit a frame and wait until then next
         * time we're called and check again...
         */
        if (static_cast<int>(samples_remaining) < codec_context_->frame_size)
            break;

        frame->nb_samples = codec_context_->frame_size
                                ? std::min(codec_context_->frame_size,
                                           static_cast<int>(samples_remaining))
                                : samples_remaining;

        frame->format = codec_context_->sample_fmt;
        frame->sample_rate = codec_context_->sample_rate;
        frame->channels = interleaved ? 2 : buffer_.channel_buffers().size();

        frame->channel_layout = AV_CH_LAYOUT_STEREO;
        frame->pts = total_samples_written_;
        total_samples_written_ += frame->nb_samples;

        sc::initialize_writable_buffer(frame.get());

        AVFrameUnrefGuard unref_guard { frame };

        auto n = 0;
        for (auto& channel_buffer : buffer_.channel_buffers()) {
            std::size_t const num_bytes =
                interleaved ? frame->nb_samples * sample_size * 2
                            : frame->nb_samples * sample_size;

            std::span source = channel_buffer.data().subspan(0, num_bytes);
            std::span target {
                reinterpret_cast<std::uint8_t*>(frame->data[n++]), num_bytes
            };

            std::copy(begin(source), end(source), begin(target));
            channel_buffer.consume(num_bytes);
        }

        samples_remaining -= frame->nb_samples;
        buffer_.sample_count -= frame->nb_samples;

        send_frame(frame.get(),
                   codec_context_.get(),
                   format_context_.get(),
                   stream_.get());
    }
}

} // namespace sc
