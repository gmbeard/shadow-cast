#ifndef SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
#define SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED

#include "av/format.hpp"
#include "av/packet.hpp"
#include <filesystem>
#include <libavcodec/avcodec.h>

namespace sc
{
struct MediaContainer
{
    explicit MediaContainer(std::filesystem::path const& output_file);
    MediaContainer(MediaContainer&&) noexcept = default;
    ~MediaContainer();
    auto operator=(MediaContainer&&) noexcept -> MediaContainer& = default;

    auto write_header() -> void;
    auto write_frame(AVFrame* frame, AVCodecContext* codec) -> void;
    auto write_trailer() -> void;
    auto context() const noexcept -> AVFormatContext*;
    auto add_stream(AVCodecContext const* encoder) -> void;

private:
    FormatContextPtr ctx_;
    bool open_;
    std::size_t stream_count_ { 0 };
    PacketPtr packet_ { av_packet_alloc() };
};
} // namespace sc

#endif // SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
