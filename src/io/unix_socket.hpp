#ifndef SHADOW_CAST_IO_UNIX_SOCKET_HPP_INCLUDED
#define SHADOW_CAST_IO_UNIX_SOCKET_HPP_INCLUDED

#include <span>
#include <string_view>

namespace sc
{
struct UnixSocket;

namespace socket
{
auto bind(std::string_view /*path*/) -> UnixSocket;
auto connect(std::string_view /*path*/) -> UnixSocket;
} // namespace socket

struct UnixSocket
{
    friend auto swap(UnixSocket&, UnixSocket&) noexcept -> void;
    friend auto socket::bind(std::string_view /*path*/) -> UnixSocket;
    friend auto socket::connect(std::string_view /*path*/) -> UnixSocket;

    UnixSocket() noexcept = default;
    UnixSocket(UnixSocket&&) noexcept;
    ~UnixSocket();

    auto operator=(UnixSocket&&) noexcept -> UnixSocket&;

    auto close() noexcept -> int;
    auto accept() -> UnixSocket;
    auto listen() -> void;
    auto fd() const noexcept -> int;

    template <typename F>
    auto use_with(F&& f)
    {
        return std::forward<F>(f)(fd_);
    }

    UnixSocket(int /*fd*/) noexcept;

    int fd_ { -1 };
};

auto swap(UnixSocket&, UnixSocket&) noexcept -> void;

} // namespace sc
#endif // SHADOW_CAST_IO_UNIX_SOCKET_HPP_INCLUDED
