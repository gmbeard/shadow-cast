#ifndef SHADOW_CAST_X11_PLATFORM_HPP_INCLUDED
#define SHADOW_CAST_X11_PLATFORM_HPP_INCLUDED

#include <X11/Xlib.h>
#include <memory>
#include <type_traits>

namespace sc
{

struct X11Platform
{
    using display_type = std::add_pointer_t<Display>;

    X11Platform() noexcept;
    auto display() const noexcept -> display_type;
    static auto is_supported() noexcept -> bool;
    operator bool() const noexcept;

private:
    struct DisplayDeleter
    {
        auto operator()(display_type) noexcept -> void;
    };

    using display_pointer =
        std::unique_ptr<std::remove_pointer_t<display_type>, DisplayDeleter>;

    display_pointer display_;
};

} // namespace sc

#endif // SHADOW_CAST_X11_PLATFORM_HPP_INCLUDED
