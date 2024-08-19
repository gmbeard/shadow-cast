#ifndef SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED

#include "av/media_chunk.hpp"
#include "av/sample_format.hpp"
#include "capture_source.hpp"
#include "exios/exios.hpp"
#include "sticky_cancel_event.hpp"
#include "utils/cmd_line.hpp"
#include "utils/contracts.hpp"
#include <memory>
#include <mutex>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <system_error>

namespace sc
{

namespace detail
{

struct PipewireCaptureSourceState
{
    StickyCancelEvent audio_buffer_event;
    std::size_t audio_buffer_high_watermark;

    SampleFormat required_sample_format;
    std::size_t required_sample_rate { 48'000 };
    std::mutex audio_data_mutex {};
    std::atomic_bool cancelled { false };
    MediaChunk audio_data;
    pw_thread_loop* loop { nullptr };
    pw_stream* stream { nullptr };
    spa_audio_info format {};
    std::size_t samples_written { 0 };
    std::size_t frame_samples_written { 0 };
};

auto transfer_samples(MediaChunk& buffer,
                      AVFrame& target,
                      std::size_t samples,
                      SampleFormat sample_format) noexcept -> void;

auto prepare_buffer_channels(sc::MediaChunk& buffer,
                             sc::SampleFormat sample_format,
                             std::size_t num_channels = 2) -> void;
} // namespace detail

struct PipewireCaptureSource
{
    explicit PipewireCaptureSource(exios::Context const&,
                                   Parameters const&,
                                   std::size_t frame_size,
                                   SampleFormat sample_format);

    using CaptureResultType = exios::Result<AVFrame*, std::error_code>;

    auto context() const noexcept -> exios::Context const&;
    auto event() noexcept -> StickyCancelEvent&;
    auto init() -> void;
    auto deinit() -> void;
    auto start_audio_stream() -> void;
    auto stop_audio_stream() -> void;
    auto cancel() noexcept -> void;
    static constexpr auto name() noexcept -> char const*
    {
        return "Pipewire capture";
    };

    template <CaptureCompletion<CaptureResultType> Completion>
    auto capture(AVFrame* frame, Completion&& completion) -> void
    {
        SC_EXPECT(frame->nb_samples > 0);

        auto f = [frame,
                  completion = std::move(completion),
                  &state = *state_]() mutable {
            {
                std::unique_lock lock { state.audio_data_mutex };
                SC_EXPECT(state.audio_data.sample_count >=
                          static_cast<std::size_t>(frame->nb_samples));
                frame->pts = state.samples_written;
                detail::transfer_samples(
                    state.audio_data,
                    *frame,
                    static_cast<std::size_t>(frame->nb_samples),
                    state.required_sample_format);
                state.samples_written += frame->nb_samples;
            }

            std::move(completion)(
                CaptureResultType { exios::result_ok(frame) });
        };

        context_.post(std::move(f), exios::select_allocator(completion));
    }

private:
    static void on_process(void* userdata);
    static void on_stream_param_changed(void* _data,
                                        uint32_t id,
                                        const struct spa_pod* param);

    exios::Context context_;
    StickyCancelEvent event_;
    std::unique_ptr<detail::PipewireCaptureSourceState> state_;
};

} // namespace sc

#endif // SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED
