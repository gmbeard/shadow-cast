#ifndef SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
#define SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED

#include "av/frame.hpp"
#include "capture_pipeline.hpp"
#include "capture_sink.hpp"
#include "capture_source.hpp"
#include "exios/exios.hpp"
#include "utils/frame_time.hpp"
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

template <CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source,
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

    Sink& sink;
    Source& source;
    TimerType& timer;
    FrameTime frame_time;
    Completion completion;
    TimePoint frame_start = ClockType::now();
    FramePtr video_frame { av_frame_alloc() };
    std::size_t frame_number { 0 };

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);

        frame_start = ClockType::now();

        capture_pipeline(sink,
                         source,
                         exios::use_allocator(std::bind(std::move(*this),
                                                        OnCapturedFrame {},
                                                        std::placeholders::_1),
                                              alloc));
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

        capture_pipeline(sink,
                         source,
                         exios::use_allocator(std::bind(std::move(*this),
                                                        OnCapturedFrame {},
                                                        std::placeholders::_1),
                                              alloc));
    }

    auto operator()(OnCapturedFrame, Source::completion_result_type result)
        -> void
    {
        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        auto elapsed = ClockType::now() - frame_start;
        std::int64_t delta = std::max(
            1l,
            static_cast<std::int64_t>(frame_time.value()) -
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                    .count());

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

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
        if (!result && result.error() != std::errc::operation_canceled) {
            std::move(completion)(std::move(result));
        }
        else {
            sink.flush(std::move(completion));
        }
    }
};

} // namespace sc

#endif // SHADOW_CAST_VIDEO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
