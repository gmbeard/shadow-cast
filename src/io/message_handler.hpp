#ifndef SHADOW_CAST_IO_MESSAGE_HANDLER_HPP_INCLUDED
#define SHADOW_CAST_IO_MESSAGE_HANDLER_HPP_INCLUDED

#include "utils/contracts.hpp"
#include "utils/result.hpp"
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <system_error>

namespace sc
{

namespace timeout
{
std::size_t constexpr kInfinite = static_cast<std::size_t>(-1);
}

namespace io
{
constexpr struct
{
} read;
constexpr struct
{
} write;
} // namespace io

template <typename T, typename Handler, typename Direction>
requires(std::is_same_v<Direction, decltype(io::read)> ||
         std::is_same_v<Direction, decltype(io::write)>)
struct MessageHandler
{

    explicit MessageHandler(T& payload,
                            std::size_t timeout_ms = timeout::kInfinite,
                            sigset_t* sigmask = nullptr) noexcept
        : payload_ { &payload }
        , timeout_ms_ { timeout_ms }
        , sigmask_ { sigmask }
    {
    }

    auto operator()(int fd) noexcept -> Result<std::size_t, std::error_code>
    {
        iovec iov = { payload_, sizeof(T) };
        msghdr msg {};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        pollfd pfd {};
        pfd.fd = fd;
        if constexpr (std::is_same_v<Direction, decltype(io::read)>) {
            pfd.events = POLLIN;
        }
        else {
            pfd.events = POLLOUT;
        }

        timespec ts_storage = {};
        timespec* ts = nullptr;
        if (timeout_ms_ != timeout::kInfinite) {
            ts_storage = { .tv_sec = static_cast<time_t>(timeout_ms_ / 1'000),
                           .tv_nsec = static_cast<time_t>(
                               (timeout_ms_ % 1'000) * 1'000'000) };
            ts = &ts_storage;
        }

        /* NOTE:
         * sigmask_ should be the *INVERSE* of the signals that we
         * want to be notified on. E.g. if we're currently blocking
         * SIGINT, and we wish for ppoll to tell us if a SIGINT
         * occurred, then `sigmask_` *MUST NOT* contain SIGINT.
         *
         * Essentially, the current mask will be *SET* to `sigmask_`
         * for the duration of `ppoll()`, and will be restored to
         * its previous value after the call. If a signal occurs
         * the `ppoll()` will return `EINTR`...
         */
        auto const poll_result = ::ppoll(&pfd, 1, ts, sigmask_);

        if (poll_result < 0)
            return result_error(
                std::error_code { errno, std::system_category() });

        if (poll_result == 0)
            return result_ok(0ull);

        auto const msg_result = handler_(fd, msg, *payload_);
        if (msg_result < 0)
            return result_error(
                std::error_code { errno, std::system_category() });

        return result_ok(static_cast<std::size_t>(msg_result));
    }

private:
    Handler handler_ {};
    T* payload_;
    std::size_t timeout_ms_ { timeout::kInfinite };
    sigset_t* sigmask_ { nullptr };
};

} // namespace sc

#endif // SHADOW_CAST_IO_MESSAGE_HANDLER_HPP_INCLUDED
