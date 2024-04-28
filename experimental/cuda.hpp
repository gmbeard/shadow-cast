#ifndef SHADOW_CAST_CUDA_HPP_INCLUDED
#define SHADOW_CAST_CUDA_HPP_INCLUDED

#include "nvidia.hpp"

namespace sc
{
auto cuda() -> NvCuda const&;
auto make_cuda_context() -> CUcontextPtr;
} // namespace sc

#endif // SHADOW_CAST_CUDA_HPP_INCLUDED
