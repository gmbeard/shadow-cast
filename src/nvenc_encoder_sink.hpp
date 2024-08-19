#ifndef SHADOW_CAST_HW_ENCODER_CAPTURE_SINK_HPP_INCLUDED
#define SHADOW_CAST_HW_ENCODER_CAPTURE_SINK_HPP_INCLUDED

#include "av/codec.hpp"
#include "av/frame.hpp"
#include "capture_sink.hpp"
#include "exios/exios.hpp"
#include "media_container.hpp"
#include "utils/cmd_line.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

namespace sc
{

struct NvencEncoderSink
{
    enum class PixelFormat
    {
        rgba,
        bgra,
    };

    NvencEncoderSink(exios::Context ctx,
                     CUcontext cuda_ctx,
                     MediaContainer& container,
                     Parameters const& params,
                     VideoOutputSize desktop_resolution,
                     PixelFormat pixel_format);

    using input_type = AVFrame*;
    using completion_result_type = exios::Result<std::error_code>;

    auto prepare() -> input_type;

    template <AddToSinkCompletion<completion_result_type> Completion>
    auto write(input_type input, Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        auto fn = [input,
                   &container = container_,
                   encoder_context = encoder_context_.get(),
                   completion = std::move(completion)]() mutable {
            SC_SCOPE_GUARD([&] { av_frame_unref(input); });
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

private:
    exios::Context ctx_;
    MediaContainer& container_;
    CodecContextPtr encoder_context_;
    FramePtr frame_;
};

} // namespace sc
#endif // SHADOW_CAST_HW_ENCODER_CAPTURE_SINK_HPP_INCLUDED
