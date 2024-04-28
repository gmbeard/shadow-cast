#ifndef SHADOW_CAST_NVFBC_HPP_INCLUDED
#define SHADOW_CAST_NVFBC_HPP_INCLUDED

#include "nvidia.hpp"

namespace sc
{

auto nvfbc() -> NvFBC const&;
auto make_nvfbc_session() -> NvFBCSessionHandlePtr;

} // namespace sc

#endif // SHADOW_CAST_NVFBC_HPP_INCLUDED
