#include "testing.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"

struct Identity : sc::ListItemBase
{
};

auto should_get_and_put() -> void
{
    sc::Pool<Identity> pool;

    for (auto i = 0; i < 10; ++i) {
        [[maybe_unused]] auto p = pool.get();
    }
}

auto main() -> int { return testing::run({ TEST(should_get_and_put) }); }
