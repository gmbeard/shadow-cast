#include "pipewire_capture_source.hpp"
#include "utils/contracts.hpp"
#include "utils/elapsed.hpp"
#include "utils/scope_guard.hpp"

namespace
{
pw_stream_events stream_events {};
}

namespace sc
{

PipewireCaptureSource::PipewireCaptureSource(exios::Context const& ctx,
                                             Parameters const& params,
                                             std::size_t frame_size)
    : state_ { std::make_unique<detail::PipewireCaptureSourceState>(
          exios::Event { ctx, exios::semaphone_mode }, frame_size) }
{
    state_->required_sample_rate = params.sample_rate;
}

auto PipewireCaptureSource::start_audio_stream() -> void
{
    stream_events = { .version = PW_VERSION_STREAM_EVENTS,
                      .destroy = nullptr,
                      .state_changed = nullptr,
                      .control_info = nullptr,
                      .io_changed = nullptr,
                      .param_changed = on_stream_param_changed,
                      .add_buffer = nullptr,
                      .remove_buffer = nullptr,
                      .process = on_process,
                      .drained = nullptr,
                      .command = nullptr,
                      .trigger_done = nullptr };

    std::array<spa_pod const*, 2> params {};
    uint8_t buffer[1024];
    struct pw_properties* props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    /* make a main loop. If you already have another main loop, you can add
     * the fd of this pipewire mainloop to it. */
    state_->loop = pw_thread_loop_new("shadow-cast-audio-capture", nullptr);

    pw_thread_loop_lock(state_->loop);

    /* Create a simple stream, the simple stream manages the core and remote
     * objects for you if you don't need to deal with them.
     *
     * If you plan to autoconnect your stream, you need to provide at least
     * media, category and role properties.
     *
     * Pass your events and a user_data pointer as the last arguments. This
     * will inform you about the stream state. The most important event
     * you need to listen to is the process event where you need to produce
     * the data.
     */
    props = pw_properties_new(PW_KEY_MEDIA_TYPE,
                              "Audio",
                              PW_KEY_MEDIA_CATEGORY,
                              "Capture",
                              PW_KEY_MEDIA_ROLE,
                              "Music",
                              NULL);

    /* uncomment if you want to capture from the sink monitor ports */
    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

    state_->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state_->loop),
                                          "audio-capture",
                                          props,
                                          &stream_events,
                                          state_.get());

    /* Make one parameter with the supported formats. The
     * SPA_PARAM_EnumFormat id means that this is a format enumeration (of 1
     * value). We leave the channels and rate empty to accept the native
     * graph rate and channels. */
    spa_audio_info_raw raw_init = {};
    raw_init.format =
        convert_to_pipewire_format(state_->required_sample_format);
    raw_init.rate = state_->required_sample_rate;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw_init);

    /* Now connect this stream.
     */
    if (pw_stream_connect(
            state_->stream,
            PW_DIRECTION_INPUT,
            PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                         PW_STREAM_FLAG_MAP_BUFFERS),
            params.data(),
            params.size()) < 0) {
        fprintf(stderr, "Couldn't connect stream\n");
        return;
    }

    pw_thread_loop_unlock(state_->loop);
    pw_thread_loop_start(state_->loop);
}

auto PipewireCaptureSource::stop_audio_stream() -> void
{
    pw_thread_loop_stop(state_->loop);
    pw_stream_destroy(state_->stream);
    pw_thread_loop_destroy(state_->loop);
}

auto PipewireCaptureSource::cancel() noexcept -> void
{
    state_->audio_buffer_event.cancel();
    state_->cancelled = true;
}

