#include "services/encoder_service.hpp"
#include "config.hpp"
#include "error.hpp"
#include "utils/pool.hpp"
#include <cstring>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <stdexcept>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace std::literals::string_literals;

namespace
{
std::size_t constexpr kInitialPoolSize = 16;
}

namespace sc
{
EncoderService::EncoderService(
    BorrowedPtr<AVFormatContext> fmt_context) noexcept
    : format_context_ { fmt_context }
    , packet_ { av_packet_alloc() }
{
}

auto EncoderService::write_frame(PoolType::ItemPtr item) -> void
{
    {
        std::lock_guard lock { data_mutex_ };
        static_cast<void>(
            avcodec_send_frame(item->codec_ctx, item->frame.get()));
        pending_.push_back(item.release());
    }

    std::uint64_t const event_num { 1 };
    static_cast<void>(::write(notify_fd_, &event_num, sizeof(event_num)));
}

auto EncoderService::on_init(ReadinessRegister reg) -> void
{
    if (notify_fd_ = ::eventfd(0, EFD_NONBLOCK); notify_fd_ < 0)
        throw new std::runtime_error {
            "EncoderService initialization failed: "s + std::strerror(errno)
        };

    pool_.fill(kInitialPoolSize);
    reg(notify_fd_, &dispatch);
}

auto EncoderService::on_uninit() noexcept -> void
{
    static_cast<void>(::close(notify_fd_));
    try {
        dispatch(*this);
        pool_.clear();
    }
    catch (...) {
    }
}

auto EncoderService::dispatch(Service& svc) -> void
{
    auto& self = static_cast<EncoderService&>(svc);
    std::uint64_t event_num;
    static_cast<void>(::read(self.notify_fd_, &event_num, sizeof(event_num)));

    IntrusiveList<StreamPoll> tmp;
    {
        std::lock_guard lock { self.data_mutex_ };
        tmp.splice(tmp.begin(),
                   self.pending_,
                   self.pending_.begin(),
                   self.pending_.end());
    }

    ReturnToPoolGuard return_to_pool_guard { tmp, self.pool_ };

    for (auto const& stream_poll : tmp) {
        auto* stream = stream_poll.stream;
        auto* ctx = stream_poll.codec_ctx;
        int response = 0;
        while (response >= 0) {
            {
                std::lock_guard lock { self.data_mutex_ };
                response = avcodec_receive_packet(ctx, self.packet_.get());
            }
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                break;
            }

            if (response < 0) {
                throw std::runtime_error { "receive packet error" };
            }

            av_packet_rescale_ts(
                self.packet_.get(), ctx->time_base, stream->time_base);
            self.packet_->stream_index = stream->index;

            response = av_interleaved_write_frame(self.format_context_.get(),
                                                  self.packet_.get());
            if (response < 0) {
                throw std::runtime_error { "write packet error: " +
                                           av_error_to_string(response) };
            }
        }
    }
}

auto EncoderService::pool() noexcept -> PoolType& { return pool_; }

StreamPoll::StreamPoll()
    : codec_ctx { nullptr }
    , stream { nullptr }
    , frame { av_frame_alloc() }
{
}

auto StreamPoll::reset() noexcept -> void
{
    codec_ctx = nullptr;
    stream = nullptr;
    if (!frame) {
        frame = FramePtr { av_frame_alloc() };
    }
    else {
        av_frame_unref(frame.get());
    }
}

} // namespace sc
