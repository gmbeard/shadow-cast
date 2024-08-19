#ifndef SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
#define SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED

#include "av/format.hpp"
#include "packet_queue.hpp"
#include <atomic>
#include <filesystem>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
}

namespace sc
{

struct MediaContainer
{
    explicit MediaContainer(std::filesystem::path const& output_file);
    ~MediaContainer();

    auto write_header() -> void;
    auto write_frame(AVFrame* frame, AVCodecContext* codec) -> void;
    auto flush() -> void;
    auto write_trailer() -> void;
    auto context() const noexcept -> AVFormatContext*;
    auto add_stream(AVCodecContext const* encoder) -> void;

private:
    auto encode_frame(AVFrame* frame, AVCodecContext* ctx, AVStream* stream)
        -> void;

    static auto queue_processor_(MediaContainer& self) -> void;

    FormatContextPtr ctx_;
    bool open_;
    std::size_t stream_count_ { 0 };
    PacketQueue output_queue_ {};
    std::atomic_uint8_t queue_processor_running_ { 1 };
    std::thread queue_processing_thread_;
};
} // namespace sc

#endif // SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
