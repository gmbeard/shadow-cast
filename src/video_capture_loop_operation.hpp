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
#include <cmath>
#include <functional>
extern "C" {
#include <libavcodec/avcodec.h>
}
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
    using loop_result_type = exios::Result<Timing, std::error_code>;

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
    std::size_t frame_backlog { 0 };

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

    auto operator()(OnNewCapture,
                    TimePoint next_frame_start,
                    exios::TimerOrEventIoResult result) -> void
    {
        frame_start = next_frame_start;
        if (frame_backlog > 0)
            frame_backlog -= 1;

        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        capture_pipeline(sink,
                         source,
                         exios::use_allocator(std::bind(std::move(*this),
                                                        OnCapturedFrame {},
                                                        std::placeholders::_1),
                                              alloc));
    }

    auto operator()(OnCapturedFrame, loop_result_type result) -> void
    {
        auto const frame_finish = ClockType::now();
        auto const elapsed = frame_finish - frame_start;

        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        auto const missed_frames =
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                .count() /
            frame_time.value();

        auto delta = std::chrono::nanoseconds(frame_time.value()) -
                     (elapsed % std::chrono::nanoseconds(frame_time.value()));

        frame_backlog += missed_frames;

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        if (frame_backlog > 0) {
            delta = std::chrono::nanoseconds(1);
        }

        timer.wait_for_expiry_after(
            delta,
            exios::use_allocator(std::bind(std::move(*this),
                                           OnNewCapture {},
                                           frame_finish + delta,
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
