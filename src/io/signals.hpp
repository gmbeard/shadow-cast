#ifndef SHADOW_CAST_IO_SIGNALS_HPP_INCLUDED
#define SHADOW_CAST_IO_SIGNALS_HPP_INCLUDED

#include <initializer_list>

namespace sc
{
auto block_signals(std::initializer_list<int> /*sigs*/) -> void;
} // namespace sc
#endif // SHADOW_CAST_IO_SIGNALS_HPP_INCLUDED
