#ifndef SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED

#include "exios/exios.hpp"
#include <chrono>
#include <functional>
#include <system_error>

namespace sc
{
struct FrameCaptureMetrics
{
    std::chrono::nanoseconds capture_duration { 0 };
    std::chrono::nanoseconds sink_write_duration { 0 };
};

using FrameCaptureResult = exios::Result<FrameCaptureMetrics, std::error_code>;

template <typename Source, typename Sink, typename Completion>
struct FrameCaptureOperation
{
    using ClockType = std::chrono::high_resolution_clock;

    struct OnSourceCaptured
    {
    };

    explicit FrameCaptureOperation(Source& source,
                                   Sink& sink,
                                   Completion completion) noexcept
        : source_ { source }
        , sink_ { sink }
        , completion_ { std::move(completion) }
    {
    }

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion_);
        source_.capture(sink_.prepare(),
                        exios::use_allocator(std::bind(std::move(*this),
                                                       OnSourceCaptured {},
                                                       ClockType::now(),
                                                       std::placeholders::_1),
                                             alloc));
    }

    template <typename SourceResult>
    auto operator()(OnSourceCaptured,
                    ClockType::time_point capture_start,
                    SourceResult result) -> void
    {
        auto const capture_duration = ClockType::now() - capture_start;

        if (result.is_error_value()) {
            std::move(completion_)(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        auto const alloc = exios::select_allocator(completion_);
        sink_.write(
            std::move(result.value()),
            [completion = std::move(completion_),
             capture_duration,
             sink_write_start = ClockType::now()](auto sink_result) mutable {
                std::chrono::nanoseconds const sink_write_duration =
                    ClockType::now() - sink_write_start;
                if (sink_result.is_error_value()) {
                    std::move(completion)(FrameCaptureResult {
                        exios::result_error(sink_result.error()) });
                    return;
                }

                std::move(completion)(
                    FrameCaptureResult { exios::result_ok(FrameCaptureMetrics {
                        .capture_duration = capture_duration,
                        .sink_write_duration = sink_write_duration }) });
            });
    }

    Source& source_;
    Sink& sink_;
    Completion completion_;
};

template <typename Source, typename Sink, typename Completion>
auto frame_capture(Source& source, Sink& sink, Completion&& completion) -> void
{
    FrameCaptureOperation(source, sink, std::move(completion)).initiate();
}

}; // namespace sc

#endif // SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED
