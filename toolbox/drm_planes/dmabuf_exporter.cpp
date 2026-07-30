#include "dmabuf_exporter.hpp"
#include "dmabuf_handle.hpp"
#include "drm_device.hpp"
#include "drm_plane_framebuffer.hpp"
#include "exios/buffer_view.hpp"
#include "frame_time_detector.hpp"
#include "frame_timer.hpp"
#include "framebuffer_descriptor.hpp"
#include "logging.hpp"
#include "utils/contracts.hpp"
#include <algorithm>
#include <asm-generic/socket.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <linux/dma-buf.h>
#include <memory>
#include <optional>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <vector>

#define DEBUG_LOGGING 1

size_t constexpr kMaxFDs = 8;

namespace
{
template <typename Allocator>
struct message_base
{
    using allocator =
        std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

    template <typename UAllocator>
    explicit message_base(UAllocator const& alloc) noexcept
        : control_buffer_internal_ { alloc }
        , iovec_buffer_internal_ {
            std::allocator_traits<allocator>::template rebind_alloc<iovec>(
                alloc)
        }
    {
    }

    message_base() noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {})
    {
    }

    template <typename UAllocator>
    message_base(UAllocator const& alloc,
                 exios::ConstBufferView data_buffer,
                 exios::ConstBufferView control_buffer) noexcept
        : control_buffer_internal_ { alloc }
        , iovec_buffer_internal_ { std::allocator_traits<
              allocator>::template rebind_alloc<iovec>(alloc) }
        , data_buffer_ { data_buffer }
        , control_buffer_ { control_buffer }
    {
    }

    message_base(exios::ConstBufferView data_buffer) noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {}, data_buffer, exios::ConstBufferView {})
    {
    }

    message_base(exios::ConstBufferView data_buffer,
                 exios::ConstBufferView control_buffer) noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {}, data_buffer, control_buffer)
    {
    }

    auto set_control_buffer(exios::ConstBufferView buffer) noexcept
        -> message_base&
    {
        control_buffer_ = buffer;
        return *this;
    }

    auto set_data_buffer(exios::ConstBufferView buffer) noexcept
        -> message_base&
    {
        data_buffer_ = buffer;
        return *this;
    }

    [[nodiscard]] auto control_buffer() const noexcept -> exios::ConstBufferView
    {
        return exios::const_buffer_view(control_buffer_internal_);
    }

    [[nodiscard]] auto data_buffer() const noexcept -> exios::ConstBufferView
    {
        return data_buffer_;
    }

    [[nodiscard]] friend auto message_view(message_base& msg) -> msghdr
    {
        msg.iovec_buffer_internal_.resize(1);
        msg.iovec_buffer_internal_[0] = { msg.data_buffer().data,
                                          msg.data_buffer().size };

        auto result = msghdr {};
        result.msg_iov = msg.iovec_buffer_internal_.data();
        result.msg_iovlen = msg.iovec_buffer_internal_.size();

        if (msg.control_buffer_.size) {
            msg.control_buffer_internal_.resize(
                CMSG_SPACE(msg.control_buffer().size));

            result.msg_control = msg.control_buffer_internal_.data();
            result.msg_controllen = msg.control_buffer_internal_.size();

            auto* hdr = CMSG_FIRSTHDR(&result);
            hdr->cmsg_level = SOL_SOCKET;
            hdr->cmsg_type = SCM_RIGHTS;
            hdr->cmsg_len = CMSG_LEN(msg.control_buffer_internal_.size());

            std::copy_n(reinterpret_cast<std::uint8_t const*>(
                            msg.control_buffer().data),
                        msg.control_buffer().size,
                        reinterpret_cast<std::uint8_t*>(CMSG_DATA(hdr)));
        }

        return result;
    }

private:
    std::vector<std::uint8_t, allocator> control_buffer_internal_;
    std::vector<iovec, allocator> iovec_buffer_internal_;
    exios::ConstBufferView control_buffer_;
    exios::ConstBufferView data_buffer_;
};

using message = message_base<std::allocator<void>>;

template <typename Allocator>
auto message_view(message_base<Allocator>& msg,
                  exios::ConstBufferView data_buffer)
{
    msg.set_data_buffer(data_buffer);
    msg.set_control_buffer(exios::ConstBufferView {});
    return message_view(msg);
}

