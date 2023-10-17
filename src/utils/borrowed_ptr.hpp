#ifndef SHADOW_CAST_BORROWED_PTR_HPP_INCLUDED
#define SHADOW_CAST_BORROWED_PTR_HPP_INCLUDED

#include <type_traits>

namespace sc
{

template <typename T>
struct BorrowedPtr
{
    using value_type = std::remove_reference_t<T>;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using reference = value_type&;
    using const_reference = value_type const&;

    BorrowedPtr() = default;

    template <typename U>
    BorrowedPtr(U* p)
    requires(std::is_convertible_v<U*, pointer>)
        : ptr_ { p }
    {
    }

    template <typename U>
    BorrowedPtr(BorrowedPtr<U> const& other)
    requires(std::is_convertible_v<typename BorrowedPtr<U>::pointer, pointer>)
        : ptr_ { other.get() }
    {
    }

    auto get() const noexcept -> pointer { return ptr_; }

    operator bool() const noexcept { return ptr_ != nullptr; }

    auto operator->() const noexcept -> const_pointer { return ptr_; }

    auto operator->() noexcept -> pointer { return ptr_; }

    auto operator*() const noexcept -> const_reference { return *ptr_; }

    auto operator*() noexcept -> reference { return *ptr_; }

private:
    pointer ptr_ = nullptr;
};

} // namespace sc

#endif // SHADOW_CAST_BORROWED_PTR_HPP_INCLUDED
