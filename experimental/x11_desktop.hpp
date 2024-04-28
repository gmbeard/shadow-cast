#ifndef SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED

#include "av/codec.hpp"
#include "display/display.hpp"

namespace sc
{
struct X11Desktop
{
    X11Desktop();

    auto size() const noexcept -> VideoOutputSize;

private:
    XDisplayPtr display_;
};
} // namespace sc
#endif // SHADOW_CAST_X11_DESKTOP_HPP_INCLUDED
