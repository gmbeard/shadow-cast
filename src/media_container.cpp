#include "./media_container.hpp"
#include "error.hpp"
#include "frame_timer.hpp"
#include "logging.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <functional>
#include <libavutil/avutil.h>
#include <span>

namespace sc
{

MediaContainer::MediaContainer(std::filesystem::path const& output_file)
    : ctx_ {}
    , open_ { false }
    , queue_processing_thread_ { &MediaContainer::queue_processor_,
                                 std::ref(*this) }
{
    AVFormatContext* fc_tmp;
    if (auto const ret = avformat_alloc_output_context2(
            &fc_tmp, nullptr, nullptr, output_file.c_str());
        ret < 0) {
        throw sc::FormatError { "Failed to allocate output context: " +
                                sc::av_error_to_string(ret) };
    }

    ctx_.reset(fc_tmp);

    if (auto const ret =
            avio_open(&ctx_->pb, output_file.c_str(), AVIO_FLAG_WRITE);
        ret < 0) {
        throw sc::IOError { "Failed to open output file: " +
                            sc::av_error_to_string(ret) };
    }

    open_ = true;
}

MediaContainer::~MediaContainer()
{
    if (open_) {
        try {
            flush();
        }
        catch (...) {
            SC_EXPECT(false);
        }

        SC_EXPECT(ctx_);
        // cppcheck-suppress [nullPointerRedundantCheck]
        avio_close(ctx_->pb);
    }
}

auto MediaContainer::write_header() -> void
{
    if (auto const ret = avformat_write_header(ctx_.get(), nullptr); ret < 0) {
        throw sc::IOError { "Failed to write header: " +
                            sc::av_error_to_string(ret) };
    }
}

auto MediaContainer::write_frame(AVFrame* frame, AVCodecContext* codec) -> void
{
    SC_EXPECT(ctx_->streams);
    std::span streams { ctx_->streams, ctx_->nb_streams };
    auto stream_pos = std::find_if(
        streams.begin(), streams.end(), [&](AVStream const* stream) {
            return stream->codecpar->codec_type == codec->codec_type;
        });

    SC_EXPECT(stream_pos != streams.end());

    // cppcheck-suppress [derefInvalidIteratorRedundantCheck]
    encode_frame(frame, codec, *stream_pos);
}

auto MediaContainer::write_trailer() -> void
{
    if (auto const ret = av_write_trailer(ctx_.get()); ret < 0) {
        throw sc::IOError { "Failed to write trailer: " +
                            sc::av_error_to_string(ret) };
    }
}

auto MediaContainer::context() const noexcept -> AVFormatContext*
{
    return ctx_.get();
}

auto MediaContainer::add_stream(AVCodecContext const* encoder) -> void
{
    sc::BorrowedPtr<AVStream> stream { avformat_new_stream(ctx_.get(),
                                                           encoder->codec) };

    stream->index = stream_count_++;

    if (!stream)
        throw sc::CodecError { "Failed to allocate stream" };

    if (encoder->codec_type == AVMEDIA_TYPE_VIDEO) {
        stream->avg_frame_rate = encoder->framerate;
    }

    if (auto const ret =
            avcodec_parameters_from_context(stream->codecpar, encoder);
        ret < 0) {
        throw sc::CodecError {
            "Failed to copy video codec parameters from context: " +
            sc::av_error_to_string(ret)
        };
    }
}

auto MediaContainer::encode_frame(AVFrame* frame,
                                  AVCodecContext* ctx,
                                  AVStream* stream) -> void
{
    auto response = avcodec_send_frame(ctx, frame);
    if (response < 0) {
        throw std::runtime_error { "send frame error: " +
                                   sc::av_error_to_string(response) };
    }

    while (response >= 0 || response == AVERROR(EAGAIN)) {
        auto pool_item = output_queue_.prepare();
        response = avcodec_receive_packet(ctx, pool_item->packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }

        if (response < 0) {
            throw std::runtime_error { "receive packet error" };
        }

        if (pool_item->packet->dts != AV_NOPTS_VALUE ||
            pool_item->packet->pts != AV_NOPTS_VALUE) {
            auto const source_timebase =
                ctx->pkt_timebase.num ? ctx->pkt_timebase : ctx->time_base;

            auto const pts = pool_item->packet->pts;
            auto const dts = pool_item->packet->dts;

            if (ctx->codec_type == AVMEDIA_TYPE_VIDEO && pts < dts) {
                log(LogLevel::warn,
                    "Received an invalid timestamp from video encoder: dts "
                    "(%lli) > "
                    "pts (%lli) (src tb: %i/%i, dst tb: %i/%i). Expect some "
                    "desync.",
                    dts,
                    pts,
                    source_timebase.num,
                    source_timebase.den,
                    stream->time_base.num,
                    stream->time_base.den);
                pool_item->packet->dts = pool_item->packet->pts;
            }
            av_packet_rescale_ts(
                pool_item->packet, source_timebase, stream->time_base);
        }

        pool_item->packet->stream_index = stream->index;

        output_queue_.enqueue(pool_item.release());
    }
}

auto MediaContainer::flush() -> void
{
    queue_processor_running_ = 0;
    if (queue_processing_thread_.joinable()) {
        queue_processing_thread_.join();
    }

    if (output_queue_.empty()) {
        return;
    }

    while (!output_queue_.empty()) {
        auto* queue_item = output_queue_.dequeue();
        SC_SCOPE_GUARD([&] { output_queue_.commit(queue_item); });

        auto const response =
            av_interleaved_write_frame(ctx_.get(), queue_item->packet);

        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       sc::av_error_to_string(response) };
        }
    }
}

auto MediaContainer::queue_processor_(MediaContainer& self) -> void
{
    while (self.queue_processor_running_) {

        auto* queue_item = self.output_queue_.dequeue();
        SC_SCOPE_GUARD([&] { self.output_queue_.commit(queue_item); });

        auto const response =
            av_interleaved_write_frame(self.ctx_.get(), queue_item->packet);

        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       sc::av_error_to_string(response) };
        }
    }
}
} // namespace sc
