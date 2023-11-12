#ifndef SHADOW_CAST_IO_MESSAGE_SENDER_HPP_INCLUDED
#define SHADOW_CAST_IO_MESSAGE_SENDER_HPP_INCLUDED

#include "io/message_handler.hpp"

namespace sc
{
template <typename T>
struct SendHandler
{
    auto operator()(int fd, msghdr const& msg, T const&) noexcept -> ssize_t
    {
        return ::sendmsg(fd, &msg, 0);
    }
};

template <typename T>
using MessageSender = MessageHandler<T, SendHandler<T>, decltype(io::write)>;

} // namespace sc

#endif // SHADOW_CAST_IO_MESSAGE_SENDER_HPP_INCLUDED
