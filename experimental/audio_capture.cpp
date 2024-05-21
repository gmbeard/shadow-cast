#include "audio_capture.hpp"
#include "pipewire_capture_source.hpp"

namespace sc
{
AudioCapture::AudioCapture(AudioEncoderSink&& sink,
                           PipewireCaptureSource&& source)
    : state_ { std::make_unique<AudioCaptureState>(std::move(sink),
                                                   std::move(source)) }
{
}

auto AudioCapture::cancel() noexcept -> void { state_->source.cancel(); }

} // namespace sc
