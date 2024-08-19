#include "nvidia_gpu.hpp"
#include "cuda.hpp"
#include "nvidia/cuda.hpp"

namespace sc
{
NvidiaGpu::NvidiaGpu()
    : cuda_context_ { make_cuda_context() }
{
}

auto NvidiaGpu::cuda_context() const noexcept -> CUcontext
{
    return cuda_context_.get();
}
} // namespace sc
