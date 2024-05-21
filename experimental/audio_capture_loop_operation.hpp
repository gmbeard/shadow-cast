#ifndef SHADOW_CAST_AUDIO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
#define SHADOW_CAST_AUDIO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED

#include "capture_pipeline.hpp"
#include "capture_sink.hpp"
#include "capture_source.hpp"
#include "exios/exios.hpp"
#include <functional>
#include <system_error>

namespace sc
{
template <typename T>
concept AudioCaptureLoopOperationCompletion =
    requires(T val, exios::Result<std::error_code> result) {
        {
            val(result)
        };
    };

template <CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source,
          AudioCaptureLoopOperationCompletion Completion>
struct AudioCaptureLoopOperation
{
    using loop_result_type = exios::Result<Timing, std::error_code>;

    struct OnCapturedFrame
    {
    };

    Sink& sink;
    Source& source;
    Completion completion;

    auto initiate() -> void
    {
        auto const alloc = exios::select_allocator(completion);

        capture_pipeline(sink,
                         source,
                         exios::use_allocator(std::bind(std::move(*this),
                                                        OnCapturedFrame {},
                                                        std::placeholders::_1),
                                              alloc));
    }

    auto operator()(OnCapturedFrame, loop_result_type result) -> void
    {
        if (!result) {
            finalize(exios::Result<std::error_code> {
                exios::result_error(result.error()) });
            return;
        }

        initiate();
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

#endif // SHADOW_CAST_AUDIO_CAPTURE_LOOP_OPERATION_HPP_INCLUDED
