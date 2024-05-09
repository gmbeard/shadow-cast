#ifndef SHADOW_CAST_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_CAPTURE_SOURCE_HPP_INCLUDED

#include <utility>

namespace sc
{

namespace detail
{
template <typename T>
using CompletionFn = auto(T val) -> void;
}

template <typename T, typename OutputType>
concept CaptureCompletion =
    requires(T fn, OutputType val) { fn(std::move(val)); };

template <typename T, typename Input>
concept CaptureSource = requires(
    T source,
    detail::CompletionFn<typename T::completion_result_type> completion,
    Input input) {
    typename T::completion_result_type;
    {
        source.capture(input, std::move(completion))
    };
};

} // namespace sc

#endif // SHADOW_CAST_CAPTURE_SOURCE_HPP_INCLUDED
