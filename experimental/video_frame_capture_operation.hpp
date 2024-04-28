#ifndef SHADOW_CAST_VIDEO_FRAME_CAPTURE_OPERATION_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_FRAME_CAPTURE_OPERATION_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/exios.hpp"
#include "nvidia/cuda.hpp"
#include <functional>
#include <iostream>
#include <memory>

namespace sc
{

template <typename Desktop, typename Gpu, typename Completion>
struct VideoFrameCaptureOpertion
{
    exios::Context context;
    Desktop& desktop;
    Gpu& gpu;
    Completion completion;

    auto initiate() -> void
    {
        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        auto const ctx = context;

        capture_frame(
            ctx,
            desktop,
            gpu,
            exios::use_allocator(
                std::bind(std::move(*this), std::placeholders::_1), alloc));
    }

    auto operator()(exios::Result<CUdeviceptr, std::error_code> result) -> void
    {
        std::move(completion)(std::move(result));
    }
};

} // namespace sc

#endif // SHADOW_CAST_VIDEO_FRAME_CAPTURE_OPERATION_HPP_INCLUDED
