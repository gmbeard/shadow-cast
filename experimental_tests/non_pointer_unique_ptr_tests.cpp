#include "testing.hpp"
#include "utils/non_pointer.hpp"
#include <memory>

struct DeleterUnderTest
{
    explicit DeleterUnderTest(int i) noexcept
        : initial_ { i }
    {
    }

    using pointer = sc::NonPointer<int>;
    auto operator()(pointer val) -> void
    {
        EXPECT(initial_ == 42);
        EXPECT(val == pointer { 42 });
    }

private:
    int initial_;
};

using PointerUnderTest = std::unique_ptr<int, DeleterUnderTest>;

auto should_move_non_pointer_deleter()
{
    PointerUnderTest ptr { 42, DeleterUnderTest { 42 } };

    {
        auto other = std::move(ptr);
    }
}

auto main() -> int
{
    return testing::run({ TEST(should_move_non_pointer_deleter) });
}
