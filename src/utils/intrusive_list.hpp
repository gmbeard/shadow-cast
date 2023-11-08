#ifndef SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
#define SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace sc
{

struct ListItemBase
{
    ListItemBase* prev { nullptr };
    ListItemBase* next { nullptr };
};

template <typename T>
concept ListItem = std::is_convertible_v<T&, ListItemBase const&>;

template <ListItem T>
struct IntrusiveList
{
    IntrusiveList() noexcept { sentinel_.next = sentinel_.prev = &sentinel_; }

    /* Not copyable; We don't know how to allocate
     * or copy items...
     */
    IntrusiveList(IntrusiveList const&) = delete;
    auto operator=(IntrusiveList const&) -> IntrusiveList& = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
    {
        if (other.empty()) {
            sentinel_.next = sentinel_.prev = &sentinel_;
        }
        else {
            other.sentinel_.next->prev = &sentinel_;
            other.sentinel_.prev->next = &sentinel_;

            other.sentinel_.next = other.sentinel_.prev = &other.sentinel_;
        }
    }

    auto operator=(IntrusiveList&& other) noexcept -> IntrusiveList&
    {
        if (this == &other)
            return *this;

        using std::swap;
        auto tmp { std::move(other) };
        swap(*this, tmp);
        return *this;
    }

    template <bool IsConst>
    struct Iterator
    {
        using difference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<IsConst,
                                              std::remove_const_t<T> const,
                                              std::remove_const_t<T>>;
        using pointer = value_type*;
        using reference = value_type&;
        using const_reference = std::remove_const_t<value_type> const&;
        using iterator_category = std::bidirectional_iterator_tag;

        using Element =
            std::conditional_t<IsConst, ListItemBase const, ListItemBase>;

        Element* current;

        Iterator(ListItemBase* item) noexcept
            : current { item }
        {
        }

        Iterator(Iterator<false> const& other) noexcept
        requires(IsConst)
            : current { other.current }
        {
        }

        Iterator(Iterator const& other) noexcept
            : current { other.current }
        {
        }

        auto operator=(Iterator const& other) noexcept -> Iterator&
        {
            current = other.current;
            return *this;
        }

        auto operator=(Iterator<false> const& other) noexcept -> Iterator&
        requires(IsConst)
        {
            current = other.current;
            return *this;
        }

        [[nodiscard]] auto operator->() const noexcept -> pointer
        {
            return static_cast<value_type*>(current);
        }

        [[nodiscard]] auto operator*() noexcept -> reference
        requires(!IsConst)
        {
            return *static_cast<value_type*>(current);
        }

        [[nodiscard]] auto operator*() const noexcept -> const_reference
        {
            return *static_cast<std::remove_const_t<value_type> const*>(
                current);
        }

        auto operator++(int) noexcept -> Iterator
        {
            auto tmp { *this };
            operator++();
            return tmp;
        }

        auto operator++() noexcept -> Iterator&
        {
            current = current->next;
            return *this;
        }

        auto operator--(int) noexcept -> Iterator
        {
            auto tmp { *this };
            operator--();
            return tmp;
        }

        auto operator--() noexcept -> Iterator&
        {
            current = current->prev;
            return *this;
        }

        [[nodiscard]] auto operator==(Iterator const& other) const noexcept
            -> bool
        {
            return current == other.current;
        }

        [[nodiscard]] auto operator!=(Iterator const& other) const noexcept
            -> bool
        {
            return !(*this == other);
        }
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return sentinel_.next == &sentinel_;
    }

    [[nodiscard]] auto erase(iterator pos) noexcept -> iterator
    {
        assert(pos != end());
        auto* item = &*pos;
        auto next_item = std::next(pos);

        item->next->prev = item->prev;
        item->prev->next = item->next;

        return next_item;
    }

    [[nodiscard]] auto front() noexcept -> typename iterator::reference
    {
        assert(!empty());
        return *begin();
    }

    [[nodiscard]] auto front() const noexcept ->
        typename const_iterator::reference
    {
        assert(!empty());
        return *begin();
    }

    [[nodiscard]] auto back() noexcept -> typename iterator::reference
    {
        assert(!empty());
        return *std::next(end(), -1);
    }

    [[nodiscard]] auto back() const noexcept ->
        typename const_iterator::reference
    {
        assert(!empty());
        return *std::next(end(), -1);
    }

    auto push_front(T* item) noexcept -> void
    {
        assert(item);
        sentinel_.next->prev = item;
        item->prev = &sentinel_;
        item->next = sentinel_.next;
        sentinel_.next = item;
    }

    auto push_back(T* item) noexcept -> void
    {
        assert(item);
        sentinel_.prev->next = item;
        item->next = &sentinel_;
        item->prev = sentinel_.prev;
        sentinel_.prev = item;
    }

    auto pop_back() noexcept -> void
    {
        assert(!empty());

        auto* item_to_remove = sentinel_.prev;

        sentinel_.prev = item_to_remove->prev;
        sentinel_.prev->next = &sentinel_;
    }

    auto pop_front() noexcept -> void
    {
        assert(!empty());

        auto* item_to_remove = sentinel_.next;
        sentinel_.next = item_to_remove->next;
        sentinel_.next->prev = &sentinel_;
    }

    [[nodiscard]] auto insert(T* item, iterator before) -> iterator
    {
        auto* before_item = before.current;
        before_item->prev->next = item;
        item->prev = before_item->prev;
        item->next = before_item;
        before_item->prev = item;
        return before;
    }

    auto
    splice(iterator before, IntrusiveList& other, iterator first, iterator last)
        -> void
    {
        auto it = first;
        while (it != last) {
            auto* val = &*it;
            it = other.erase(it);
            before = insert(val, before);
        }
    }

    auto splice(iterator before, IntrusiveList& other) -> void
    {
        splice(before, other, other.begin(), other.end());
    }

    [[nodiscard]] auto begin() noexcept -> iterator
    {
        return iterator { sentinel_.next };
    }
    [[nodiscard]] auto begin() const noexcept -> const_iterator
    {
        return const_iterator { sentinel_.next };
    }

    [[nodiscard]] auto end() noexcept -> iterator
    {
        return iterator { &sentinel_ };
    }
    [[nodiscard]] auto end() const noexcept -> const_iterator
    {
        return const_iterator { &sentinel_ };
    }

private:
    ListItemBase sentinel_;
};
} // namespace sc
#endif // SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
