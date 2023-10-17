#ifndef SHADOW_CASTER_RECEIVER_HPP_INCLUDED
#define SHADOW_CASTER_RECEIVER_HPP_INCLUDED

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace sc
{

template <typename Ret, typename... Args>
struct Receiver;

template <typename Ret, typename... Args>
struct Receiver<Ret(Args...)>
{
    template <typename F, typename Alloc>
    struct Storage
    {
        F fn;
        Alloc alloc;
    };

    template <typename F, typename Alloc = std::allocator<void>>
    explicit Receiver(F&& fn, Alloc alloc = Alloc {})
    requires(!std::is_same_v<std::decay_t<F>, Receiver>)
        : dispatch_fn_ { &dispatch_<std::decay_t<F>, Alloc> }
        , destroy_fn_ { &destroy_<std::decay_t<F>, Alloc> }
        , storage_ { create_(std::forward<F>(fn), alloc) }
    {
    }

    ~Receiver()
    {
        if (storage_) {
            destroy_fn_(*this);
        }
    }

    Receiver(Receiver const&) = delete;

    Receiver(Receiver&& other) noexcept
        : dispatch_fn_ { std::exchange(other.dispatch_fn_, nullptr) }
        , destroy_fn_ { std::exchange(other.destroy_fn_, nullptr) }
        , storage_ { std::exchange(other.storage_, nullptr) }
    {
    }

    auto operator=(Receiver const&) -> Receiver& = delete;

    auto operator=(Receiver&& other) noexcept -> Receiver&
    {
        using std::swap;
        auto tmp { std::move(other) };

        swap(dispatch_fn_, tmp.dispatch_fn_);
        swap(destroy_fn_, tmp.destroy_fn_);
        swap(storage_, tmp.storage_);
        return *this;
    }

    auto operator()(Args... args) -> Ret
    {
        assert(storage_ && dispatch_fn_);
        return dispatch_fn_(*this, std::forward<Args>(args)...);
    }

private:
    template <typename T, typename Alloc>
    static auto dispatch_(Receiver& self, Args... args) -> Ret
    {
        auto& storage = *reinterpret_cast<Storage<T, Alloc>*>(self.storage_);
        return storage.fn(std::forward<Args>(args)...);
    }

    template <typename T, typename Alloc>
    static auto destroy_(Receiver& self) -> void
    {
        using StorageType = Storage<T, Alloc>;
        using RecastAlloc = typename std::allocator_traits<
            Alloc>::template rebind_alloc<StorageType>;

        auto& storage = *reinterpret_cast<StorageType*>(self.storage_);
        RecastAlloc alloc { storage.alloc };

        storage.~StorageType();
        alloc.deallocate(&storage, 1);
    }

    template <typename T, typename Alloc>
    static auto create_(T&& fn, Alloc const& alloc) -> Storage<T, Alloc>*
    {
        using StorageType = Storage<std::decay_t<T>, Alloc>;
        using RecastAlloc = typename std::allocator_traits<
            Alloc>::template rebind_alloc<StorageType>;
        struct ThrowProtection
        {
            StorageType* p;
            RecastAlloc& alloc;
            ~ThrowProtection()
            {
                if (p)
                    alloc.deallocate(p, 1);
            }
        };

        RecastAlloc recast_alloc { alloc };
        auto ptr = recast_alloc.allocate(1);
        ThrowProtection protect { ptr, recast_alloc };

        new (static_cast<void*>(ptr))
            StorageType { std::forward<T>(fn), alloc };

        return std::exchange(protect.p, nullptr);
    }

    auto(*dispatch_fn_)(Receiver&, Args...) -> Ret = nullptr;
    auto(*destroy_fn_)(Receiver&) -> void = nullptr;
    void* storage_ = nullptr;
};

} // namespace sc

#endif // SHADOW_CASTER_RECEIVER_HPP_INCLUDED
