#include "platform/gpu.hpp"
#include "nvidia/cuda.hpp"

namespace sc
{

auto select_gpu(Parameters const&) -> GPU
{
    /* Only NVIDIA GPUs supported currently...
     */
    auto nvcudalib = load_cuda();
    if (auto const init_result = nvcudalib.cuInit(0);
        init_result != CUDA_SUCCESS)
        throw sc::NvCudaError { nvcudalib, init_result };
    auto cuda_ctx = create_cuda_context(nvcudalib);
    return Nvidia { nvcudalib, std::move(cuda_ctx) };
}

} // namespace sc
