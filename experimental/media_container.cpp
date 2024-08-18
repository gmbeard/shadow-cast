#include "./media_container.hpp"
#include "error.hpp"
#include "logging.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <functional>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <mutex>
#include <span>

namespace sc
{

MediaContainer::MediaContainer(std::filesystem::path const& output_file)
    : ctx_ {}
    , open_ { false }
    , queue_processing_thread_ { &MediaContainer::queue_processing_thread_,
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
        flush();

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

    auto packet = packet_pool_.get();

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
        auto pool_item = packet_pool_.get();
        response = avcodec_receive_packet(ctx, pool_item->packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }

        if (response < 0) {
            throw std::runtime_error { "receive packet error" };
        }

        pool_item->packet->stream_index = stream->index;
        av_packet_rescale_ts(
            pool_item->packet, ctx->time_base, stream->time_base);

        /* FIX:
         * It's probably not a good idea to allow the output queue to just grow
         * to any size; Encoding could potentially be faster than writing the
         * output media, so we'd just end up consuming an unbound amount of
         * memory. Maybe a better implementation would be to allow _some_
         * packets to be queued but after a high watermark we should block until
         * the output queue size reduces...
         */
        {
            std::unique_lock lock { queue_mutex_ };
            output_queue_.push_back(pool_item.release());
        }

        queue_item_ready_.notify_one();
    }
}

auto MediaContainer::flush() -> void
{
    queue_processor_running_ = 0;
    queue_item_ready_.notify_one();
    if (queue_processing_thread_.joinable()) {
        queue_processing_thread_.join();
    }

    if (output_queue_.empty()) {
        return;
    }

    while (!output_queue_.empty()) {
        auto& packet = output_queue_.front();
        output_queue_.pop_front();
        SC_SCOPE_GUARD([&] { packet_pool_.put(&packet); });

        auto const response =
            av_interleaved_write_frame(ctx_.get(), packet.packet);

        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       sc::av_error_to_string(response) };
        }
    }

    log(LogLevel::debug, "Packet pool size: %llu", packet_pool_.size());
    log(LogLevel::debug,
        "Packet pool requests: %llu. Cached %llu",
        packet_pool_.requests(),
        packet_pool_.cached_requests());
}

auto MediaContainer::queue_processor_(MediaContainer& self) -> void
{
    while (self.queue_processor_running_) {
        std::unique_lock lock { self.queue_mutex_ };

        if (self.output_queue_.empty()) {
            self.queue_item_ready_.wait(lock, [&] {
                return !self.output_queue_.empty() ||
                       self.queue_processor_running_ == 0;
            });
        }

        if (!self.queue_processor_running_)
            break;

        auto& packet = self.output_queue_.front();
        self.output_queue_.pop_front();
        lock.unlock();

        SC_SCOPE_GUARD([&] { self.packet_pool_.put(&packet); });

        auto const response =
            av_interleaved_write_frame(self.ctx_.get(), packet.packet);

        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       sc::av_error_to_string(response) };
        }
    }
}

auto PacketPoolItem::reset() noexcept -> void { av_packet_unref(packet); }

auto PacketPoolLifetime::construct(PacketPoolItem* ptr) -> void
{
    new (static_cast<void*>(ptr)) PacketPoolItem {};
    ptr->packet = av_packet_alloc();
}

auto PacketPoolLifetime::destruct(PacketPoolItem* ptr) -> void
{
    av_packet_free(&ptr->packet);
    ptr->~PacketPoolItem();
}
} // namespace sc
