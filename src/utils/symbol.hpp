#ifndef SHADOW_CAST_UTILS_SYMBOL_HPP_INCLUDED
#define SHADOW_CAST_UTILS_SYMBOL_HPP_INCLUDED

#include "error.hpp"
#include <dlfcn.h>

#define TRY_ATTACH_SYMBOL(target, name, lib)                                   \
    ::sc::attach_symbol(target, name, lib)

namespace sc
{
template <typename F>
auto attach_symbol(F** fn, char const* name, void* lib) -> void
{
    *fn = reinterpret_cast<F*>(dlsym(lib, name));
    if (!*fn)
        throw sc::ModuleError { std::string { "Couldn't load symbol: " } +
                                name };
}
} // namespace sc

#endif // SHADOW_CAST_UTILS_SYMBOL_HPP_INCLUDED
