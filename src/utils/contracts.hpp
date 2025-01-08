#ifndef SHADOW_CAST_UTILS_CONTRACTS_HPP_INCLUDED
#define SHADOW_CAST_UTILS_CONTRACTS_HPP_INCLUDED

#include <source_location>

#define SC_STRINGIFY_IMPL(s) #s
#define SC_STRINGIFY(s) SC_STRINGIFY_IMPL(s)
#define SC_EXPECT(cond)                                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::sc::contract_check_failed(SC_STRINGIFY(cond));                   \
            __builtin_unreachable();                                           \
        }                                                                      \
    }                                                                          \
    while (0)

namespace sc
{

[[noreturn]] auto contract_check_failed(
    char const* /*msg*/,
    std::source_location /*loc*/ = std::source_location::current()) -> void;

} // namespace sc

#endif // SHADOW_CAST_UTILS_CONTRACTS_HPP_INCLUDED
