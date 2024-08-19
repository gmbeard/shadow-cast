#ifndef SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED

#include "config.hpp"

#include "av/media_chunk.hpp"
#include "av/sample_format.hpp"
#include "services/readiness.hpp"
#include "services/service.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"
#include "utils/receiver.hpp"
#include <array>
#include <list>
#include <mutex>
#include <optional>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace sc
{

struct AudioService;

struct AudioLoopData
{
    AudioService* service;
    pw_thread_loop* loop;
    pw_stream* stream;
    spa_audio_info format;
    SampleFormat required_sample_format;
    std::size_t required_sample_rate;
};

auto on_process(void* userdata) -> void;

struct AudioService final : Service
{
    friend auto dispatch_chunks(Service&) -> void;
    friend auto add_chunk(AudioService&, SynchronizedPool<MediaChunk>::ItemPtr)
        -> void;

    using ChunkReceiverType = Receiver<void(MediaChunk const&)>;
    using StreamEndReceiverType = Receiver<void()>;

    AudioService(SampleFormat,
                 std::size_t /*sample_rate*/,
                 std::size_t /*frame_size*/);

    ~AudioService();
    /* AudioService must have a stable `this` pointer while
     * the h/w loop is running, so disable copy / move...
     */
    AudioService(AudioService const&) = delete;
    auto operator=(AudioService const&) -> AudioService& = delete;

    friend auto on_process(void* userdata) -> void;

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() noexcept -> void override;

public:
    template <typename F>
    auto set_chunk_listener(F&& listener) -> void
    {
        chunk_listener_ = ChunkReceiverType { std::forward<F>(listener) };
    }

    template <typename F>
    auto set_stream_end_listener(F&& listener) -> void
    {
        stream_end_listener_ =
            StreamEndReceiverType { std::forward<F>(listener) };
    }

private:
    auto notify(std::size_t frames) noexcept -> void;

    std::optional<ChunkReceiverType> chunk_listener_;
    std::optional<StreamEndReceiverType> stream_end_listener_;
    std::mutex data_mutex_;
    AudioLoopData loop_data_ {};
    SampleFormat sample_format_;
    std::size_t sample_rate_;
    std::size_t frame_size_;
    MediaChunk input_buffer_;
    int event_fd_ { -1 };
};

auto dispatch_chunks(sc::Service&) -> void;

} // namespace sc

#endif // SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED
