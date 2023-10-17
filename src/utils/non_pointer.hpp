#ifndef SHADOW_CAST_UTILS_NON_POINTER_HPP_INCLUDED
#define SHADOW_CAST_UTILS_NON_POINTER_HPP_INCLUDED

#include <cstddef>

namespace sc
{
template <typename T>
struct NonPointer
{
    static T constexpr kNull = static_cast<T>(-1);

    NonPointer() noexcept = default;

    NonPointer(T val) noexcept
        : val_ { val }
    {
    }

    NonPointer(std::nullptr_t) noexcept
        : val_ { kNull }
    {
    }

    operator T() noexcept { return val_; }

    auto operator==(NonPointer const& other) noexcept -> bool
    {
        return val_ == other.val_;
    }

    auto operator!=(NonPointer const& other) noexcept -> bool
    {
        return !(*this == other);
    }

    auto operator==(std::nullptr_t) noexcept -> bool { return val_ == kNull; }
    auto operator!=(std::nullptr_t) noexcept -> bool { return val_ != kNull; }

private:
    T val_ { kNull };
};

} // namespace sc
#endif // SHADOW_CAST_UTILS_NON_POINTER_HPP_INCLUDED
