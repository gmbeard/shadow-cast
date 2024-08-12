#include "./media_container.hpp"
#include "error.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <span>

namespace
{
auto encode_frame(AVFrame* frame,
                  AVCodecContext* ctx,
                  AVFormatContext* fmt,
                  AVStream* stream,
                  AVPacket* packet) -> void
{
    auto response = avcodec_send_frame(ctx, frame);
    if (response < 0) {
        throw std::runtime_error { "send frame error: " +
                                   sc::av_error_to_string(response) };
    }

    while (response >= 0 || response == AVERROR(EAGAIN)) {
        response = avcodec_receive_packet(ctx, packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }

        if (response < 0) {
            throw std::runtime_error { "receive packet error" };
        }

        SC_SCOPE_GUARD([&] { av_packet_unref(packet); });

        packet->stream_index = stream->index;
        av_packet_rescale_ts(packet, ctx->time_base, stream->time_base);

        response = av_interleaved_write_frame(fmt, packet);
        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       sc::av_error_to_string(response) };
        }
    }
}
} // namespace

namespace sc
{

MediaContainer::MediaContainer(std::filesystem::path const& output_file)
    : ctx_ {}
    , open_ { false }
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
    encode_frame(frame, codec, ctx_.get(), *stream_pos, packet_.get());
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

} // namespace sc
