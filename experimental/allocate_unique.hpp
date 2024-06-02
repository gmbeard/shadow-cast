#ifndef SHADOW_CAST_ALLOCATE_UNIQUE_HPP_INCLUDED
#define SHADOW_CAST_ALLOCATE_UNIQUE_HPP_INCLUDED

#include <memory>

namespace sc
{
template <typename Allocator>
struct AllocateUniqueDeleter
{
    using allocator_traits = std::allocator_traits<Allocator>;
    using pointer = typename allocator_traits::pointer;

    explicit AllocateUniqueDeleter(Allocator const& alloc) noexcept
        : alloc_ { alloc }
    {
    }

    auto operator()(pointer p) noexcept -> void
    {
        allocator_traits::destroy(alloc_, p);
        allocator_traits::deallocate(alloc_, p, 1);
    }

private:
    Allocator alloc_;
};

template <typename T, typename Allocator, typename... Args>
auto allocate_unique(Allocator const& alloc, Args&&... args)
{
    using AllocatorTraits =
        std::allocator_traits<Allocator>::template rebind_traits<T>;
    using ReboundAlloc = AllocatorTraits::allocator_type;
    using Deleter = AllocateUniqueDeleter<ReboundAlloc>;

    ReboundAlloc rebound_alloc { alloc };
    auto* p = AllocatorTraits::allocate(rebound_alloc, 1);

    try {
        AllocatorTraits::construct(
            rebound_alloc, p, std::forward<Args>(args)...);
        return std::unique_ptr<T, Deleter> { p, Deleter { rebound_alloc } };
    }
    catch (...) {
        AllocatorTraits::deallocate(rebound_alloc, p, 1);
        throw;
    }
}

template <typename T, typename Alloc>
using AllocateUniquePtr =
    std::unique_ptr<T,
                    AllocateUniqueDeleter<typename std::allocator_traits<
                        Alloc>::template rebind_alloc<T>>>;

} // namespace sc

#endif // SHADOW_CAST_ALLOCATE_UNIQUE_HPP_INCLUDED