template <typename Allocator>
auto message_view(message_base<Allocator>& msg,
                  exios::ConstBufferView data_buffer,
                  exios::ConstBufferView control_buffer)
{
    msg.set_data_buffer(data_buffer);
    msg.set_control_buffer(control_buffer);
    return message_view(msg);
}

} // namespace

dmabuf_exporter::dmabuf_exporter(
    exios::UnixSocket& socket,
    std::string_view socket_name,
    sc::StickyCancelTimer& delay_timer,
    drm_device const& device,
    sc::framebuffer_descriptor_sequence& shared_mem,
    frame_time_estimation const& estimation,
    duration_type frame_time) noexcept
    : socket_ { socket }
    , socket_name_ { socket_name }
    , delay_timer_ { delay_timer }
    , device_ { device }
    , shared_mem_ { shared_mem }
    , estimation_ { estimation }
    , frame_time_ns_ {
        std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time).count()
    }
{
}

auto dmabuf_exporter::start() && -> void
{
    export_count_ = 0;
    msghdr_buffer_.resize(CMSG_SPACE(sizeof(int) * kMaxFDs));
    iovec_buffer_.resize(1);
    auto& s = socket_;
    auto cb = [self = std::move(*this)](exios::ConnectResult result) mutable {
        if (!result) {
            if (result.error() == std::errc::operation_canceled)
                return;

            sc::log(sc::LogLevel::error, "(dmabuf_exporter) Connect failed");
            throw std::system_error(result.error());
        }

        std::move(self).read_request();
    };
    s.connect(socket_name_, std::move(cb));
}

auto dmabuf_exporter::read_request() && -> void
{
    auto data = buffer_.prepare(sizeof(request_type));

    auto& s = socket_;
    auto cb = [self = std::move(*this)](exios::IoResult result) mutable {
        if (!result) {
            if (result.error() == std::errc::operation_canceled)
                return;

            sc::log(sc::LogLevel::error, "(dmabuf_exporter) Read failed");
            throw std::system_error(result.error());
        }

        if (result.value() == 0) {
            sc::log(sc::LogLevel::error,
                    "(dmabuf_exporter) Client unexpectedly dropped connection");
            throw std::system_error(
                std::make_error_code(std::errc::connection_aborted));
        }

        self.buffer_.commit(result.value());
        if (self.buffer_.size() < sizeof(request_type)) {
            sc::log(sc::LogLevel::warn,
                    "(dmabuf_exporter) Reading again: %llu bytes remaining",
                    (sizeof(request_type) - self.buffer_.size()));
            std::move(self).read_request();
        }
        else {
            auto const received_data = self.buffer_.data();
            request_type msg {};
            std::memcpy(&msg, received_data.data(), sizeof(request_type));
            self.buffer_.consume(self.buffer_.size());
            std::move(self).detect_phase_correction(msg);
        }
    };

    s.read(exios::buffer_view(data), std::move(cb));
}

auto dmabuf_exporter::detect_phase_correction(request_type msg) && -> void
{
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    auto const& avg_frame_time = estimation_.avg_frame_time;

    if (!avg_frame_time.has_value()) {
        std::move(*this).send_reply(
            std::make_pair(std::move(msg), std::uint32_t(0)));
        return;
    }

    std::uint32_t phase_offset { 0 };
    auto const now = sc::frame_timer::now();
    auto const& last_frame_ts = estimation_.last_frame_ts;

    std::int64_t const detected_framerate =
        duration_cast<nanoseconds>(seconds(1)).count() /
        *avg_frame_time.value();
    std::int64_t const specified_framerate =
        duration_cast<nanoseconds>(seconds(1)).count() / frame_time_ns_;

    auto const estimated_next_frame_ts =
        last_frame_ts + nanoseconds(frame_time_ns_);

    if (now < estimated_next_frame_ts &&
        std::abs(specified_framerate - detected_framerate) <= 1) {
        phase_offset = static_cast<std::uint32_t>(
            duration_cast<nanoseconds>(now - last_frame_ts).count() %
            frame_time_ns_);
    }

#if DEBUG_LOGGING
    if (export_count_ % 120 == 0) {
        sc::log(sc::LogLevel::debug,
                "(dmabuf_exporter) Detected FPS: %lli, Specified FPS: %lli, "
                "offset: %u",
                detected_framerate,
                specified_framerate,
                phase_offset);
    }
#endif

    std::move(*this).send_reply(std::make_pair(std::move(msg), phase_offset));
}

