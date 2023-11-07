#ifndef SHADOW_CAST_AV_MEDIA_CHUNK_HPP_INCLUDED
#define SHADOW_CAST_AV_MEDIA_CHUNK_HPP_INCLUDED

#include <cinttypes>
#include <cstddef>
#include <span>
#include <vector>

#include "utils/intrusive_list.hpp"

namespace sc
{
struct DynamicBuffer
{
    auto prepare(std::size_t) -> std::span<std::uint8_t>;
    auto commit(std::size_t) -> void;
    auto consume(std::size_t) -> void;
    auto data() noexcept -> std::span<std::uint8_t>;
    auto data() const noexcept -> std::span<std::uint8_t const>;
    auto size() const noexcept -> std::size_t;
    auto capacity() const noexcept -> std::size_t;
    auto reset() noexcept -> void;

private:
    std::vector<std::uint8_t> data_;
    std::size_t bytes_committed_ { 0 };
};

struct MediaChunk : ListItemBase
{
    std::size_t timestamp_ms { 0 };
    std::size_t sample_count { 0 };
    auto channel_buffers() noexcept -> std::vector<DynamicBuffer>&;
    auto channel_buffers() const noexcept -> std::vector<DynamicBuffer> const&;
    auto reset() noexcept -> void;

private:
    std::vector<DynamicBuffer> buffers_;
};
} // namespace sc

#endif // SHADOW_CAST_AV_MEDIA_CHUNK_HPP_INCLUDED
