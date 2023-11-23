#include "handlers/audio_chunk_writer.hpp"
#include "av/frame.hpp"
#include "av/sample_format.hpp"
#include "config.hpp"
#include "error.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>

namespace sc
{

ChunkWriter::ChunkWriter(AVFormatContext* format_context,
                         AVCodecContext* codec_context,
                         AVStream* stream) noexcept
    : format_context_ { format_context }
    , codec_context_ { codec_context }
    , stream_ { stream }
    , frame_ { av_frame_alloc() }
    , total_samples_written_ { 0 }
    , packet_ { av_packet_alloc() }
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
#if LIBAVCODEC_VERSION_MAJOR < 60
    frame->channels = interleaved ? 2 : chunk.channel_buffers().size();
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
#else
    av_channel_layout_copy(&frame->ch_layout, &codec_context_->ch_layout);
#endif
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
               stream_.get(),
               packet_.get());
}

} // namespace sc
