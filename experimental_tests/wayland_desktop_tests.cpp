#include "testing.hpp"
#include "wayland_desktop.hpp"

auto should_create_desktop() -> void
{
    sc::WaylandDesktop desktop;
    EXPECT(desktop);
}

auto main() -> int
{
    //
    return testing::run({ TEST(should_create_desktop) });
}
