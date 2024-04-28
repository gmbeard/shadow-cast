#include "./media_container.hpp"
#include "av/frame.hpp"
#include "error.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <span>

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

    send_frame(frame, codec, ctx_.get(), *stream_pos, packet_.get());
    av_frame_unref(frame);
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
    //

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
