#ifndef SHADOW_CAST_PACKET_QUEUE_HPP_INCLUDED
#define SHADOW_CAST_PACKET_QUEUE_HPP_INCLUDED

#include "av/fwd.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"
#include <condition_variable>
#include <mutex>

namespace sc
{
constexpr std::size_t const kDefaultMaxPacketQueueSize = 10'000'000;

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

struct PacketQueue
{
    PacketQueue(std::size_t max_size = kDefaultMaxPacketQueueSize) noexcept;
    ~PacketQueue();

    auto empty() noexcept -> bool;
    auto prepare()
        -> SynchronizedPool<PacketPoolItem, PacketPoolLifetime>::ItemPtr;
    auto enqueue(PacketPoolItem* packet) -> void;
    auto dequeue() -> PacketPoolItem*;
    auto commit(PacketPoolItem* packet) -> void;

private:
    std::size_t max_size_;
    std::size_t size_ { 0 };
    SynchronizedPool<PacketPoolItem, PacketPoolLifetime> packet_pool_;
    IntrusiveList<PacketPoolItem> output_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_item_ready_;
    std::condition_variable queue_space_available_;
};
} // namespace sc

#endif // SHADOW_CAST_PACKET_QUEUE_HPP_INCLUDED
