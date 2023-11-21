#include "io/accept_handler.hpp"
#include <sys/socket.h>
#include <sys/un.h>

namespace sc
{
AcceptHandler::AcceptHandler(std::size_t tout, sigset_t* mask) noexcept
    : timeout_ms_ { tout }
    , sigmask_ { mask }
{
}

auto AcceptHandler::operator()(int fd) noexcept -> Result<int, std::error_code>
{
    pollfd pfd[1] = {};
    pfd[0].fd = fd;
    pfd[0].events = POLLIN;

    timespec ts_storage = {};
    timespec* ts = nullptr;
    if (timeout_ms_ != timeout::kInfinite) {
        ts_storage = { .tv_sec = static_cast<time_t>(timeout_ms_ / 1'000),
                       .tv_nsec = static_cast<time_t>((timeout_ms_ % 1'000) *
                                                      1'000'000) };
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
    auto const poll_result = ::ppoll(pfd, 1, ts, sigmask_);

    if (poll_result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    if (poll_result == 0)
        return result_error(
            std::error_code { ETIMEDOUT, std::system_category() });

    sockaddr_un remote_addr;
    socklen_t len = sizeof(sockaddr_un);
    auto const accept_result =
        ::accept(fd, reinterpret_cast<sockaddr*>(&remote_addr), &len);
    if (accept_result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(accept_result);
}

} // namespace sc
