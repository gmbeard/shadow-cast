#ifndef SHADOW_CAST_ARENA_HPP_INCLUDED
#define SHADOW_CAST_ARENA_HPP_INCLUDED

#include "config.hpp"
#include "utils/contracts.hpp"
#include <cstddef>
#include <memory>
#include <type_traits>

namespace sc
{
namespace detail
{

struct BlockHeader
{
    std::size_t capacity { 0 };
    std::size_t free { 0 };
    std::size_t bytes_allocated { 0 };

    /* WARN:
     *  (de/)allocation is not thread-safe...
     */
    [[nodiscard]] auto allocate(std::size_t num_bytes, std::size_t alignment)
        -> void*;
    auto deallocate(std::size_t num_bytes) noexcept -> void;
    [[nodiscard]] auto data() const noexcept -> void*;
};

[[nodiscard]] auto get_arena(std::size_t n) noexcept -> BlockHeader*&;
} // namespace detail

[[nodiscard]] auto set_arena(std::size_t n, detail::BlockHeader* block) noexcept
    -> detail::BlockHeader*;
[[nodiscard]] auto create_arena(std::size_t capacity) -> detail::BlockHeader*;
auto destroy_arena(std::size_t n) noexcept -> void;

template <typename T, std::size_t ArenaIndex>
struct ArenaAllocator
{
    using value_type = T;

    explicit ArenaAllocator() noexcept {}

    template <typename U>
    ArenaAllocator(ArenaAllocator<U, ArenaIndex> const&) noexcept
    {
    }

    template <typename U>
    struct rebind
    {
        using other = ArenaAllocator<U, ArenaIndex>;
    };

    [[nodiscard]] auto allocate(std::size_t n) -> T*
    {
        SC_EXPECT(detail::get_arena(n));
        return reinterpret_cast<T*>(
            detail::get_arena(ArenaIndex)->allocate(sizeof(T) * n, alignof(T)));
    }

    auto deallocate(T*, std::size_t n) noexcept -> void
    {
        SC_EXPECT(detail::get_arena(n));
        detail::get_arena(ArenaIndex)->deallocate(sizeof(T) * n);
    }
};

template <typename T, std::size_t N>
constexpr auto operator==(ArenaAllocator<T, N> const&,
                          ArenaAllocator<T, N> const&) noexcept -> bool
{
    return true;
}

template <typename T, std::size_t N>
constexpr auto operator!=(ArenaAllocator<T, N> const& lhs,
                          ArenaAllocator<T, N> const& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

struct Arenas
{
    Arenas() noexcept = default;
    Arenas(Arenas const&) = delete;
    ~Arenas();
    auto operator=(Arenas const&) -> Arenas& = delete;
};

[[nodiscard]] auto create_memory_arenas() -> Arenas;

template <typename T>
using VideoOperationAllocator =
    typename std::conditional_t<0 != kNumberOfMemoryArenas,
                                ArenaAllocator<T, 0>,
                                std::allocator<void>>;

template <typename T>
using AudioOperationAllocator =
    typename std::conditional_t<0 != kNumberOfMemoryArenas,
                                ArenaAllocator<T, 1>,
                                std::allocator<void>>;
} // namespace sc

#endif // SHADOW_CAST_ARENA_HPP_INCLUDED