void PipewireCaptureSource::on_process(void* userdata)
{
    SC_EXPECT(userdata);
    detail::PipewireCaptureSourceState& state =
        // cppcheck-suppress [nullPointerRedundantCheck]
        *reinterpret_cast<detail::PipewireCaptureSourceState*>(userdata);

    if (state.cancelled)
        return;

    struct pw_buffer* b;

    if ((b = pw_stream_dequeue_buffer(state.stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    SC_SCOPE_GUARD([&] { pw_stream_queue_buffer(state.stream, b); });

    spa_buffer* buf = b->buffer;

    if (!buf || !buf->datas[0].data) {
        pw_log_warn("No data in buffer\n");
        return;
    }

    auto const sample_size = sample_format_size(state.required_sample_format);
    auto const num_bytes = buf->datas[0].chunk->size;
    auto const num_samples = is_interleaved_format(state.required_sample_format)
                                 ? (num_bytes / 2) / sample_size
                                 : num_bytes / sample_size;

    sc::MediaChunk& chunk = state.audio_data;

    {
        std::unique_lock lock { state.audio_data_mutex };

        /* TODO:
         *  We don't really need to record the timestamp do we??
         */
        chunk.timestamp_ms = sc::global_elapsed.value();
        chunk.sample_count += num_samples;

        std::span channel_data { buf->datas, buf->n_datas };
        while (chunk.channel_buffers().size() < channel_data.size())
            chunk.channel_buffers().push_back(sc::DynamicBuffer {});

        auto out_buffer_it = chunk.channel_buffers().begin();

        for (auto const& d : channel_data) {
            sc::DynamicBuffer& chunk_buffer = *out_buffer_it++;
            auto target_bytes = chunk_buffer.prepare(num_bytes);

            std::span source_bytes {
                reinterpret_cast<std::uint8_t const*>(d.data), num_bytes
            };

            std::copy(
                begin(source_bytes), end(source_bytes), begin(target_bytes));
            chunk_buffer.commit(num_bytes);
        }

        state.frame_samples_written += num_samples;

        if (state.frame_samples_written >= state.audio_buffer_high_watermark) {
            auto const frames_ready =
                state.frame_samples_written / state.audio_buffer_high_watermark;
            state.frame_samples_written %= state.audio_buffer_high_watermark;

            state.audio_buffer_event.trigger_with_value(frames_ready,
                                                        [](auto) {});
        }
    }
}

void PipewireCaptureSource::on_stream_param_changed(void* data,
                                                    uint32_t id,
                                                    const struct spa_pod* param)
{
    SC_EXPECT(data);
    detail::PipewireCaptureSourceState& state =
        // cppcheck-suppress [nullPointerRedundantCheck]
        *reinterpret_cast<detail::PipewireCaptureSourceState*>(data);

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(
            param, &state.format.media_type, &state.format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (state.format.media_type != SPA_MEDIA_TYPE_audio ||
        state.format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &state.format.info.raw);

    /* TODO:
     *  Get rid of this console printing and replace with a
     *  proper logging system...
     */
    fprintf(stdout,
            "capturing rate: %d, format: 0x%x, channels: %d\n",
            state.format.info.raw.rate,
            state.format.info.raw.format,
            state.format.info.raw.channels);
}

namespace detail
{
auto transfer_samples(MediaChunk& buffer,
                      AVFrame& target,
                      std::size_t samples) noexcept -> void
{
    /* FIXME:
     *  We're assuming planar sample data. Make sure we can handle
     * interleaved too!
     */
    SC_EXPECT(buffer.channel_buffers().size() > 0);
    SC_EXPECT(target.channels > 0);
    SC_EXPECT(buffer.channel_buffers().size() ==
              static_cast<std::size_t>(target.channels));

    /* FIXME:
     *  We're only dealing with float-sized samples. Make sure we can
     * handle other sample formats too!
     */
    auto const num_bytes = samples * sizeof(float);

    std::span dest_channels { target.data,
                              static_cast<std::size_t>(target.channels) };

    auto out_it = dest_channels.begin();

    std::for_each(buffer.channel_buffers().begin(),
                  buffer.channel_buffers().end(),
                  [&](auto& source_buffer) {
                      SC_EXPECT(source_buffer.data().size() >= num_bytes);
                      std::copy_n(
                          source_buffer.data().begin(), num_bytes, *out_it++);
                      source_buffer.consume(num_bytes);
                  });

    buffer.sample_count -= samples;
}
} // namespace detail

} // namespace sc
