#include "shared_memory.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace sc
{

shared_memory::shared_memory(int fd,
                             std::string&& path,
                             std::size_t size,
                             void* ptr,
                             bool is_owner) noexcept
    : fd_ { fd }
    , path_ { std::move(path) }
    , size_ { size }
    , ptr_ { ptr }
    , is_owner_ { is_owner }
{
}

shared_memory::~shared_memory()
{
    if (fd_ >= 0) {
        ::close(fd_);
        if (is_owner_) {
            ::shm_unlink(path_.c_str());
        }
    }
}

shared_memory::shared_memory(shared_memory&& other) noexcept
    : fd_ { std::exchange(other.fd_, -1) }
    , path_ { std::move(other.path_) }
    , size_ { std::exchange(other.size_, 0) }
    , ptr_ { std::exchange(other.ptr_, nullptr) }
    , is_owner_ { std::exchange(other.is_owner_, false) }
{
}

auto swap(shared_memory& lhs, shared_memory& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.fd_, rhs.fd_);
    swap(lhs.path_, rhs.path_);
    swap(lhs.size_, rhs.size_);
    swap(lhs.ptr_, rhs.ptr_);
    swap(lhs.is_owner_, rhs.is_owner_);
}

auto shared_memory::operator=(shared_memory&& rhs) noexcept -> shared_memory&
{
    shared_memory tmp { std::move(rhs) };
    swap(*this, tmp);
    return *this;
}

auto shared_memory::size() const noexcept -> std::size_t
{
    return size_;
}

auto shared_memory::data() const noexcept -> void*
{
    return ptr_;
}

auto shared_memory::path() const noexcept -> std::string const&
{
    return path_;
}

auto create_shared_memory(std::string path, std::size_t size) -> shared_memory
{
    int fd;
    while (true) {
        fd = shm_open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd >= 0)
            break;
        auto const ec = std::error_code(errno, std::system_category());
        if (ec != std::errc::file_exists) {
            throw std::system_error(ec);
        }

        /* TODO:
         * This is to guard against an owner process who terminated abnormally,
         * possibly without unlinking the shmem file. Is this wise? Would it
         * affect any processes that _legitimately_ have this shared memory
         * mapped?...
         */
        SC_EXPECT(::shm_unlink(path.c_str()) == 0);
    }

    auto guard = sc::ScopeGuard { [&] {
        ::close(fd);
        ::shm_unlink(path.c_str());
    } };

    if (ftruncate(fd, size) < 0) {
        throw std::system_error(errno, std::system_category());
    }

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!ptr) {
        throw std::system_error(errno, std::system_category());
    }

    guard.deactivate();

    return shared_memory(fd, std::move(path), size, ptr, true);
}

auto open_shared_memory(std::string path, std::size_t size) -> shared_memory
{
    int fd = shm_open(path.c_str(), O_RDWR, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::system_category());
    }

    auto guard = sc::ScopeGuard { [&] { ::close(fd); } };

    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!ptr) {
        throw std::system_error(errno, std::system_category());
    }

    guard.deactivate();

    return shared_memory(fd, std::move(path), size, ptr, false);
}

} // namespace sc
