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
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * response.num_fds);

        cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * response.num_fds);

        int* fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        std::span<PlaneDescriptor const> descriptors { response.descriptors,
                                                       response.num_fds };
        for (auto const& desc : descriptors)
            *fds++ = desc.fd;
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
                             response.num_fds };

        SC_EXPECT(response.num_fds <= fds.size());
        for (std::uint32_t i = 0; i < response.num_fds; ++i) {
            response.descriptors[i].fd = fds[i];
        }
    }

    return res;
}

} // namespace sc
