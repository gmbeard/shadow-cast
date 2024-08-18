#ifndef SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
#define SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED

#include "exios/intrusive_list.hpp"

namespace sc
{

using ListItemBase = exios::ListItemBase;

template <typename T>
concept ListItem = exios::ListItem<T>;

template <typename T>
using IntrusiveList = exios::IntrusiveList<T>;

} // namespace sc
#endif // SHADOW_CAST_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
