#ifndef SHADOW_CAST_LTU_MAP_HPP_INCLUDED
#define SHADOW_CAST_LTU_MAP_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <utility>

namespace sc
{

template <typename Key,
          typename T,
          typename Compare = std::less<Key>,
          typename Allocator = std::allocator<
              std::pair<Key const,
                        typename std::list<std::pair<Key const, T>>::iterator>>>
struct lru_map
{
    using history_type = std::list<std::pair<Key const, T>>;
    using iterator = typename history_type::iterator;
    using reverse_iterator = typename history_type::reverse_iterator;
    using const_iterator = typename history_type::const_iterator;
    using const_reverse_iterator =
        typename history_type::const_reverse_iterator;
    using index_type = std::map<Key, iterator, Compare, Allocator>;

    explicit lru_map(std::size_t max_size) noexcept
        : max_size_ { max_size }
    {
    }

    auto find(Key const& key) -> iterator
    {
        auto position = index_.find(key);
        if (position == index_.end())
            return end();

        return move_to_front(position);
    }

    auto insert(Key const& key, T item) -> std::pair<iterator, bool>
    {
        if (max_size_ == 0)
            return std::make_pair(history_.end(), false);

        auto existing = index_.find(key);

        if (existing != index_.end()) {
            auto position = move_to_front(existing);
            SC_EXPECT(position != end());
            return std::make_pair(position, false);
        }

        if (history_.size() == max_size_ && history_.size() > 0) {
            auto& last = history_.back();
            SC_EXPECT(index_.erase(std::get<0>(last)) == 1);
            history_.pop_back();
            SC_EXPECT(size_ > 0);
            size_ -= 1;
        }

        auto history_position = history_.insert(
            history_.begin(), std::make_pair(key, std::move(item)));

        auto [position, inserted] =
            index_.insert(std::make_pair(key, history_position));
        SC_EXPECT(inserted);

        size_ += 1;

        return std::make_pair(history_position, true);
    }

    auto erase(iterator pos) -> void
    {
        SC_EXPECT(pos != end());
        SC_EXPECT(index_.erase(pos->first) == 1);
        history_.erase(pos);
        SC_EXPECT(size_ > 0);
        size_ -= 1;
    }

    auto capacity() const noexcept -> std::size_t
    {
        return max_size_;
    }
    auto size() const noexcept -> std::size_t
    {
        return size_;
    }
    auto begin() const noexcept -> const_iterator
    {
        return history_.begin();
    }
    auto rbegin() const noexcept -> const_reverse_iterator
    {
        return history_.rbegin();
    }
    auto end() const noexcept -> const_iterator
    {
        return history_.end();
    }
    auto rend() const noexcept -> const_reverse_iterator
    {
        return history_.rend();
    }
    auto begin() noexcept -> iterator
    {
        return history_.begin();
    }
    auto rbegin() noexcept -> reverse_iterator
    {
        return history_.rbegin();
    }
    auto end() noexcept -> iterator
    {
        return history_.end();
    }
    auto rend() noexcept -> reverse_iterator
    {
        return history_.rend();
    }

private:
    auto move_to_front(typename index_type::iterator pos) -> iterator
    {
        auto history_position = std::get<1>(*pos);
        history_.splice(history_.begin(), history_, history_position);
        return history_position;
    }

    std::size_t max_size_;
    history_type history_;
    index_type index_;
    std::size_t size_ { 0 };
};

} // namespace sc

#endif // SHADOW_CAST_LTU_MAP_HPP_INCLUDED
