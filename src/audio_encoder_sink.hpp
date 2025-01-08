#ifndef SHADOW_CAST_AUDIO_ENCODER_SINK_HPP_INCLUDED
#define SHADOW_CAST_AUDIO_ENCODER_SINK_HPP_INCLUDED

#include "av/codec.hpp"
#include "av/frame.hpp"
#include "av/sample_format.hpp"
#include "capture_sink.hpp"
#include "exios/exios.hpp"
#include "media_container.hpp"
#include "utils/cmd_line.hpp"

namespace sc
{
struct AudioEncoderSink
{
    using input_type = AVFrame*;
    using completion_result_type = exios::Result<std::error_code>;

    AudioEncoderSink(exios::Context ctx,
                     MediaContainer& container,
                     Parameters const& params);

    auto prepare() -> input_type;

    template <AddToSinkCompletion<completion_result_type> Completion>
    auto write(input_type input, Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        auto fn = [input,
                   &container = container_,
                   encoder_context = encoder_context_.get(),
                   completion = std::move(completion)]() mutable {
            // SC_SCOPE_GUARD([&] { av_frame_unref(input); });
            container.write_frame(input, encoder_context);
            std::move(completion)(exios::Result<std::error_code> {});
        };

        ctx_.post(std::move(fn), alloc);
    }

    template <AddToSinkCompletion<completion_result_type> Completion>
    auto flush(Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        auto fn = [&container = container_,
                   encoder_context = encoder_context_.get(),
                   completion = std::move(completion)]() mutable {
            container.write_frame(nullptr, encoder_context);
            std::move(completion)(exios::Result<std::error_code> {});
        };

        ctx_.post(std::move(fn), alloc);
    }

    auto frame_size() const noexcept -> std::size_t;
    auto sample_format() const -> SampleFormat;

private:
    exios::Context ctx_;
    MediaContainer& container_;
    CodecContextPtr encoder_context_;
    FramePtr frame_;
};

} // namespace sc

#endif // SHADOW_CAST_AUDIO_ENCODER_SINK_HPP_INCLUDED
