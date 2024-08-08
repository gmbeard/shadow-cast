#ifndef SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED
#define SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED

#include "config.hpp"
#include "exios/exios.hpp"
#include "frame_capture.hpp"
#include "logging.hpp"
#include "metrics/metrics.hpp"
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
#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    using ClockType = std::chrono::high_resolution_clock;
    using TimePoint = decltype(ClockType::now());
    TimePoint frame_start = ClockType::now();
#endif

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
#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
        frame_start = ClockType::now();
#endif
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

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
        namespace ch = std::chrono;
        metrics::add_frame_time(
            metrics::audio_metrics,
            ch::duration_cast<ch::nanoseconds>(ClockType::now() - frame_start)
                .count());
#endif
        initiate();
    }

private:
    auto finalize(FrameCaptureResult result) -> void
    {
        /* TODO: We need a way of flushing the remaining samples
         * in the buffer, here...
         */

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
    TimePoint loop_start = frame_start;
    std::size_t frame_backlog { 0 };
    std::size_t frame_number { 0 };
    std::int64_t total_frame_time { 0 };
    bool frame_lag { false };
    std::size_t frame_lag_start { 0 };

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
        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        /* Record the required frame start time. Using `ClockType::now()` isn't
         * reliable because there may be overhead in calling the timeout
         * callback...
         */
        frame_start = next_frame_start;
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

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
        metrics::add_frame_time(
            metrics::video_metrics,
            ch::duration_cast<ch::nanoseconds>(frame_finish - frame_start)
                .count());
#endif

        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        frame_number += 1;

        std::size_t const total_duration_ns =
            ch::duration_cast<ch::nanoseconds>(frame_finish - loop_start)
                .count();

        std::size_t const expected_frames =
            (total_duration_ns + frame_time - 1) / frame_time;
        std::size_t delta = expected_frames * frame_time - total_duration_ns;

        if (expected_frames > frame_number) {
            frame_lag = true;
            delta = 0;
        }
        else {
            if (frame_lag) {
                log(LogLevel::warn,
                    "%s: Frame lag detected between frames %llu and "
                    "%llu (%llu frames). Output may contain artifacts.",
                    source.name(),
                    frame_lag_start,
                    frame_number,
                    frame_number - frame_lag_start);
            }
            frame_lag = false;
            frame_lag_start = frame_number;
        }

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
        frame_backlog -= 1;
        if (frame_backlog == 0 || !result) {
            if (!result)
                std::move(completion)(exios::Result<std::error_code> {
                    exios::result_error(result.error()) });
            else
                std::move(completion)(exios::Result<std::error_code> {});

            return;
        }

        SC_EXPECT(frame_backlog > 0);
        log(LogLevel::debug,
            "%s: Frame backlog is now %llu",
            source.name(),
            frame_backlog);
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
        auto const total_loop_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                ClockType::now() - loop_start)
                .count();

        std::size_t const target_frame_count =
            (total_loop_time_ns + frame_time - 1) / frame_time;

        log(LogLevel::debug,
            "%s: Expected frames: %llu. Actual frames: %llu",
            source.name(),
            target_frame_count,
            frame_number);
        if (target_frame_count > frame_number) {
            log(LogLevel::debug,
                "%s: Capturing %llu extra frames",
                source.name(),
                (target_frame_count - frame_number));
            frame_backlog += target_frame_count - frame_number;
        }

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
