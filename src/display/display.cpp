#include "display/display.hpp"

namespace sc
{

auto XDisplayDeleter::operator()(Display* ptr) const noexcept -> void
{
    XCloseDisplay(ptr);
}

auto get_display() -> XDisplayPtr
{
    XDisplayPtr display { XOpenDisplay(nullptr) };
    if (!display)
        throw std::runtime_error { "Failed to open X display" };

    return display;
}

} // namespace sc
