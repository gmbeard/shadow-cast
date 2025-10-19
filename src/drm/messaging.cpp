#include "drm/messaging.hpp"
#include <span>
#include <sys/socket.h>

namespace sc
{

auto DRMResponseSendHandler::operator()(int fd,
                                        msghdr& msg,
                                        DRMResponse const& response) noexcept
    -> ssize_t
{
    /* The use of the control messages is significant
     * here; They are the _only_ valid way to transport
     * file descriptors via a unix socket. The kernel
     * must somehow know to marshal them correctly
     * between processes.
     * NOTE: _It's important to set the length of
     * the control message (`cmsg_len`) to the exact
     * size of the number of descriptors. Setting this
     * value higher cause issues when subsequently using
     * the descriptors_
     */

    SC_EXPECT(response.num_fds <= kMaxPlaneDescriptors);
    char cmsgbuf[CMSG_SPACE(sizeof(int) * kMaxPlaneDescriptors)];
    std::memset(cmsgbuf, 0, sizeof(int) * kMaxPlaneDescriptors);

    if (response.num_fds) {

        msg.msg_control = cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * response.num_fds * 2);

        cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * response.num_fds * 2);

        int* fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        std::span<PlaneDescriptor const> descriptors { response.descriptors,
                                                       response.num_fds };
        for (auto const& desc : descriptors)
            *fds++ = desc.fd;

        for (auto const& desc : descriptors)
            *fds++ = desc.sync_fd;
    }

    return ::sendmsg(fd, &msg, 0);
}

auto DRMResponseReceiveHandler::operator()(int fd,
                                           msghdr& msg,
                                           DRMResponse& response) noexcept
    -> ssize_t
{
    /* Here, we pluck the file descriptors from the control
     * message and update the response message payload
     */

    char cmsgbuf[CMSG_SPACE(sizeof(int) * kMaxPlaneDescriptors)] {};
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    int res = ::recvmsg(fd, &msg, MSG_WAITALL);
    if (res <= 0)
        return res;

    if (response.num_fds > 0) {
        cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        std::span<int> fds { reinterpret_cast<int*>(CMSG_DATA(cmsg)),
                             response.num_fds * 2 };

        SC_EXPECT(response.num_fds <= fds.size());
        for (std::uint32_t i = 0; i < response.num_fds; ++i) {
            response.descriptors[i].fd = fds[i];
        }
        for (std::uint32_t i = 0; i < response.num_fds; ++i) {
            response.descriptors[i].sync_fd = fds[response.num_fds + i];
        }
    }

    return res;
}

auto dmabuf_reply_message_receive_handler::operator()(
    int fd, msghdr& msg, dmabuf_reply_message& response) noexcept -> ssize_t
{
    /* Here, we pluck the file descriptors from the control
     * message and update the response message payload
     */

    char cmsgbuf[CMSG_SPACE(sizeof(int) * kMaxPlaneDescriptors)] {};
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    int res = ::recvmsg(fd, &msg, MSG_WAITALL);
    if (res <= 0)
        return res;

    if (response.fd_and_sync_pair_count == 0) {
        return res;
    }

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    std::span<int> fds { reinterpret_cast<int*>(CMSG_DATA(cmsg)),
                         response.fd_and_sync_pair_count *
                             response.fd_and_sync_stride };

    SC_EXPECT(response.fd_and_sync_pair_count * response.fd_and_sync_stride <=
              fds.size());
    response.fb_fd = fds[0];
    response.sync_fd = fds[1];

    return res;
}

} // namespace sc
