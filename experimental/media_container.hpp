#ifndef SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
#define SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED

#include "av/format.hpp"
#include "av/packet.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <mutex>

namespace sc
{

struct PacketPoolItem : ListItemBase
{
    AVPacket* packet;
    auto reset() noexcept -> void;
};

struct PacketPoolLifetime
{
    auto construct(PacketPoolItem* ptr) -> void;
    auto destruct(PacketPoolItem* ptr) -> void;
};

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
    SynchronizedPool<PacketPoolItem, PacketPoolLifetime> packet_pool_;
    IntrusiveList<PacketPoolItem> output_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_item_ready_;
    std::thread queue_processing_thread_;
    std::atomic_uint8_t queue_processor_running_ { 1 };
};
} // namespace sc

#endif // SHADOW_CAST_MEDIA_CONTAINER_HPP_INCLUDED
