#ifndef SHADOW_CAST_AUDIO_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_AUDIO_CAPTURE_HPP_INCLUDED

#include "audio_capture_loop_operation.hpp"
#include "audio_encoder_sink.hpp"
#include "pipewire_capture_source.hpp"
#include <memory>

namespace sc
{
struct AudioCapture
{
    struct AudioCaptureState
    {
        AudioEncoderSink sink;
        PipewireCaptureSource source;
    };

    AudioCapture(AudioEncoderSink&& sink, PipewireCaptureSource&& source);

    auto cancel() noexcept -> void;

    template <typename Completion>
    auto run(Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        state_->source.start_audio_stream();
        AudioCaptureLoopOperation(
            state_->sink,
            state_->source,
            exios::use_allocator(
                [&source = state_->source,
                 completion = std::move(completion)](auto result) mutable {
                    source.stop_audio_stream();
                    std::move(completion)(std::move(result));
                },
                alloc))
            .initiate();
    }

private:
    std::unique_ptr<AudioCaptureState> state_;
};

} // namespace sc
#endif // SHADOW_CAST_AUDIO_CAPTURE_HPP_INCLUDED
