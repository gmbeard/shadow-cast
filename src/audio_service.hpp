#ifndef SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED

#include "./media_chunk.hpp"
#include "./readiness.hpp"
#include "./receiver.hpp"
#include "./service.hpp"
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
};

struct AudioService final : Service
{
    friend auto dispatch_chunks(Service&) -> void;
    friend auto add_chunk(AudioService&, MediaChunk&&) -> void;

    using ChunkReceiverType = Receiver<void(MediaChunk&&)>;
    using StreamEndReceiverType = Receiver<void()>;

    AudioService() = default;

    /* AudioService must have a stable `this` pointer while
     * the h/w loop is running, so disable copy / move...
     */
    AudioService(AudioService const&) = delete;
    auto operator=(AudioService const&) -> AudioService& = delete;

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() -> void override;

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
    std::array<int, 2> pipe_ { -1, -1 };
    std::optional<ChunkReceiverType> chunk_listener_;
    std::optional<StreamEndReceiverType> stream_end_listener_;
    std::list<MediaChunk> available_chunks_;
    std::mutex data_mutex_;
    AudioLoopData loop_data_ {};
};

auto dispatch_chunks(sc::Service&) -> void;
auto add_chunk(AudioService&, MediaChunk&&) -> void;

} // namespace sc

#endif // SHADOW_CAST_AUDIO_SERVICE_HPP_INCLUDED
