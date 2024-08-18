#ifndef SHADOW_CAST_UTILS_POOL_HPP_INCLUDED
#define SHADOW_CAST_UTILS_POOL_HPP_INCLUDED

#include "utils/contracts.hpp"
#include "utils/intrusive_list.hpp"
#include <atomic>
#include <memory>
#include <mutex>

namespace sc
{

// clang-format off
template<typename T>
concept PoolItem = ListItem<T> && requires(T val) {
    { val.reset() };
};
// clang-format on

struct DefaultPoolSynchronization
{
    auto enter() noexcept -> void {}
    auto leave() noexcept -> void {}
};

struct MutexPoolSynchronization
{
    auto enter() noexcept -> void { mutex_.lock(); }
    auto leave() noexcept -> void { mutex_.unlock(); }

private:
    std::mutex mutex_;
};

template <typename T>
struct DefaultObjectLifetime
{
    auto construct(T* ptr) -> void { new (static_cast<void*>(ptr)) T {}; }

    auto destruct(T* ptr) -> void { ptr->~T(); }
};

template <typename T>
struct SynchronizationGuard
{
    T& sync;

    SynchronizationGuard(T& s) noexcept
        : sync { s }
    {
        sync.enter();
    }

    ~SynchronizationGuard() { sync.leave(); }

    SynchronizationGuard(SynchronizationGuard const&) = delete;
    SynchronizationGuard(SynchronizationGuard&&) = delete;
    auto operator=(SynchronizationGuard const&)
        -> SynchronizationGuard& = delete;
    auto operator=(SynchronizationGuard&&) -> SynchronizationGuard& = delete;
};

template <ListItem T,
          typename ObjectLifetime = DefaultObjectLifetime<T>,
          typename Allocator = std::allocator<T>,
          typename Synchronization = DefaultPoolSynchronization>
struct Pool
{
    struct PoolItemDeleter
    {
        PoolItemDeleter(
            Pool<T, ObjectLifetime, Allocator, Synchronization>& pool) noexcept
            : pool_ { &pool }
        {
        }

        auto operator()(T* ptr) const noexcept -> void { pool_->put(ptr); }

    private:
        Pool<T, ObjectLifetime, Allocator, Synchronization>* pool_;
    };

    using ItemPtr = std::unique_ptr<T, PoolItemDeleter>;

    explicit Pool(ObjectLifetime lifetime = ObjectLifetime {},
                  Allocator alloc = Allocator {},
                  std::size_t max_pool_size = 1'024) noexcept
        : lifetime_ { lifetime }
        , alloc_ { alloc }
        , max_pool_size_ { max_pool_size }
    {
    }

    Pool(Pool const&) = delete;
    auto operator=(Pool const&) -> Pool& = delete;

    Pool(Pool&&) noexcept = default;
    auto operator=(Pool&&) -> Pool& = default;

    ~Pool() { clear(); }

    auto size() const noexcept -> std::size_t { return size_; }

    auto requests() const noexcept -> std::size_t { return requests_; }

    auto cached_requests() const noexcept -> std::size_t { return cached_; }

    auto max_pool_size() const noexcept -> std::size_t
    {
        return max_pool_size_;
    }

    /* Returns an item from the pool, or allocates
     * a new item if the pool is empty. The caller
     * must not delete any items returned by this
     * function. They must instead call put()
     */
    [[nodiscard]] auto get() -> ItemPtr
    {
        requests_ += 1;
        {
            SynchronizationGuard guard { sync_ };
            if (!pool_list_.empty()) {
                auto it = pool_list_.begin();
                T* item = &*it;
                static_cast<void>(pool_list_.erase(it));
                size_ -= 1;
                cached_ += 1;
                return ItemPtr { item, *this };
            }
        }

        T* new_item = allocate_item();
        try {
            lifetime_.construct(new_item);
        }
        catch (...) {
            deallocate_item(new_item);
            requests_ -= 1;
            throw;
        }

        return ItemPtr { new_item, *this };
    }

    /* Returns an item to the pool. The item
     * must have been previously obtained from
     * a call to get()
     */
    auto put(T* item) noexcept -> void
    {
        if constexpr (PoolItem<T>) {
            item->reset();
        }
        SynchronizationGuard guard { sync_ };
        if (size_ < max_pool_size_) {
            pool_list_.push_back(item);
            size_ += 1;
        }
        else {
            lifetime_.destruct(item);
            deallocate_item(item);
        }
    }

    /* Pre-populates the pool with `n` items
     */
    auto fill(std::size_t n) -> void
    {
        SC_EXPECT(n <= max_pool_size_);

        while (n--) {
            T* new_item = allocate_item();
            try {
                lifetime_.construct(new_item);
            }
            catch (...) {
                deallocate_item(new_item);
                throw;
            }

            SynchronizationGuard guard { sync_ };
            pool_list_.push_back(new_item);
            size_ += 1;
        }
    }

    /* Removes and deallocates all remaining
     * items in the pool. There should have been a
     * equal number of put() and get() calls
     * before calling clear()
     */
    auto clear() noexcept -> void
    {
        SynchronizationGuard guard { sync_ };
        auto it = pool_list_.begin();
        while (it != pool_list_.end()) {
            T* item = &*it;
            it = pool_list_.erase(it);

            lifetime_.destruct(item);
            deallocate_item(item);
        }
        size_ = 0;
    }

private:
    auto allocate_item() -> T*
    {
        using RecastAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        RecastAlloc alloc { alloc_ };
        return alloc.allocate(1);
    }

    auto deallocate_item(T* item) noexcept -> void
    {
        using RecastAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        RecastAlloc alloc { alloc_ };
        alloc.deallocate(item, 1);
    }

    ObjectLifetime lifetime_;
    Allocator alloc_;
    std::size_t max_pool_size_;
    IntrusiveList<T> pool_list_;
    Synchronization sync_;
    std::atomic_size_t size_ { 0 };
    std::atomic_size_t requests_ { 0 };
    std::atomic_size_t cached_ { 0 };
};

template <ListItem T,
          typename ObjectLifetime = DefaultObjectLifetime<T>,
          typename Allocator = std::allocator<T>>
using SynchronizedPool =
    Pool<T, ObjectLifetime, Allocator, MutexPoolSynchronization>;

template <typename List, typename Pool>
struct ReturnToPoolGuard
{
    List& list;
    Pool& pool;

    ReturnToPoolGuard(List& l, Pool& p) noexcept
        : list { l }
        , pool { p }
    {
    }

    ReturnToPoolGuard(ReturnToPoolGuard const&) = delete;
    auto operator=(ReturnToPoolGuard const&) -> ReturnToPoolGuard& = delete;

    ~ReturnToPoolGuard()
    {
        auto it = list.begin();
        while (it != list.end()) {
            auto item = typename Pool::ItemPtr { &*it, pool };
            it = list.erase(it);
        }
    }
};

template <typename List, typename Pool>
ReturnToPoolGuard(List&, Pool&) -> ReturnToPoolGuard<List, Pool>;

} // namespace sc

#endif // SHADOW_CAST_UTILS_POOL_HPP_INCLUDED
