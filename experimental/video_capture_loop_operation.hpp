#ifndef SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED

#include "av/frame.hpp"
#include "exios/exios.hpp"
#include "exios/result.hpp"
#include "media_container.hpp"
#include "nvidia/cuda.hpp"
#include "video_frame_capture_operation.hpp"
#include <chrono>
#include <cinttypes>
#include <functional>
#include <libavcodec/avcodec.h>
#include <system_error>

namespace sc
{
template <typename T>
concept VideoCaptureLoopOperationCompletion =
    requires(T val, exios::Result<std::error_code> result) {
        {
            val(result)
        };
    };

template <typename Desktop,
          typename Gpu,
          typename TimerType,
          VideoCaptureLoopOperationCompletion Completion>
struct VideoCaptureLoopOperation
{
    using ClockType = std::chrono::high_resolution_clock;
    using TimePoint = decltype(ClockType::now());

    struct OnNewCapture
    {
    };
    struct OnCapturedFrame
    {
    };

    exios::Context ctx;
    Desktop& desktop;
    Gpu& gpu;
    AVCodecContext* encoder;
    MediaContainer& container;
    TimerType& timer;
    Completion completion;
    TimePoint frame_start = ClockType::now();
    FramePtr video_frame { av_frame_alloc() };
    std::size_t frame_number { 0 };

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);

        ctx.post(std::bind(
                     std::move(*this), OnNewCapture {}, exios::result_ok(0llu)),
                 alloc);
    }

    auto operator()(OnNewCapture, exios::TimerOrEventIoResult result) -> void
    {
        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        frame_start = ClockType::now();

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        std::cerr << "Capturing frame: " << frame_number << '\n';

        VideoFrameCaptureOpertion(
            ctx,
            desktop,
            gpu,
            exios::use_allocator(std::bind(std::move(*this),
                                           OnCapturedFrame {},
                                           std::placeholders::_1),
                                 alloc))
            .initiate();
    }

    auto operator()(OnCapturedFrame,
                    exios::Result<CUdeviceptr, std::error_code> result) -> void
    {
        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        std::cerr << "Writing frame: " << frame_number << '\n';

        fill_frame(
            gpu, result.value(), encoder, video_frame.get(), frame_number++);
        container.write_frame(video_frame.get(), encoder);

        auto elapsed = ClockType::now() - frame_start;
        std::int64_t delta = std::max(
            1l,
            static_cast<std::int64_t>(gpu.frame_time().value()) -
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                    .count());

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        std::cerr << "Next frame in: " << delta << "ns\n";

        timer.wait_for_expiry_after(
            std::chrono::nanoseconds(delta),
            exios::use_allocator(std::bind(std::move(*this),
                                           OnNewCapture {},
                                           std::placeholders::_1),
                                 alloc));
    }

private:
    auto finalize(exios::Result<std::error_code> result) -> void
    {
        container.write_frame(nullptr, encoder);
        std::move(completion)(std::move(result));
    }
};

} // namespace sc

#endif // SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
