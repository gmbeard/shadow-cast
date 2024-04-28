#ifndef SHADOW_CAST_NVFBC_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_NVFBC_CAPTURE_HPP_INCLUDED

#include "exios/exios.hpp"
#include "exios/work.hpp"
#include "nvfbc.hpp"
#include "nvfbc_gpu.hpp"
#include "nvidia/cuda.hpp"
#include "x11_desktop.hpp"
#include <iostream>
#include <memory>

namespace sc
{

template <typename T>
concept CaptureFrameCompletion =
    requires(T val, exios::Result<CUdeviceptr, std::error_code> result) {
        {
            val(result)
        };
    };

template <CaptureFrameCompletion Completion>
auto capture_frame(exios::Context ctx,
                   X11Desktop&,
                   NvFbcGpu& gpu,
                   Completion&& completion) -> void
{
    using Result = exios::Result<CUdeviceptr, std::error_code>;

    auto const alloc =
        exios::select_allocator(completion, std::allocator<void> {});

    std::cerr << "capture_frame - initiate\n";
    auto fn = [&gpu, completion = std::move(completion)]() mutable {
        std::cerr << "capture_frame - begin\n";
        CUdeviceptr cu_device_ptr {};

        NVFBC_FRAME_GRAB_INFO frame_info {};

        NVFBC_TOCUDA_GRAB_FRAME_PARAMS grab_params {};
        grab_params.dwVersion = NVFBC_TOCUDA_GRAB_FRAME_PARAMS_VER;
        grab_params.dwFlags = NVFBC_TOCUDA_GRAB_FLAGS_NOWAIT;
        grab_params.pFrameGrabInfo = &frame_info;
        grab_params.pCUDADeviceBuffer = &cu_device_ptr;

        if (auto const status =
                nvfbc().nvFBCToCudaGrabFrame(gpu.nvfbc_session(), &grab_params);
            status != NVFBC_SUCCESS)
            throw NvFBCError { nvfbc(), gpu.nvfbc_session() };

        std::cerr << "capture_frame - complete\n";
        std::move(completion)(Result { exios::result_ok(cu_device_ptr) });
    };

    ctx.post(std::move(fn), alloc);
}

auto fill_frame(NvFbcGpu const&,
                CUdeviceptr value,
                AVCodecContext* codec,
                AVFrame* frame,
                std::size_t frame_number) -> void;

} // namespace sc

#endif // SHADOW_CAST_NVFBC_CAPTURE_HPP_INCLUDED
