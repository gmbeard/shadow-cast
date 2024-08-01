#include "services/audio_service.hpp"
#include "utils/contracts.hpp"
#include "utils/elapsed.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <errno.h>
#include <fcntl.h>
#include <span>
#include <system_error>
#include <unistd.h>

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
#include "metrics/metrics.hpp"
#endif

using namespace std::literals::string_literals;

namespace
{

auto transfer_chunk_n(sc::MediaChunk& source,
                      std::size_t num_bytes,
                      sc::MediaChunk& dest) -> void
{
    auto channels_required =
        std::max(0,
                 static_cast<int>(source.channel_buffers().size() -
                                  dest.channel_buffers().size()));

    while (channels_required--)
        dest.channel_buffers().push_back(sc::DynamicBuffer {});

    auto it = source.channel_buffers().begin();
    auto out_it = dest.channel_buffers().begin();

    for (; it != source.channel_buffers().end(); ++it) {
        auto& src_buf = *it;
        auto& dst_buf = *out_it++;

        assert(src_buf.size() >= num_bytes);

        auto dst_data = dst_buf.prepare(num_bytes);
        std::copy_n(src_buf.data().begin(), num_bytes, dst_data.begin());
        dst_buf.commit(num_bytes);
        src_buf.consume(num_bytes);
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

    auto pool_chunk = data->service->chunk_pool().get();

    sc::MediaChunk& chunk = *pool_chunk;
    chunk.timestamp_ms = sc::global_elapsed.value();
    chunk.sample_count = num_samples;
    std::span channel_data { buf->datas, buf->n_datas };
    while (chunk.channel_buffers().size() < channel_data.size())
        chunk.channel_buffers().push_back(sc::DynamicBuffer {});

    auto out_buffer_it = chunk.channel_buffers().begin();

    for (auto const& d : channel_data) {
        sc::DynamicBuffer& chunk_buffer = *out_buffer_it++;
        auto target_bytes = chunk_buffer.prepare(num_bytes);

        std::span source_bytes { reinterpret_cast<std::uint8_t const*>(d.data),
                                 num_bytes };

        std::copy(begin(source_bytes), end(source_bytes), begin(target_bytes));
        chunk_buffer.commit(num_bytes);
    }

    add_chunk(*data->service, std::move(pool_chunk));
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

AudioService::AudioService(SampleFormat sample_format,
                           std::size_t sample_rate,
                           std::size_t frame_size)
    : sample_format_ { sample_format }
    , sample_rate_ { sample_rate }
    , frame_size_ { frame_size }
{
}

AudioService::~AudioService()
{
    ReturnToPoolGuard return_to_pool_guard { available_chunks_, chunk_pool_ };
}

auto AudioService::chunk_pool() noexcept -> SynchronizedPool<MediaChunk>&
{
    return chunk_pool_;
}

auto AudioService::on_init(ReadinessRegister reg) -> void
{
    reg(FrameTimeRatio(1), &dispatch_chunks);

    loop_data_ = {};
    loop_data_.service = this;
    loop_data_.required_sample_format = sample_format_;
    loop_data_.required_sample_rate = sample_rate_;
    start_pipewire(loop_data_);
}

auto AudioService::on_uninit() noexcept -> void
{
    stop_pipewire(loop_data_);

    if (stream_end_listener_)
        (*stream_end_listener_)();
}

auto dispatch_chunks(sc::Service& svc) -> void
{
    auto& self = static_cast<AudioService&>(svc);

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    auto const frame_start = global_elapsed.nanosecond_value();
#endif

    decltype(self.available_chunks_) tmp_chunks;
    ReturnToPoolGuard return_to_pool_guard { tmp_chunks, self.chunk_pool() };

    {
        std::lock_guard data_lock { self.data_mutex_ };
        auto it = std::find_if(self.available_chunks_.begin(),
                               self.available_chunks_.end(),
                               [&](auto const& chunk) {
                                   return self.frame_size_ &&
                                          chunk.sample_count < self.frame_size_;
                               });

        tmp_chunks.splice(tmp_chunks.begin(),
                          self.available_chunks_,
                          self.available_chunks_.begin(),
                          it);
    }

    if (auto& listener = self.chunk_listener_; listener) {
        for (auto const& chunk : tmp_chunks) {
            SC_EXPECT(!self.frame_size_ ||
                      chunk.sample_count == self.frame_size_);
            (*listener)(chunk);
        }
    }

#ifdef SHADOW_CAST_ENABLE_HISTOGRAMS
    metrics::add_frame_time(metrics::audio_metrics,
                            global_elapsed.nanosecond_value() - frame_start);
#endif
}

auto add_chunk(AudioService& svc,
               sc::SynchronizedPool<MediaChunk>::ItemPtr chunk) -> void
{
    auto const sample_size = sc::sample_format_size(svc.sample_format_);
    auto const interleaved = sc::is_interleaved_format(svc.sample_format_);

    using namespace std::literals::string_literals;

    {
        std::lock_guard data_lock { svc.data_mutex_ };

        if (!svc.frame_size_) {
            svc.available_chunks_.push_back(chunk.release());
            return;
        }

        if (!svc.available_chunks_.empty()) {
            auto it = std::next(svc.available_chunks_.end(), -1);

            if (svc.frame_size_ > it->sample_count) {
                auto const samples_to_copy = std::min(
                    chunk->sample_count, svc.frame_size_ - it->sample_count);

                if (samples_to_copy) {
                    transfer_chunk_n(*chunk,
                                     sample_size * samples_to_copy *
                                         (interleaved ? 2 : 1),
                                     *it);
                    it->sample_count += samples_to_copy;
                    chunk->sample_count -= samples_to_copy;
                }
            }
        }

        while (chunk->sample_count > svc.frame_size_) {
            auto new_chunk = svc.chunk_pool().get();

            transfer_chunk_n(*chunk,
                             sample_size * svc.frame_size_ *
                                 (interleaved ? 2 : 1),
                             *new_chunk);

            chunk->sample_count -= svc.frame_size_;
            new_chunk->sample_count += svc.frame_size_;

            svc.available_chunks_.push_back(new_chunk.release());
        }

        if (chunk->sample_count) {
            svc.available_chunks_.push_back(chunk.release());
        }
    }
}

} // namespace sc
