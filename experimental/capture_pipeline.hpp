#ifndef SHADOW_CAST_CAPTURE_PIPELINE_HPP_INCLUDED
#define SHADOW_CAST_CAPTURE_PIPELINE_HPP_INCLUDED

#include "capture_sink.hpp"
#include "capture_source.hpp"
#include "exios/exios.hpp"
#include <functional>
#include <system_error>

namespace sc
{

template <typename T>
concept CapturePipelineCompletion =
    requires(T fn, exios::Result<std::error_code> r) { fn(r); };

template <CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source,
          CapturePipelineCompletion Completion>
struct CapturePipeline
{
    using sink_type = Sink;
    using source_type = Source;
    using result_type = exios::Result<std::error_code>;

    struct OnSourceComplete
    {
    };

    struct OnSinkComplete
    {
    };

    CapturePipeline(Sink& sink, Source& source, Completion&& completion)
        : sink_ { sink }
        , source_ { source }
        , completion_ { std::move(completion) }
        , buffer_ { sink_.prepare_input() }
    {
    }

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion_);

        source_.capture(buffer_,
                        exios::use_allocator(std::bind(std::move(*this),
                                                       OnSourceComplete {},
                                                       std::placeholders::_1),
                                             alloc));
    }

    auto operator()(OnSourceComplete,
                    source_type::completion_result_type result) -> void
    {
        if (!result) {
            std::move(completion_)(
                result_type { exios::result_error(result.error()) });
            return;
        }

        auto const alloc = exios::select_allocator(completion_);

        sink_.add(std::move(buffer_),
                  exios::use_allocator(std::bind(std::move(*this),
                                                 OnSinkComplete {},
                                                 std::placeholders::_1),
                                       alloc));
    }

    auto operator()(OnSinkComplete, Sink::completion_result_type result) -> void
    {
        if (!result)
            std::move(completion_)(
                result_type { exios::result_error(result.error()) });
        else
            std::move(completion_)(result_type {});
    }

    Sink& sink_;
    Source& source_;
    Completion completion_;
    typename Sink::input_type buffer_;
};

template <CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source,
          CapturePipelineCompletion Completion>
auto capture_pipeline(Sink& sink, Source& source, Completion&& completion)
    -> void
{
    CapturePipeline<Sink, Source, std::decay_t<Completion>> {
        sink, source, std::forward<Completion>(completion)
    }
        .initiate();
}

} // namespace sc

#endif // SHADOW_CAST_CAPTURE_PIPELINE_HPP_INCLUDED
