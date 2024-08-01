#ifndef SHADOW_CAST_SERVICES_MEDIA_WRITER_SERVICE_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_MEDIA_WRITER_SERVICE_HPP_INCLUDED

#include "config.hpp"

#include "av/frame.hpp"
#include "av/packet.hpp"
#include "services/readiness.hpp"
#include "services/service.hpp"
#include "utils/borrowed_ptr.hpp"
#include "utils/intrusive_list.hpp"
#include "utils/pool.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <mutex>

namespace sc
{

struct StreamPoll : ListItemBase
{
    StreamPoll();

    AVCodecContext* codec_ctx;
    AVStream* stream;
    FramePtr frame;

    auto reset() noexcept -> void;
};

struct EncoderService final : Service
{
private:
    struct EncoderPoolLifetime
    {
        auto construct(StreamPoll* item) -> void
        {
            new (static_cast<void*>(item)) StreamPoll {};
            item->frame = FramePtr { av_frame_alloc() };
        }

        auto destruct(StreamPoll* item) -> void { item->~StreamPoll(); }
    };

public:
    using PoolType = SynchronizedPool<StreamPoll, EncoderPoolLifetime>;
    EncoderService(BorrowedPtr<AVFormatContext>) noexcept;

    EncoderService(EncoderService const&) = delete;
    auto operator=(EncoderService const&) -> EncoderService& = delete;

    auto write_frame(PoolType::ItemPtr) -> void;

    auto pool() noexcept -> PoolType&;

protected:
    auto on_init(ReadinessRegister) -> void override;
    auto on_uninit() noexcept -> void override;

private:
    static auto dispatch(Service&) -> void;

    BorrowedPtr<AVFormatContext> format_context_;
    int notify_fd_ { -1 };
    std::mutex data_mutex_;
    PacketPtr packet_;
    PoolType pool_;
    IntrusiveList<StreamPoll> pending_;
};
} // namespace sc

#endif // SHADOW_CAST_SERVICES_MEDIA_WRITER_SERVICE_HPP_INCLUDED
