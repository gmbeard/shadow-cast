#ifndef SHADOW_CAST_CAPTURE_SINK_HPP_INCLUDED
#define SHADOW_CAST_CAPTURE_SINK_HPP_INCLUDED

#include <concepts>
#include <utility>

namespace sc
{

namespace detail
{
template <typename T>
using AddToSinkCompletionFn = auto(T val) -> void;
}

template <typename T, typename ResultType>
concept AddToSinkCompletion =
    requires(T fn, ResultType r) { fn(std::move(r)); };

template <typename T>
concept CaptureSink =
    requires(T sink,
             typename T::input_type input,
             detail::AddToSinkCompletionFn<typename T::completion_result_type>
                 completion) {
        typename T::input_type;
        typename T::completion_result_type;

        {
            sink.add(std::move(input), std::move(completion))
        };

        {
            sink.prepare_input()
        } -> std::same_as<typename T::input_type>;

        {
            sink.flush(std::move(completion))
        };
    };

} // namespace sc
#endif // SHADOW_CAST_CAPTURE_SINK_HPP_INCLUDED
