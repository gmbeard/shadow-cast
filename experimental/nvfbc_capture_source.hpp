#ifndef SHADOW_CAST_NVFBC_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_NVFBC_CAPTURE_SOURCE_HPP_INCLUDED

#include "./capture_source.hpp"
#include "av/codec.hpp"
#include "cuda.hpp"
#include "exios/exios.hpp"
#include "nvfbc.hpp"
#include "nvidia/cuda.hpp"
#include "utils/cmd_line.hpp"

namespace sc
{
struct NvfbcCaptureSource
{
    NvfbcCaptureSource(exios::Context ctx,
                       Parameters const& params,
                       VideoOutputSize desktop_resolution);

    using completion_result_type = exios::Result<std::error_code>;

    template <CaptureCompletion<completion_result_type> Completion>
    auto capture(AVFrame* frame, Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        auto fn = [frame_number = frame_number_++,
                   frame,
                   nvfbc_session = nvfbc_session_.get(),
                   completion = std::move(completion)]() mutable {
            CUdeviceptr cu_device_ptr {};

            NVFBC_FRAME_GRAB_INFO frame_info {};

            NVFBC_TOCUDA_GRAB_FRAME_PARAMS grab_params {};
            grab_params.dwVersion = NVFBC_TOCUDA_GRAB_FRAME_PARAMS_VER;
            grab_params.dwFlags = NVFBC_TOCUDA_GRAB_FLAGS_NOWAIT;
            grab_params.pFrameGrabInfo = &frame_info;
            grab_params.pCUDADeviceBuffer = &cu_device_ptr;

            Elapsed timer;
            if (auto const status =
                    nvfbc().nvFBCToCudaGrabFrame(nvfbc_session, &grab_params);
                status != NVFBC_SUCCESS)
                throw NvFBCError { nvfbc(), nvfbc_session };

            CUDA_MEMCPY2D memcpy_struct {};

            memcpy_struct.srcXInBytes = 0;
            memcpy_struct.srcY = 0;
            memcpy_struct.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            memcpy_struct.dstXInBytes = 0;
            memcpy_struct.dstY = 0;
            memcpy_struct.dstMemoryType = CU_MEMORYTYPE_DEVICE;
            memcpy_struct.srcDevice = cu_device_ptr;
            memcpy_struct.dstDevice =
                reinterpret_cast<CUdeviceptr>(frame->data[0]);
            memcpy_struct.dstPitch = frame->linesize[0];
            memcpy_struct.WidthInBytes = frame->linesize[0];
            memcpy_struct.Height = frame->height;

            if (auto const r = cuda().cuMemcpy2D_v2(&memcpy_struct);
                r != CUDA_SUCCESS) {
                char const* err = "unknown";
                cuda().cuGetErrorString(r, &err);
                throw std::runtime_error {
                    std::to_string(frame_number) +
                    std::string { " Failed to copy CUDA buffer: " } + err
                };
            }

            frame->pts = frame_number;

            std::move(completion)(completion_result_type {});
        };

        ctx_.post(std::move(fn), alloc);
    }

private:
    exios::Context ctx_;
    NvFBCSessionHandlePtr nvfbc_session_;
    std::size_t frame_number_ { 0 };
};

} // namespace sc

#endif // SHADOW_CAST_NVFBC_CAPTURE_SOURCE_HPP_INCLUDED
