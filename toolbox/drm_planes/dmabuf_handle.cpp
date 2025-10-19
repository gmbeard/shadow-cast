#include "dmabuf_handle.hpp"
#include <unistd.h>
#include <utility>

dmabuf_handle::dmabuf_handle(dmabuf_handle&& other) noexcept
    : fd_ { std::exchange(other.fd_, -1) }
{
}

dmabuf_handle::~dmabuf_handle()
{
    if (fd_ != -1)
        ::close(fd_);
}

auto dmabuf_handle::operator=(dmabuf_handle rhs) noexcept -> dmabuf_handle&
{
    swap(*this, rhs);
    return *this;
}

auto swap(dmabuf_handle& lhs, dmabuf_handle& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.fd_, rhs.fd_);
}

dmabuf_handle::operator int() const noexcept
{
    return fd_;
}

dmabuf_handle::dmabuf_handle(int fd) noexcept
    : fd_ { fd }
{
}

auto dmabuf_handle::reset() noexcept -> void
{
    dmabuf_handle tmp { std::move(*this) };
}

auto dmabuf_handle::release() noexcept -> int
{
    return std::exchange(fd_, -1);
}
