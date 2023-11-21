#ifndef SHADOW_CAST_IO_MESSAGE_RECEIVER_HPP_INCLUDED
#define SHADOW_CAST_IO_MESSAGE_RECEIVER_HPP_INCLUDED

#include "io/message_handler.hpp"

namespace sc
{

template <typename T>
struct ReceiveHandler
{
    auto operator()(int fd, msghdr& msg, T&) noexcept -> ssize_t
    {
        return ::recvmsg(fd, &msg, MSG_WAITALL);
    }
};

template <typename T>
using MessageReceiver =
    MessageHandler<T, ReceiveHandler<T>, decltype(io::read)>;

} // namespace sc

#endif // SHADOW_CAST_IO_MESSAGE_RECEIVER_HPP_INCLUDED
