#include "./audio_service.hpp"
#include "./elapsed.hpp"
#include <array>
#include <errno.h>
#include <fcntl.h>
#include <span>
#include <system_error>
#include <unistd.h>

namespace
{

auto constexpr kPipeRead = 0;
auto constexpr kPipeWrite = 1;

auto discard_pipe_data(int pipe_read_fd, std::size_t n)
{
    std::array<std::uint8_t, 32> discard_buffer {};
    while (n > 0) {
        auto n_read = read(pipe_read_fd,
                           discard_buffer.data(),
                           std::min(n, discard_buffer.size()));
        if (n_read < 0)
            break;

        n -= n_read;
    }
}

} // namespace

namespace
{

struct RequeueBufferGuard
{
    pw_buffer* buf;
    pw_stream* stream;

    ~RequeueBufferGuard() { pw_stream_queue_buffer(stream, buf); }
};

/* our data processing function is in general:
 *
 *  struct pw_buffer *b;
 *  b = pw_stream_dequeue_buffer(stream);
 *
 *  .. consume stuff in the buffer ...
 *
 *  pw_stream_queue_buffer(stream, b);
 */
static void on_process(void* userdata)
{
    using SampleType = float;

    sc::AudioLoopData* data = reinterpret_cast<sc::AudioLoopData*>(userdata);
    struct pw_buffer* b;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    RequeueBufferGuard scope_guard { b, data->stream };

    spa_buffer* buf = b->buffer;

    if (!buf || !buf->datas[0].data) {
        pw_log_warn("No data in buffer\n");
        return;
    }

    auto const num_bytes = buf->datas[0].chunk->size;
    auto const num_samples = num_bytes / sizeof(SampleType);

    sc::MediaChunk chunk;
    chunk.timestamp_ms = sc::global_elapsed.value();
    chunk.sample_count = num_samples;
    std::span channel_data { buf->datas, buf->n_datas };
    for (auto const& d : channel_data) {
        sc::DynamicBuffer chunk_buffer;
        auto target_bytes = chunk_buffer.prepare(num_bytes);

        std::span source_bytes { reinterpret_cast<std::uint8_t const*>(d.data),
                                 num_bytes };

        std::copy(begin(source_bytes), end(source_bytes), begin(target_bytes));
        chunk_buffer.commit(num_bytes);

        chunk.channel_buffers().push_back(std::move(chunk_buffer));
    }

    add_chunk(*data->service, std::move(chunk));
}

/* Be notified when the stream param changes. We're only looking at the
 * format changes.
 */
static void
on_stream_param_changed(void* _data, uint32_t id, const struct spa_pod* param)
{
    sc::AudioLoopData* data = reinterpret_cast<sc::AudioLoopData*>(_data);

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(
            param, &data->format.media_type, &data->format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &data->format.info.raw);

    fprintf(stdout,
            "capturing rate: %d, channels: %d\n",
            data->format.info.raw.rate,
            data->format.info.raw.channels);
}
constexpr pw_stream_events stream_events = { .version =
                                                 PW_VERSION_STREAM_EVENTS,
                                             .destroy = nullptr,
                                             .state_changed = nullptr,
                                             .control_info = nullptr,
                                             .io_changed = nullptr,
                                             .param_changed =
                                                 on_stream_param_changed,
                                             .add_buffer = nullptr,
                                             .remove_buffer = nullptr,
                                             .process = on_process,
                                             .drained = nullptr,
                                             .command = nullptr,
                                             .trigger_done = nullptr };
auto start_pipewire(sc::AudioLoopData& data)
{
    std::array<spa_pod const*, 1> params {};
    uint8_t buffer[1024];
    struct pw_properties* props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    /* make a main loop. If you already have another main loop, you can add
     * the fd of this pipewire mainloop to it. */
    data.loop = pw_thread_loop_new("shadow-capture-audio", nullptr);

    pw_thread_loop_lock(data.loop);

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

    data.stream = pw_stream_new_simple(pw_thread_loop_get_loop(data.loop),
                                       "audio-capture",
                                       props,
                                       &stream_events,
                                       &data);

    /* Make one parameter with the supported formats. The
     * SPA_PARAM_EnumFormat id means that this is a format enumeration (of 1
     * value). We leave the channels and rate empty to accept the native
     * graph rate and channels. */
    spa_audio_info_raw raw_init = {};
    raw_init.format = SPA_AUDIO_FORMAT_F32P;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw_init);

    /* Now connect this stream.
     */
    if (pw_stream_connect(
            data.stream,
            PW_DIRECTION_INPUT,
            PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                         PW_STREAM_FLAG_MAP_BUFFERS),
            params.data(),
            params.size()) < 0) {
        fprintf(stderr, "Couldn't connect stream\n");
        return;
    }

    pw_thread_loop_unlock(data.loop);
    pw_thread_loop_start(data.loop);
}

auto stop_pipewire(sc::AudioLoopData& data) noexcept -> void
{
    pw_thread_loop_stop(data.loop);
    pw_stream_destroy(data.stream);
    pw_thread_loop_destroy(data.loop);
}

} // namespace

namespace sc
{

auto AudioService::on_init(ReadinessRegister reg) -> void
{
    if (pipe2(pipe_.data(), O_NONBLOCK) < 0)
        throw std::system_error { errno, std::system_category() };

    reg(pipe_[kPipeRead], &dispatch_chunks);

    loop_data_ = {};
    loop_data_.service = this;
    start_pipewire(loop_data_);
}

auto AudioService::on_uninit() -> void
{
    stop_pipewire(loop_data_);
    close(pipe_[kPipeRead]);
    close(pipe_[kPipeWrite]);
    pipe_ = { -1, -1 };

    if (stream_end_listener_)
        (*stream_end_listener_)();
}

auto dispatch_chunks(sc::Service& svc) -> void
{
    auto& self = static_cast<AudioService&>(svc);

    decltype(self.available_chunks_) tmp_chunks;

    {
        std::lock_guard data_lock { self.data_mutex_ };
        tmp_chunks.splice(tmp_chunks.end(), self.available_chunks_);
        discard_pipe_data(self.pipe_[kPipeRead], tmp_chunks.size());
    }

    if (auto& listener = self.chunk_listener_; listener) {
        for (auto&& chunk : tmp_chunks)
            (*listener)(std::move(chunk));
    }
}

auto add_chunk(AudioService& svc, MediaChunk&& chunk) -> void
{
    static std::array<std::uint8_t, 1> const signal_byte = { 0x42 };

    {
        std::lock_guard data_lock { svc.data_mutex_ };
        svc.available_chunks_.push_back(std::move(chunk));
        write(svc.pipe_[kPipeWrite], signal_byte.data(), 1);
    }
}

} // namespace sc
