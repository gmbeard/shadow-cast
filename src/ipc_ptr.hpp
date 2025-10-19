#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_IPC_SHARED_PTR_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_IPC_SHARED_PTR_HPP_INCLUDED

#include "shared_memory.hpp"
#include "utils/contracts.hpp"
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace sc
{
template <typename T>
requires(
    not(std::is_pointer_v<T> || std::is_reference_v<T> || std::is_const_v<T>))
struct ipc_ptr
{
    using value_type = T;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using reference = value_type&;
    using const_reference = value_type const&;

    ipc_ptr(ipc_ptr&& other) noexcept
        : sh_mem_ { std::move(other.sh_mem_) }
    {
    }

    ~ipc_ptr()
    {
        if (sh_mem_.data()) {
            reinterpret_cast<pointer>(sh_mem_.data())->~T();
        }
    }

    friend auto swap(ipc_ptr& lhs, ipc_ptr& rhs) noexcept -> void
    {
        using std::swap;
        swap(lhs.sh_mem_, rhs.sh_mem_);
    }

    auto operator=(ipc_ptr&& rhs) noexcept -> ipc_ptr&
    {
        ipc_ptr tmp { std::move(rhs) };
        swap(*this, tmp);
        return *this;
    }

    template <typename... Args>
    static auto create(std::string path, Args&&... args) -> ipc_ptr<T>
    {
        shared_memory sh_mem = create_shared_memory(std::move(path), sizeof(T));
        new (sh_mem.data()) T { std::forward<Args>(args)... };

        return ipc_ptr<T> { std::move(sh_mem) };
    }

    static auto open(std::string path) -> ipc_ptr<T>
    {
        shared_memory sh_mem = open_shared_memory(std::move(path), sizeof(T));
        return ipc_ptr<T> { std::move(sh_mem) };
    }

    auto get() const noexcept -> const_pointer
    {
        SC_EXPECT(sh_mem_.data());
        return reinterpret_cast<const_pointer>(sh_mem_.data());
    }

    auto get() noexcept -> pointer
    {
        return const_cast<pointer>(std::as_const(*this).get());
    }

    auto operator->() const noexcept -> const_pointer
    {
        return std::addressof(get());
    }

    auto operator->() noexcept -> pointer
    {
        return std::addressof(get());
    }

    auto operator*() const noexcept -> const_reference
    {
        return *get();
    }

    auto operator*() noexcept -> reference
    {
        return *get();
    }

private:
    explicit ipc_ptr(shared_memory&& sh_mem) noexcept
        : sh_mem_ { std::move(sh_mem) }
    {
    }

    shared_memory sh_mem_;
};

template <typename T, typename... Args>
requires(not(std::is_pointer_v<T> || std::is_reference_v<T> ||
             std::is_const_v<T>))
auto make_ipc_ptr(std::string path, Args&&... args) -> ipc_ptr<T>
{
    return ipc_ptr<T>::create(std::move(path), std::forward<Args>(args)...);
}

template <typename T, typename... Args>
requires(not(std::is_pointer_v<T> || std::is_reference_v<T> ||
             std::is_const_v<T>))
auto open_ipc_ptr(std::string path) -> ipc_ptr<T>
{
    return ipc_ptr<T>::open(std::move(path));
}
} // namespace sc
#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_IPC_SHARED_PTR_HPP_INCLUDED
