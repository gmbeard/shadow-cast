#ifndef SHADOW_CAST_NVIDIA_GPU_HPP_INCLUDED
#define SHADOW_CAST_NVIDIA_GPU_HPP_INCLUDED

#include "nvidia/cuda.hpp"
#include <type_traits>
namespace sc
{

struct NvidiaGpu
{
    NvidiaGpu();

    auto cuda_context() const noexcept -> CUcontext;

private:
    CUcontextPtr cuda_context_;
};

} // namespace sc

#endif // SHADOW_CAST_NVIDIA_GPU_HPP_INCLUDED
