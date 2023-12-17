#ifndef SHADOW_CAST_UTILS_OVERLOADS_HPP_INCLUDED
#define SHADOW_CAST_UTILS_OVERLOADS_HPP_INCLUDED

namespace sc
{

// helper type for the visitor
template <class... Ts>
struct Overloads : Ts...
{
    using Ts::operator()...;
};

} // namespace sc
#endif // SHADOW_CAST_UTILS_OVERLOADS_HPP_INCLUDED
