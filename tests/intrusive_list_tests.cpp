#include "testing.hpp"
#include "utils/intrusive_list.hpp"

struct Identity : sc::ListItemBase
{
    Identity() noexcept = default;

    Identity(int v) noexcept
        : value { v }
    {
    }

    int value { 0 };
};

auto should_construct_empty() -> void
{
    sc::IntrusiveList<Identity> list;
    EXPECT(list.empty());
}

auto should_add_item_and_not_be_empty() -> void
{
    Identity id;
    sc::IntrusiveList<Identity> list;
    list.push_back(&id);
    EXPECT(!list.empty());
}

auto should_remove_item() -> void
{
    Identity a, b;
    sc::IntrusiveList<Identity> list;
    list.push_back(&a);
    list.push_back(&b);

    list.pop_back();
    EXPECT(!list.empty());
    list.pop_front();
    EXPECT(list.empty());
}

auto should_iterate_through_non_empty_list() -> void
{
    Identity item_a { 1 }, item_b { 2 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&item_a);
    list.push_front(&item_b);

    decltype(item_a.value) sum {};
    decltype(sum) const expected = 3;

    for (auto const& i : list)
        sum += i.value;

    EXPECT(sum == expected);
}

auto should_add_in_correct_order() -> void
{
    Identity a { 1 }, b { 2 }, c { 3 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&b);
    list.push_front(&a);
    list.push_back(&c);

    auto const& const_list = list;

    auto it = const_list.begin();
    EXPECT(it->value == 1);
    it++;
    EXPECT(it->value == 2);
    it++;
    EXPECT(it->value == 3);
    it++;
    EXPECT(it == list.end());
}

auto should_erase_items() -> void
{
    Identity a { 1 }, b { 2 }, c { 3 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&b);
    list.push_front(&a);
    list.push_back(&c);

    auto it = list.begin();
    EXPECT(it->value == 1);
    it = list.erase(it);
    EXPECT(it->value == 2);
    it = list.erase(it);
    EXPECT(it->value == 3);
    it = list.erase(it);
    EXPECT(it == list.end());
}

auto should_erase_and_insert() -> void
{
    Identity a;
    sc::IntrusiveList<Identity> list;

    for (auto i = 0; i < 10; ++i) {
        list.push_back(&a);
        auto it = list.erase(list.begin());
        EXPECT(it == list.end());
        EXPECT(list.empty());
    }
}

auto should_iterate() -> void
{
    Identity a { 1 }, b { 2 }, c { 3 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);

    EXPECT(std::distance(list.begin(), list.end()) == 3);

    auto n = 0;
    for ([[maybe_unused]] auto const& i : list)
        ++n;

    EXPECT(n == 3);
}

auto should_splice() -> void
{
    Identity a { 1 }, b { 2 }, c { 3 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);

    sc::IntrusiveList<Identity> other_list;

    other_list.splice(other_list.begin(), list);

    EXPECT(!other_list.empty());
    EXPECT(list.empty());

    auto it = other_list.begin();
    EXPECT((it++)->value == 1);
    EXPECT((it++)->value == 2);
    EXPECT((it++)->value == 3);
    EXPECT(it == other_list.end());
}

auto should_splice_range() -> void
{
    Identity a { 1 }, b { 2 }, c { 3 };
    sc::IntrusiveList<Identity> list;
    list.push_back(&a);
    list.push_back(&b);
    list.push_back(&c);

    sc::IntrusiveList<Identity> other_list;
    other_list.splice(
        other_list.begin(), list, std::next(list.begin()), list.end());

    EXPECT(!other_list.empty());
    EXPECT(!list.empty());

    EXPECT(list.front().value == 1);

    auto it = other_list.begin();
    EXPECT((it++)->value == 2);
    EXPECT((it++)->value == 3);
    EXPECT(it == other_list.end());
}

auto main() -> int
{
    return testing::run({ TEST(should_construct_empty),
                          TEST(should_add_item_and_not_be_empty),
                          TEST(should_iterate_through_non_empty_list),
                          TEST(should_remove_item),
                          TEST(should_add_in_correct_order),
                          TEST(should_erase_items),
                          TEST(should_erase_and_insert),
                          TEST(should_iterate),
                          TEST(should_splice),
                          TEST(should_splice_range) });
}
