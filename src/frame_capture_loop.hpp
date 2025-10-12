#ifndef SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED
#define SHADOW_CAST_FRAME_CAPTURE_LOOP_HPP_INCLUDED

#include "config.hpp"
#include "cpu_usage.hpp"
#include "exios/exios.hpp"
#include "frame_capture.hpp"
#include "frame_timer.hpp"
#include "logging.hpp"
#include <cstddef>
#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
#include "cpu_usage.hpp"
#include "metrics/metrics.hpp"
#endif
#include "utils/contracts.hpp"
#include <chrono>

namespace sc
{

constexpr std::size_t const kFrameLagWarningLevel = 1;

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
        frame_capture(
            source,
            sink,
            frame_timer(std::chrono::seconds(1)),
            [](auto&) {},
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

struct SetFramePTS
{
    explicit SetFramePTS(frame_timer timer) noexcept
        : timer_ { timer }
    {
    }

    template <typename Frame>
    auto operator()(Frame* frame) const noexcept -> void
    {
        frame->pts = std::chrono::duration_cast<std::chrono::microseconds>(
                         timer_.elapsed())
                         .count();
    }

    frame_timer timer_;
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
    std::uint64_t cpu_time = get_cpu_usage();
    std::size_t frame_backlog { 0 };
    std::size_t frame_number { 0 };
    std::int64_t total_frame_time { 0 };
    std::size_t frame_lag { 0 };
    std::size_t frame_lag_start { 0 };
    frame_timer frame_timer_ { source.interval(), loop_start };

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);
        frame_capture(source,
                      sink,
                      sc::frame_timer(source.interval(), frame_start),
                      SetFramePTS(frame_timer_),
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnCapturedFrame {},
                                                     std::placeholders::_1),
                                           alloc));
    }

    auto operator()(OnNewCapture, exios::TimerOrEventIoResult result) -> void
    {
        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        /* Record the required frame start time...
         */
        frame_start = frame_timer_.now();
        auto const alloc =
            exios::select_allocator(completion, std::allocator<void> {});

        cpu_time = get_cpu_usage();
        frame_capture(source,
                      sink,
                      sc::frame_timer(source.interval(), frame_start),
                      SetFramePTS(frame_timer_),
                      exios::use_allocator(std::bind(std::move(*this),
                                                     OnCapturedFrame {},
                                                     std::placeholders::_1),
                                           alloc));
    }

    auto operator()(OnCapturedFrame, FrameCaptureResult result) -> void
    {
        namespace ch = std::chrono;

        auto const frame_finish = frame_timer_.now();

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
        auto const metrics_elapsed_ns =
            ch::duration_cast<ch::nanoseconds>(frame_finish - frame_start)
                .count();
        auto const metrics_total_ns =
            ch::duration_cast<ch::nanoseconds>(frame_finish - loop_start)
                .count();
        metrics::add_frame_time(metrics::video_metrics, metrics_elapsed_ns);
        metrics::add_frame_time(
            metrics::cpu_metrics,
            static_cast<std::size_t>(
                static_cast<float>(get_cpu_usage() - cpu_time) /
                metrics_total_ns * 1000));
#endif

        if (!result) {
            finalize(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        frame_timer_.increment_frame_number();

        auto const next_frame_wait_duration =
            frame_timer_.duration_until_next_frame_from(frame_finish);
        auto const expected_frame_number =
            frame_timer_.expected_frame_number_at(frame_finish);
        auto const actual_frame_number = frame_timer_.frame_number();

        if (expected_frame_number > actual_frame_number) {
            std::ptrdiff_t const lag =
                expected_frame_number - actual_frame_number;
            log(LogLevel::warn,
                "%s: Lagging behind by %ti frame(s) at frame %llu",
                source.name(),
                lag,
                actual_frame_number);
            frame_timer_.increment_frame_number(lag);
        }

        auto const alloc = exios::select_allocator(completion);
        source.timer().wait_for_expiry_after(
            next_frame_wait_duration,
            exios::use_allocator(std::bind(std::move(*this),
                                           OnNewCapture {},
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

        if (!result && result.error() != std::errc::operation_canceled)
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
