#ifndef SHADOW_CAST_SESSION_HPP_INCLUDED
#define SHADOW_CAST_SESSION_HPP_INCLUDED

#include "allocate_unique.hpp"
#include "audio_encoder_sink.hpp"
#include "capture.hpp"
#include "exios/exios.hpp"
#include "media_container.hpp"
#include "nvenc_encoder_sink.hpp"
#include "nvfbc_capture_source.hpp"
#include "pipewire_capture_source.hpp"
#include "utils/cmd_line.hpp"
#include <memory>

namespace sc
{

namespace detail
{

template <typename Allocator, typename Completion>
struct SessionState
{
    AllocateUniquePtr<MediaContainer, Allocator> media_container;
    Capture video_capture;
    Capture audio_capture;
    Completion completion;
    std::size_t captures_remaining { 2 };
    std::error_code error {};
};

template <typename Allocator, typename Completion>
struct SessionCompletion
{
    using SessionStateType =
        std::shared_ptr<SessionState<Allocator, Completion>>;

    auto operator()(exios::Result<std::error_code> result) -> void
    {
        state->captures_remaining -= 1;
        if (state->captures_remaining) {
            if (is_video_callback) {
                state->audio_capture.cancel();
            }
            else {
                state->video_capture.cancel();
            }
            if (result.is_error_value()) {
                state->error = result.error();
            }

            state.reset();

            return;
        }

        state->media_container->write_trailer();
        auto completion = std::move(state->completion);
        auto error = std::move(state->error);
        state.reset();

        if (error) {
            std::move(completion)(
                decltype(result) { exios::result_error(error) });
        }
        else {
            std::move(completion)(decltype(result) {});
        }
    }

    SessionStateType state;
    bool is_video_callback;
};

} // namespace detail

template <typename Desktop, typename Completion>
auto run_session(exios::Context const& execution_context,
                 exios::Event& cancel_event,
                 Desktop const& desktop,
                 Parameters const& params,
                 CUcontext cuda_ctx,
                 Completion&& completion) -> void
{
    auto const alloc = exios::select_allocator(completion);

    auto container = allocate_unique<MediaContainer>(alloc, params.output_file);
    sc::NvfbcCaptureSource video_source { execution_context,
                                          params,
                                          desktop.size() };
    sc::NvencEncoderSink video_sink {
        execution_context, cuda_ctx, *container, params, desktop.size()
    };

    sc::AudioEncoderSink audio_sink { execution_context, *container, params };
    sc::PipewireCaptureSource audio_source { execution_context,
                                             params,
                                             audio_sink.frame_size() };

    using SessionStateType = detail::SessionState<std::decay_t<decltype(alloc)>,
                                                  std::decay_t<Completion>>;

    auto state = std::allocate_shared<SessionStateType>(
        alloc,
        std::move(container),
        Capture { std::move(video_source), std::move(video_sink) },
        Capture { std::move(audio_source), std::move(audio_sink) },
        std::move(completion));

    state->media_container->write_header();

    auto& video = state->video_capture;
    auto& audio = state->audio_capture;

    cancel_event.wait_for_event([state](exios::TimerOrEventIoResult) mutable {
        state->audio_capture.cancel();
        state->video_capture.cancel();
        state.reset();
    });

    video.run(detail::SessionCompletion { state, true });
    audio.run(detail::SessionCompletion { state, false });
}

} // namespace sc

#endif // SHADOW_CAST_SESSION_HPP_INCLUDED
