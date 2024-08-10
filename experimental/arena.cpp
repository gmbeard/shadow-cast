#include "arena.hpp"
#include "config.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <array>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

namespace sc
{
namespace detail
{
auto BlockHeader::allocate(std::size_t num_bytes, std::size_t alignment)
    -> void*
{
    void* next = data();
    if (!std::align(alignment, num_bytes, next, free)) {
        throw std::bad_alloc {};
    }

    free -= num_bytes;
    bytes_allocated += num_bytes;

    return next;
}

auto BlockHeader::deallocate(std::size_t num_bytes) noexcept -> void
{
    SC_EXPECT(bytes_allocated >= num_bytes);
    bytes_allocated -= num_bytes;
    if (!bytes_allocated) {
        free = capacity;
    }
}

auto BlockHeader::data() const noexcept -> void*
{
    return reinterpret_cast<unsigned char*>(const_cast<BlockHeader*>(this) +
                                            1) +
           (capacity - free);
}

auto get_arena(std::size_t n) noexcept -> BlockHeader*&
{
    /* HACK:
     *  We need this for when memory arenas are disabled. The contract check
     * will enforce this isn't being called...
     */
    constexpr std::size_t const N = std::max(kNumberOfMemoryArenas, 1ul);

    static std::array<BlockHeader*, N> arenas {};
    SC_EXPECT(n < kNumberOfMemoryArenas);

    return arenas[n];
}

} // namespace detail
auto set_arena(std::size_t n, detail::BlockHeader* block) noexcept
    -> detail::BlockHeader*
{
    SC_EXPECT(n < kNumberOfMemoryArenas);
    return std::exchange(detail::get_arena(n), block);
}

auto create_arena(std::size_t capacity) -> detail::BlockHeader*
{
    SC_EXPECT(capacity > 0);

    std::size_t const num_bytes = sizeof(detail::BlockHeader) + capacity;

    void* ptr = std::malloc(num_bytes);
    if (!ptr) {
        throw std::bad_alloc {};
    }

    new (ptr) detail::BlockHeader { .capacity = capacity,
                                    .free = capacity,
                                    .bytes_allocated = 0 };
    return reinterpret_cast<detail::BlockHeader*>(ptr);
}

auto destroy_arena(std::size_t n) noexcept -> void
{
    auto* block = set_arena(n, nullptr);
    SC_EXPECT(block);
    SC_EXPECT(block->bytes_allocated == 0);

    std::free(block);
}

Arenas::~Arenas()
{
    if constexpr (!kNumberOfMemoryArenas) {
        return;
    }

    for (std::size_t n = 0; n < kNumberOfMemoryArenas; ++n) {
        destroy_arena(n);
    }
}

auto create_memory_arenas() -> Arenas
{
    if constexpr (!kNumberOfMemoryArenas) {
        return Arenas {};
    }

    std::size_t n = 0;
    auto guard = sc::ScopeGuard { [&] {
        std::size_t i = 0;
        for (; i < n; ++i) {
            destroy_arena(i);
        }
    } };

    for (; n < kNumberOfMemoryArenas; ++n) {
        auto* prev = set_arena(n, create_arena(kMemoryArenaByteCapacity));
        SC_EXPECT(!prev);
    }

    guard.deactivate();

    return Arenas {};
}

} // namespace sc
