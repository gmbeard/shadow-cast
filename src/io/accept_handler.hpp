#ifndef SHADOW_CAST_IO_ACCEPT_HANDLE_HPP_INCLUDED
#define SHADOW_CAST_IO_ACCEPT_HANDLE_HPP_INCLUDED

#include "io/message_handler.hpp"
#include <cstddef>
#include <signal.h>
#include <system_error>

namespace sc
{
struct AcceptHandler
{
    explicit AcceptHandler(std::size_t /*timeout_ms*/ = timeout::kInfinite,
                           sigset_t* /*sigmask*/ = nullptr) noexcept;

    auto operator()(int /*fd*/) noexcept -> Result<int, std::error_code>;

private:
    std::size_t timeout_ms_;
    sigset_t* sigmask_;
};
} // namespace sc
#endif // SHADOW_CAST_IO_ACCEPT_HANDLE_HPP_INCLUDED
