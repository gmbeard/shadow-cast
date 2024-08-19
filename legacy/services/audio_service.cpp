#include "services/audio_service.hpp"
#include "av/media_chunk.hpp"
#include "av/sample_format.hpp"
#include "utils/contracts.hpp"
#include "utils/elapsed.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <span>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
#include "metrics/metrics.hpp"
#endif

using namespace std::literals::string_literals;

namespace
{

auto prepare_buffer_channels(sc::MediaChunk& buffer,
                             sc::SampleFormat sample_format,
                             std::size_t num_channels = 2) -> void
{
    buffer.channel_buffers().clear();
    buffer.channel_buffers().push_back(sc::DynamicBuffer {});

    if (!sc::is_interleaved_format(sample_format)) {
        while (num_channels != buffer.channel_buffers().size()) {
            buffer.channel_buffers().push_back(sc::DynamicBuffer {});
        }
    }

    SC_EXPECT(buffer.channel_buffers().size() ==
                      sc::is_interleaved_format(sample_format)
                  ? 1
                  : num_channels);
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
            "Audio capturing rate: %d, channels: %d\n",
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
                                             .process = sc::on_process,
                                             .drained = nullptr,
                                             .command = nullptr,
                                             .trigger_done = nullptr };
auto start_pipewire(sc::AudioLoopData& data)
{
    std::array<spa_pod const*, 2> params {};
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
    fprintf(stderr,
            "Audio sample format: %s\n",
            sample_format_name(data.required_sample_format));
    raw_init.format = convert_to_pipewire_format(data.required_sample_format);
    raw_init.rate = data.required_sample_rate;
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

auto on_process(void* userdata) -> void
{
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

    auto const sample_size = sample_format_size(data->required_sample_format);
    auto const num_bytes = buf->datas[0].chunk->size;
    auto const num_samples = is_interleaved_format(data->required_sample_format)
                                 ? (num_bytes / 2) / sample_size
                                 : num_bytes / sample_size;

    std::span channel_data { buf->datas, buf->n_datas };

    auto lock = std::lock_guard { data->service->data_mutex_ };

    auto& input_buffer = data->service->input_buffer_;
    SC_EXPECT(channel_data.size() == input_buffer.channel_buffers().size());

    auto out_buffer_it = input_buffer.channel_buffers().begin();

    for (auto const& d : channel_data) {
        sc::DynamicBuffer& chunk_buffer = *out_buffer_it++;
        auto target_bytes = chunk_buffer.prepare(num_bytes);

        std::span source_bytes { reinterpret_cast<std::uint8_t const*>(d.data),
                                 num_bytes };

        std::copy(begin(source_bytes), end(source_bytes), begin(target_bytes));
        chunk_buffer.commit(num_bytes);
    }

    input_buffer.sample_count += num_samples;

    if (input_buffer.sample_count >= data->service->frame_size_) {
        data->service->notify(input_buffer.sample_count /
                              data->service->frame_size_);
    }
}

AudioService::AudioService(SampleFormat sample_format,
                           std::size_t sample_rate,
                           std::size_t frame_size)
    : sample_format_ { sample_format }
    , sample_rate_ { sample_rate }
    , frame_size_ { frame_size }
{
}

AudioService::~AudioService() {}

auto AudioService::notify(std::size_t frames) noexcept -> void
{
    std::uint64_t const val = frames;
    ::write(event_fd_, &val, sizeof(val));
}

auto AudioService::on_init(ReadinessRegister reg) -> void
{
    event_fd_ = eventfd(0, EFD_NONBLOCK);
    SC_EXPECT(event_fd_ >= 0);
    reg(event_fd_, &dispatch_chunks);

    prepare_buffer_channels(input_buffer_, sample_format_);

    loop_data_ = {};
    loop_data_.service = this;
    loop_data_.required_sample_format = sample_format_;
    loop_data_.required_sample_rate = sample_rate_;
    start_pipewire(loop_data_);
}

auto AudioService::on_uninit() noexcept -> void
{
    ::close(event_fd_);
    event_fd_ = -1;
    stop_pipewire(loop_data_);

    if (stream_end_listener_)
        (*stream_end_listener_)();
}

auto dispatch_chunks(sc::Service& svc) -> void
{
    auto& self = static_cast<AudioService&>(svc);
    std::uint64_t val;
    ::read(self.event_fd_, &val, sizeof(val));

    std::size_t num_frames = val;
    SC_EXPECT(num_frames);

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    auto const frame_start = global_elapsed.nanosecond_value();
#endif

    if (auto& listener = self.chunk_listener_; listener) {
        while (num_frames--) {
            auto lock = std::lock_guard { self.data_mutex_ };
            SC_EXPECT(self.input_buffer_.sample_count >= self.frame_size_);

            (*listener)(self.input_buffer_);
            auto const sample_size =
                sc::sample_format_size(self.sample_format_);
            auto const interleaved =
                sc::is_interleaved_format(self.sample_format_);

            for (auto it = self.input_buffer_.channel_buffers().begin();
                 it != self.input_buffer_.channel_buffers().end();
                 ++it) {

                if (interleaved) {
                    it->consume(sample_size * self.frame_size_ * 2);
                    break;
                }

                it->consume(sample_size * self.frame_size_);
            }

            self.input_buffer_.sample_count -= self.frame_size_;
        }
    }

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    metrics::add_frame_time(metrics::audio_metrics,
                            global_elapsed.nanosecond_value() - frame_start);
#endif
}

} // namespace sc