auto dmabuf_exporter::send_reply(
    std::pair<request_type, std::uint32_t> request) && -> void
{
    auto descriptor = read_next_sequence(shared_mem_);
    SC_EXPECT(descriptor.fb_id > 0);
    drm_plane_framebuffer fb(static_cast<int>(device_), descriptor.fb_id);
    reply_type reply = { .fb_id = descriptor.fb_id,
                         .fd_and_sync_pair_count = 0,
                         .fd_and_sync_stride = 2,
                         /* Make these point to invalid descriptors. The
                          * receiver should fill these in at their end...
                          */
                         .fb_fd = -1,
                         .sync_fd = -1,
                         .phase_offset_nanoseconds = std::get<1>(request) };

    /* NOTE:
     * Export will fail unless one of the following conditions is met...
     * - We're running as root
     * - Binary has SYS_CAP_ADMIN
     */
    auto [got_handle, dmabuf_fd] = fb.get_dmabuf_handle();

    dmabuf_handle sync_fd;

    if (got_handle) {
        reply.fd_and_sync_pair_count = 1;
        struct dma_buf_export_sync_file arg = {
            .flags =
                DMA_BUF_SYNC_READ, // we want to READ; get fence for writers
            .fd = -1
        };

        if (ioctl(static_cast<int>(dmabuf_fd),
                  DMA_BUF_IOCTL_EXPORT_SYNC_FILE,
                  &arg) >= 0) {
            sync_fd = dmabuf_handle(arg.fd);
        }
        else {
            sc::log(sc::LogLevel::error,
                    "(dmabuf_exporter) EXPORT_SYNC failed: %s",
                    strerror(errno));
        }
    }

    std::memset(msghdr_buffer_.data(), 0, msghdr_buffer_.size());

    msghdr msg {};

    /* NOTE:
     * For the same reason that we can't use local stack pointers for `iov`s, we
     * need to avoid the local stack for the msg_control buffer, too...
     */
    msg.msg_control = msghdr_buffer_.data();
    msg.msg_controllen = CMSG_SPACE(sizeof(int) * reply.fd_and_sync_pair_count *
                                    reply.fd_and_sync_stride);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * reply.fd_and_sync_pair_count *
                              reply.fd_and_sync_stride);

    if (reply.fd_and_sync_pair_count > 0) {
        std::span<int> fds(reinterpret_cast<int*>(CMSG_DATA(cmsg)),
                           reply.fd_and_sync_pair_count *
                               reply.fd_and_sync_stride);
        fds[0] = static_cast<int>(dmabuf_fd);
        fds[1] = static_cast<int>(sync_fd);
    }

    auto buffer = buffer_.prepare(sizeof(reply_type));
    std::memcpy(buffer.data(), &reply, sizeof(reply_type));
    buffer_.commit(buffer.size());

    /* NOTE:
     * We can't use a ptr to a local `iov` because the I/O will be queued until
     * later and won't be on the same stack frame. We use a vector member
     * variable to ensure this lifetime. This is safe to use because, even
     * though we do `move(*this)`, the vector's underlying memory is stable. We
     * just have to be careful not to modify the vector while the I/O is in
     * flight. Cancellation should also be safe because the underlying I/O won't
     * ever read from this buffer if we get cancelled.
     */
    iovec_buffer_[0] = { buffer.data(), buffer.size() };
    msg.msg_iov = iovec_buffer_.data();
    msg.msg_iovlen = 1;

    auto& s = socket_;

    /* TODO:
     * WRT exios, a better API for `send_message` would be to accept the
     * message's buffer as a separate argument, and construct the `iov` members
     * at the point that the underlying `sendmsg` syscall is invoked...
     */
    auto cb = [dma_fd = std::move(dmabuf_fd),
               sync_fd = std::move(sync_fd),
               self = std::move(*this)](exios::IoResult result) mutable {
        /* Write complete. We're done with the dmabuf and fence FDs...
         */
        dma_fd.reset();
        sync_fd.reset();

        if (!result) {
            if (result.error() == std::errc::operation_canceled)
                return;

            sc::log(sc::LogLevel::error, "(dmabuf_exporter) Write failed");
            throw std::system_error(result.error());
        }

        self.buffer_.consume(result.value());
        SC_EXPECT(self.buffer_.size() == 0);

        self.export_count_ += 1;

        std::move(self).read_request();
    };

    s.send_message(msg, std::move(cb));
}
