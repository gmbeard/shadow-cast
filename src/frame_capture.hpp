#ifndef SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED

#include "exios/exios.hpp"
#include "frame_timer.hpp"
#include <chrono>
#include <functional>
#include <system_error>
#include <type_traits>
#include <variant>

namespace sc
{
struct FrameCaptureMetrics
{
    std::chrono::nanoseconds capture_duration { 0 };
    std::chrono::nanoseconds sink_write_duration { 0 };
    std::uint32_t phase_offset { 0 };
};

struct frame_capture_out_of_phase_error
{
    std::uint32_t phase_offset { 0 };
};

using frame_capture_error =
    std::variant<frame_capture_out_of_phase_error, std::error_code>;

using FrameCaptureResult =
    exios::Result<FrameCaptureMetrics, frame_capture_error>;

template <typename T>
struct throws_out_of_phase_error : std::false_type
{
};

template <>
struct throws_out_of_phase_error<frame_capture_out_of_phase_error>
    : std::true_type
{
};

template <typename T>
struct is_phase_feedback_result : std::false_type
{
};

template <typename T>
struct is_phase_feedback_result<std::pair<T, std::uint32_t>> : std::true_type
{
};

template <typename T>
static constexpr bool is_phase_feedback_result_v =
    is_phase_feedback_result<T>::value;

template <typename T>
static constexpr bool throws_out_of_phase_error_v =
    throws_out_of_phase_error<T>::value;

template <typename Source, typename Sink, typename Map, typename Completion>
struct FrameCaptureOperation
{
    using ClockType = std::chrono::high_resolution_clock;

    struct OnSourceCaptured
    {
    };

    explicit FrameCaptureOperation(Source& source,
                                   Sink& sink,
                                   frame_timer frame_budget,
                                   Map map,
                                   Completion completion) noexcept
        : source_ { source }
        , sink_ { sink }
        , frame_budget_ { frame_budget }
        , map_ { std::move(map) }
        , completion_ { std::move(completion) }
    {
    }

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion_);
        auto const frame_budget = frame_budget_;
        auto input = sink_.prepare();
        source_.capture(input,
                        frame_budget,
                        exios::use_allocator(std::bind(std::move(*this),
                                                       OnSourceCaptured {},
                                                       ClockType::now(),
                                                       input,
                                                       std::placeholders::_1),
                                             alloc));
    }

    template <typename SourceResult>
    requires(throws_out_of_phase_error_v<
             typename Source::CaptureResultType::ErrorType>)
    auto operator()(OnSourceCaptured,
                    ClockType::time_point capture_start,
                    Sink::input_type input,
                    SourceResult result) -> void
    {
        auto const capture_duration = ClockType::now() - capture_start;

        if (result.is_error_value()) {
            sink_.discard(input);
            std::move(completion_)(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        map_(result.value());

        auto const alloc = exios::select_allocator(completion_);
        auto fn = [completion = std::move(completion_),
                   capture_duration,
                   sink_write_start =
                       ClockType::now()](auto sink_result) mutable {
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
        };
        sink_.write(std::move(result.value()),
                    exios::use_allocator(std::move(fn), alloc));
    }

    template <typename SourceResult>
    requires(!throws_out_of_phase_error_v<
             typename Source::CaptureResultType::ErrorType>)
    auto operator()(OnSourceCaptured,
                    ClockType::time_point capture_start,
                    Sink::input_type input,
                    SourceResult result) -> void
    {
        auto const capture_duration = ClockType::now() - capture_start;

        if (result.is_error_value()) {
            sink_.discard(input);
            std::move(completion_)(
                FrameCaptureResult { exios::result_error(result.error()) });
            return;
        }

        map_(result.value());

        auto const alloc = exios::select_allocator(completion_);
        auto fn = [completion = std::move(completion_),
                   capture_duration,
                   sink_write_start =
                       ClockType::now()](auto sink_result) mutable {
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
        };
        sink_.write(std::move(result.value()),
                    exios::use_allocator(std::move(fn), alloc));
    }

    Source& source_;
    Sink& sink_;
    frame_timer frame_budget_;
    Map map_;
    Completion completion_;
};

template <typename Source, typename Sink, typename Map, typename Completion>
auto frame_capture(Source& source,
                   Sink& sink,
                   frame_timer frame_budget,
                   Map&& map,
                   Completion&& completion) -> void
{
    FrameCaptureOperation(
        source, sink, frame_budget, std::move(map), std::move(completion))
        .initiate();
}

}; // namespace sc

#endif // SHADOW_CAST_FRAME_CAPTURE_HPP_INCLUDED
