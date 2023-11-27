#include "io/unix_socket.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <utility>

using namespace std::literals::string_literals;

namespace
{

template <typename F>
auto create_socket(std::string_view path, F&& on_create)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error { "connect(): "s + std::strerror(errno) };

    auto close_guard = sc::ScopeGuard { [&] { close(fd); } };

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (path.size() > std::size(addr.sun_path))
        throw std::runtime_error { "create_socket(): path too long" };

    auto&& result = on_create(fd, addr);
    close_guard.deactivate();
    return result;
}

} // namespace

namespace sc
{

UnixSocket::UnixSocket(UnixSocket&& other) noexcept
    : fd_ { std::exchange(other.fd_, -1) }
{
}

UnixSocket::UnixSocket(int fd) noexcept
    : fd_ { fd }
{
}

UnixSocket::~UnixSocket() { close(); }

auto UnixSocket::operator=(UnixSocket&& other) noexcept -> UnixSocket&
{
    using std::swap;
    auto tmp { std::move(other) };

    swap(*this, tmp);
    return *this;
}

auto UnixSocket::close() noexcept -> int { return ::close(fd_); }

auto UnixSocket::accept() -> UnixSocket
{
    sockaddr_un remote_addr;
    socklen_t len = sizeof(sockaddr_un);
    auto fd = ::accept(fd_, reinterpret_cast<sockaddr*>(&remote_addr), &len);
    if (fd < 0)
        throw std::runtime_error { "UnixSocket::accept() "s +
                                   std::strerror(errno) };

    return UnixSocket { fd };
}

auto UnixSocket::listen() -> void
{
    if (auto const listen_result = ::listen(fd_, 1); listen_result < 0)
        throw std::runtime_error { "UnixSocket::listen() "s +
                                   std::strerror(errno) };
}

auto UnixSocket::fd() const noexcept -> int { return fd_; }

auto swap(UnixSocket& lhs, UnixSocket& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.fd_, rhs.fd_);
}

namespace socket
{

auto bind(std::string_view path) -> UnixSocket
{
    return create_socket(path, [&](int fd, sockaddr_un addr) {
        if (auto const result = bind(fd,
                                     reinterpret_cast<sockaddr*>(&addr),
                                     sizeof(addr.sun_family) + path.size());
            result < 0)
            throw std::logic_error { "bind(): "s + std::strerror(errno) };

        return UnixSocket { fd };
    });
}

auto connect(std::string_view path) -> UnixSocket
{
    return create_socket(path, [&](int fd, sockaddr_un addr) {
        if (auto const result = connect(fd,
                                        reinterpret_cast<sockaddr*>(&addr),
                                        sizeof(addr.sun_family) + path.size());
            result < 0)
            throw std::logic_error { "connect(): "s + std::strerror(errno) };

        return UnixSocket { fd };
    });
}

} // namespace socket
} // namespace sc
