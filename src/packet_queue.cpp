#include "packet_queue.hpp"
#include "logging.hpp"
#include <mutex>

namespace sc
{

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

PacketQueue::PacketQueue(std::size_t max_size) noexcept
    : max_size_ { max_size }
{
}

PacketQueue::~PacketQueue()
{
    std::unique_lock lock { queue_mutex_ };
    while (!output_queue_.empty()) {
        auto& item = output_queue_.front();
        output_queue_.pop_front();
        packet_pool_.put(&item);
    }
}

auto PacketQueue::empty() noexcept -> bool
{
    std::unique_lock lock { queue_mutex_ };
    return output_queue_.empty();
}

auto PacketQueue::prepare()
    -> SynchronizedPool<PacketPoolItem, PacketPoolLifetime>::ItemPtr
{
    return packet_pool_.get();
}

auto PacketQueue::enqueue(PacketPoolItem* packet) -> void
{
    std::size_t packet_size = packet->packet->size;

    {
        std::unique_lock lock { queue_mutex_ };
        if ((size_ + packet_size) > max_size_) {
            log(LogLevel::debug, "Packet queue is full. Waiting...");
            queue_space_available_.wait(
                lock, [&] { return (size_ + packet_size) <= max_size_; });
            log(LogLevel::debug, "Packet has room. Enqueuing...");
        }

        output_queue_.push_back(packet);
        size_ += packet_size;
    }

    queue_item_ready_.notify_one();
}

auto PacketQueue::dequeue() -> PacketPoolItem*
{
    PacketPoolItem* output;

    {
        std::unique_lock lock { queue_mutex_ };
        if (output_queue_.empty()) {
            queue_item_ready_.wait(lock,
                                   [&] { return !output_queue_.empty(); });
        }

        auto& item = output_queue_.front();
        output_queue_.pop_front();
        SC_EXPECT(item.packet->size > 0);
        SC_EXPECT(size_ >= static_cast<std::size_t>(item.packet->size));
        size_ -= item.packet->size;
        output = &item;
    }

    queue_space_available_.notify_one();

    return output;
}

auto PacketQueue::commit(PacketPoolItem* packet) -> void
{
    packet_pool_.put(packet);
}

} // namespace sc
