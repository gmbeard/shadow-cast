#include "av/media_chunk.hpp"
#include <cassert>

namespace sc
{
auto DynamicBuffer::reset() noexcept -> void { bytes_committed_ = 0; }

auto DynamicBuffer::prepare(std::size_t n) -> std::span<std::uint8_t>
{
    if (n > capacity())
        data_.resize(data_.size() + (n - capacity()));

    return std::span { next(begin(data_), bytes_committed_), end(data_) };
}

auto DynamicBuffer::commit(std::size_t n) -> void
{
    assert(n <= (data_.size() - bytes_committed_));
    bytes_committed_ += n;
}

auto DynamicBuffer::consume(std::size_t n) -> void
{
    assert(n <= bytes_committed_);
    data_.erase(begin(data_), next(begin(data_), n));

    bytes_committed_ -= n;
}

auto DynamicBuffer::data() noexcept -> std::span<std::uint8_t>
{
    return std::span { data_.data(), bytes_committed_ };
}

auto DynamicBuffer::data() const noexcept -> std::span<std::uint8_t const>
{
    return std::span { data_.data(), bytes_committed_ };
}

auto DynamicBuffer::size() const noexcept -> std::size_t
{
    return bytes_committed_;
}

auto DynamicBuffer::capacity() const noexcept -> std::size_t
{
    return data_.size() - size();
}

auto MediaChunk::reset() noexcept -> void
{
    timestamp_ms = 0;
    sample_count = 0;
    for (auto& b : channel_buffers())
        b.reset();
}

auto MediaChunk::channel_buffers() noexcept -> std::vector<DynamicBuffer>&
{
    return buffers_;
}

auto MediaChunk::channel_buffers() const noexcept
    -> std::vector<DynamicBuffer> const&
{
    return buffers_;
}

} // namespace sc
