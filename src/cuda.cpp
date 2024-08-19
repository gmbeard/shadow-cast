#include "./cuda.hpp"
#include "nvidia/cuda.hpp"

namespace sc
{
auto cuda() -> NvCuda const&
{
    static NvCuda lib = [] {
        auto cuda_lib = load_cuda();
        if (auto const init_result = cuda_lib.cuInit(0);
            init_result != CUDA_SUCCESS) {
            throw sc::NvCudaError { cuda_lib, init_result };
        }
        return cuda_lib;
    }();

    return lib;
}

auto make_cuda_context() -> CUcontextPtr { return create_cuda_context(cuda()); }

} // namespace sc
