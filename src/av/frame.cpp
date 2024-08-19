#include "av/frame.hpp"
#include "av/fwd.hpp"
#include "av/packet.hpp"
#include "error.hpp"
#include <mutex>
#include <stdexcept>

std::mutex send_frame_mutex {};

namespace
{
template <typename F>
auto invoke_synchronized(std::mutex& mutex, F&& f)
{
    std::lock_guard lock { mutex };
    return std::forward<F>(f)();
}

} // namespace

namespace sc
{

auto FrameDeleter::operator()(AVFrame* ptr) const noexcept -> void
{
    av_frame_free(&ptr);
}

AVFrameUnrefGuard::~AVFrameUnrefGuard()
{
    if (frame)
        av_frame_unref(frame.get());
}

auto send_frame(AVFrame* frame,
                AVCodecContext* ctx,
                AVFormatContext* fmt,
                AVStream* stream,
                AVPacket* packet) -> void
{
    auto response = avcodec_send_frame(ctx, frame);
    if (response < 0 && response != AVERROR(EAGAIN)) {
        throw std::runtime_error { "send frame error: " +
                                   av_error_to_string(response) };
    }

    bool requeue = response == AVERROR(EAGAIN);

    while (response >= 0 || response == AVERROR(EAGAIN)) {
        response = avcodec_receive_packet(ctx, packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }

        if (response < 0) {
            throw std::runtime_error { "receive packet error" };
        }

        PacketUnrefGuard packet_unref_guard { packet };
        packet->stream_index = stream->index;
        av_packet_rescale_ts(packet, ctx->time_base, stream->time_base);

        response = invoke_synchronized(send_frame_mutex, [&] {
            return av_interleaved_write_frame(fmt, packet);
        });
        if (response < 0) {
            throw std::runtime_error { "write packet error: " +
                                       av_error_to_string(response) };
        }
    }

    if (requeue) {
        send_frame(frame, ctx, fmt, stream, packet);
    }
}
} // namespace sc
