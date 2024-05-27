#ifndef SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED
#define SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED

#include "exios/exios.hpp"
#include "frame_capture.hpp"
#include "logging.hpp"
#include "utils/contracts.hpp"
#include <chrono>

namespace sc
{

template <typename T>
concept IntervalBasedSource = requires(T& val) {
    {
        val.timer()
    };
    {
        val.interval()
    };
};

template <typename T>
concept EventTriggeredSource = requires(T& val) {
    {
        val.event()
    };
};

template <typename T>
concept RequiresInit = requires(T& val) {
    {
        val.init()
    };
    {
        val.deinit()
    };
};

namespace detail
{
template <EventTriggeredSource Source, typename Sink, typename Completion>
struct AudioCaptureLoopOperation
{
    struct OnEvent
    {
    };

    struct OnCapturedFrame
    {
    };

    Source& source;
    Sink& sink;
    Completion completion;

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);
        source.event().wait_for_event(exios::use_allocator(
            std::bind(std::move(*this), OnEvent {}, std::placeholders::_1),
            alloc));
    }

    auto operator()(OnEvent, exios::TimerOrEventIoResult result) -> void
    {
        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        auto const alloc = exios::select_allocator(completion);
        frame_capture(source,
                      sink,
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnCapturedFrame {},
                                                     std::placeholders::_1),
                                           alloc));
    }

    auto operator()(OnCapturedFrame, FrameCaptureResult result) -> void
    {
        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        initiate();
    }

private:
    auto finalize(FrameCaptureResult result) -> void
    {
        if (!result)
            std::move(completion)(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
        else
            std::move(completion)(exios::Result<std::error_code> {});
    }
};

template <IntervalBasedSource Source, typename Sink, typename Completion>
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
    struct OnClearBacklog
    {
    };

    Source& source;
    Sink& sink;
    Completion completion;
    std::int64_t frame_time {
        std::chrono::duration_cast<std::chrono::nanoseconds>(source.interval())
            .count()
    };
    TimePoint frame_start = ClockType::now();
    std::size_t frame_backlog { 0 };
    std::size_t frame_number { 0 };
    std::int64_t total_frame_time { 0 };

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);
        frame_capture(source,
                      sink,
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnCapturedFrame {},
                                                     std::placeholders::_1),
                                           alloc));
    }

    auto operator()(OnNewCapture,
                    TimePoint next_frame_start,
                    exios::TimerOrEventIoResult result) -> void
    {
        /* Record the required frame start time. Using `ClockType::now()` isn't
         * reliable because there may be overhead in calling the timeout
         * callback...
         */
        frame_start = next_frame_start;
        if (frame_backlog > 0)
            frame_backlog -= 1;

        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        frame_capture(source,
                      sink,
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnCapturedFrame {},
                                                     std::placeholders::_1),
                                           alloc));
    }

    auto operator()(OnCapturedFrame, FrameCaptureResult result) -> void
    {
        namespace ch = std::chrono;

        auto const frame_finish = ClockType::now();
        /* Elapsed time accounts for the time it took for a frame to be
         * captured, plus the overhead of the I/O system...
         */
        auto const elapsed =
            ch::duration_cast<ch::nanoseconds>(frame_finish - frame_start)
                .count();

        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        /* Track how many frames we missed in the last interval. This is where
         * the time took to process the last frame exceeded the frame interval
         * value...
         */
        std::size_t const missed_frames = elapsed / frame_time;

        if (missed_frames > 0) {
            log(LogLevel::warn,
                "Frame %llu took %lluns and missed %llu frame%s (Source="
                "%lluns, Sink=%lluns). Attempting to catch up.",
                frame_number,
                elapsed,
                missed_frames,
                (missed_frames > 1 ? "s" : ""),
                result.value().capture_duration.count(),
                result.value().sink_write_duration.count());
        }

        frame_backlog += missed_frames;

        /* Work out how much to wait until the next frame...
         */
        auto delta = frame_time - (elapsed - (missed_frames * frame_time));

        /* If we have a frame backlog then we're behind, so capture a new
         * frame immediately without waiting for the next interval...
         */
        if (frame_backlog > 0) {
            delta = 0;
        }

        frame_number += 1;

        auto const alloc = exios::select_allocator(completion);
        source.timer().wait_for_expiry_after(
            ch::nanoseconds(delta),
            exios::use_allocator(
                std::bind(std::move(*this),
                          OnNewCapture {},
                          /* We specify when we _want_ the next frame to start;
                           * The timeout may happen later that when we asked
                           * for, so this ensures the delay is accounted for...
                           */
                          frame_finish + ch::nanoseconds(delta),
                          std::placeholders::_1),
                alloc));
    }

    auto operator()(OnClearBacklog, FrameCaptureResult result) -> void
    {
        if (frame_backlog == 0 || !result) {
            if (!result)
                std::move(completion)(exios::Result<std::error_code> {
                    exios::result_error(result.error()) });
            else
                std::move(completion)(exios::Result<std::error_code> {});

            return;
        }

        SC_EXPECT(frame_backlog > 0);
        frame_backlog -= 1;
        auto const alloc = exios::select_allocator(completion);

        frame_capture(source,
                      sink,
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnClearBacklog {},
                                                     std::placeholders::_1),
                                           alloc));
    }

private:
    auto finalize(FrameCaptureResult result) -> void
    {
        if (!result && result.error() == std::errc::operation_canceled &&
            frame_backlog > 0) {
            log(LogLevel::info,
                "%s: Clearing backlog of %llu frames",
                source.name(),
                frame_backlog);

            auto const alloc = exios::select_allocator(completion);
            frame_capture(source,
                          sink,
                          exios::use_allocator(std::bind(std::move(*this),
                                                         OnClearBacklog {},
                                                         std::placeholders::_1),
                                               alloc));
            return;
        }

        if (!result)
            std::move(completion)(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
        else
            std::move(completion)(exios::Result<std::error_code> {});
    }
};

} // namespace detail

template <typename Source, typename Sink, typename Completion>
auto frame_capture_loop(Source& source, Sink& sink, Completion&& completion)
    -> void
requires(IntervalBasedSource<Source> || EventTriggeredSource<Source>)
{
    if constexpr (RequiresInit<Source>) {
        log(LogLevel::info, "Initializing %s", source.name());
        source.init();
    }

    auto const alloc = exios::select_allocator(completion);
    auto fn = [&source, &sink, completion = std::move(completion)](
                  exios::Result<std::error_code> result) mutable {
        if constexpr (RequiresInit<Source>) {
            log(LogLevel::info, "Uninitializing %s source", source.name());
            source.deinit();
        }

        if (!result && result.error() != std::errc::operation_canceled) {
            log(LogLevel::error,
                "%s exited with an error: %llu",
                source.name(),
                result.error());

            std::move(completion)(std::move(result));
        }
        else {
            log(LogLevel::info,
                "%s exited normally. Flushing output stream.",
                source.name());
            sink.flush(std::move(completion));
        }
    };

    if constexpr (IntervalBasedSource<Source>) {
        detail::VideoCaptureLoopOperation(
            source, sink, exios::use_allocator(std::move(fn), alloc))
            .initiate();
    }
    else {
        detail::AudioCaptureLoopOperation(
            source, sink, exios::use_allocator(std::move(fn), alloc))
            .initiate();
    }
}

} // namespace sc

#endif // SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED
