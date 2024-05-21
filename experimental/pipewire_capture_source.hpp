#ifndef SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED
#define SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED

#include "av/media_chunk.hpp"
#include "av/sample_format.hpp"
#include "capture_source.hpp"
#include "exios/exios.hpp"
#include "utils/cmd_line.hpp"
#include "utils/contracts.hpp"
#include <libavutil/frame.h>
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
    exios::Event audio_buffer_event;
    std::size_t audio_buffer_high_watermark;

    SampleFormat required_sample_format { SampleFormat::float_planar };
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
                      std::size_t samples) noexcept -> void;

} // namespace detail

struct PipewireCaptureSource
{
    explicit PipewireCaptureSource(exios::Context const&,
                                   Parameters const&,
                                   std::size_t frame_size);

    using completion_result_type = exios::Result<std::error_code>;

    auto start_audio_stream() -> void;
    auto stop_audio_stream() -> void;
    auto cancel() noexcept -> void;

    template <CaptureCompletion<completion_result_type> Completion>
    auto capture(AVFrame* frame, Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);

        SC_EXPECT(frame->nb_samples > 0);

        auto& state = *state_;

        state.audio_buffer_event.wait_for_event(exios::use_allocator(
            [frame, completion = std::move(completion), &state](
                auto result) mutable {
                if (!result) {
                    std::move(completion)(completion_result_type {
                        exios::result_error(result.error()) });
                    return;
                }

                {
                    std::unique_lock lock { state.audio_data_mutex };
                    SC_EXPECT(state.audio_data.sample_count >=
                              static_cast<std::size_t>(frame->nb_samples));
                    frame->pts = state.samples_written;
                    detail::transfer_samples(
                        state.audio_data,
                        *frame,
                        static_cast<std::size_t>(frame->nb_samples));
                    state.samples_written += frame->nb_samples;
                }

                std::move(completion)(completion_result_type {});
            },
            alloc));
    }

private:
    static void on_process(void* userdata);
    static void on_stream_param_changed(void* _data,
                                        uint32_t id,
                                        const struct spa_pod* param);

    std::unique_ptr<detail::PipewireCaptureSourceState> state_;
};

} // namespace sc

#endif // SHADOW_CAST_PIPEWIRE_CAPTURE_SOURCE_HPP_INCLUDED
