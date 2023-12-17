#ifndef SHADOW_CAST_PLATFORM_GPU_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_GPU_HPP_INCLUDED

#include "nvidia/cuda.hpp"
#include "utils/cmd_line.hpp"
#include <variant>

namespace sc
{
struct Nvidia
{
    NvCuda cudalib;
    CUcontextPtr cuda_ctx;
};

using GPU = std::variant<Nvidia>;

auto select_gpu(Parameters const&) -> GPU;

} // namespace sc

#endif // SHADOW_CAST_PLATFORM_GPU_HPP_INCLUDED
