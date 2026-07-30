#include "./media_container.hpp"
#include "error.hpp"
#include "frame_timer.hpp"
#include "logging.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <libavutil/avutil.h>
#include <span>

#define RETRY_PACKET_WRITE 0
#if RETRY_PACKET_WRITE
std::size_t constexpr kMaxConsecutivePacketWriteFailures = 10;
#endif

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
        catch (std::exception const& e) {
            log(LogLevel::warn,
                "Flush in MediaContainer destructor threw an error: %s",
                e.what());
        }
        catch (...) {
            log(LogLevel::warn,
                "Flush in MediaContainer destructor threw an error");
        }

        SC_EXPECT(ctx_);
        avio_close(ctx_->pb);
    }
}

auto MediaContainer::check_for_thread_failure() -> void
{
    if (thread_error_pending_.exchange(false, std::memory_order_acquire)) {
        if (queue_processor_running_.exchange(0, std::memory_order_relaxed) ==
                1 &&
            queue_processing_thread_.joinable()) {
            queue_processing_thread_.join();
        }
        std::rethrow_exception(thread_error_);
    }
}

auto MediaContainer::write_header() -> void
{
    check_for_thread_failure();
    if (auto const ret = avformat_write_header(ctx_.get(), nullptr); ret < 0) {
        throw sc::IOError { "Failed to write header: " +
                            sc::av_error_to_string(ret) };
    }
}

auto MediaContainer::write_frame(AVFrame* frame, AVCodecContext* codec) -> void
{
    check_for_thread_failure();
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
    check_for_thread_failure();
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
    check_for_thread_failure();
    sc::BorrowedPtr<AVStream> stream { avformat_new_stream(ctx_.get(),
                                                           encoder->codec) };

    stream->index = stream_count_++;

    if (!stream)
        throw sc::CodecError { "Failed to allocate stream" };

    if (encoder->codec_type == AVMEDIA_TYPE_VIDEO) {
        stream->time_base = encoder->time_base;
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
    check_for_thread_failure();
    queue_processor_running_.store(0);
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
#if RETRY_PACKET_WRITE
    std::size_t consecutive_write_errors = 0;
#endif

    while (self.queue_processor_running_.load(std::memory_order_relaxed)) {

        auto* queue_item = self.output_queue_.dequeue();
        SC_SCOPE_GUARD([&] { self.output_queue_.commit(queue_item); });

        auto const response =
            av_interleaved_write_frame(self.ctx_.get(), queue_item->packet);

        if (response < 0) {
#if !RETRY_PACKET_WRITE
            self.thread_error_ = std::make_exception_ptr(std::runtime_error {
                "write packet error: " + sc::av_error_to_string(response) });
            self.thread_error_pending_.exchange(true,
                                                std::memory_order_release);
            return;
#else
            consecutive_write_errors += 1;

            if (consecutive_write_errors ==
                kMaxConsecutivePacketWriteFailures) {
                self.thread_error_ = std::make_exception_ptr(
                    std::runtime_error { "write packet error: " +
                                         sc::av_error_to_string(response) });
                self.thread_error_occurred_.exchange(true);
                return;
            }

            log(LogLevel::warn,
                "Write packet error: %s",
                sc::av_error_to_string(response).c_str());
        }
        else {
            consecutive_write_errors = 0;
#endif
        }
    }
}

} // namespace sc
