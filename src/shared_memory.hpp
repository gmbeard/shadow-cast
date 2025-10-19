#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_SHARED_MEMORY_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_SHARED_MEMORY_HPP_INCLUDED

#include <string>

namespace sc
{
struct shared_memory
{
    shared_memory(shared_memory&& other) noexcept;
    ~shared_memory();
    friend auto swap(shared_memory& lhs, shared_memory& rhs) noexcept -> void;
    auto operator=(shared_memory&& rhs) noexcept -> shared_memory&;

    auto size() const noexcept -> std::size_t;
    auto data() const noexcept -> void*;
    auto path() const noexcept -> std::string const&;

private:
    friend auto create_shared_memory(std::string path, std::size_t size)
        -> shared_memory;

    friend auto open_shared_memory(std::string path, std::size_t size)
        -> shared_memory;

    shared_memory(int fd,
                  std::string&& path,
                  std::size_t size,
                  void* ptr,
                  bool is_owner) noexcept;

    int fd_;
    std::string path_;
    std::size_t size_;
    void* ptr_ { nullptr };
    bool is_owner_;
};

auto create_shared_memory(std::string path, std::size_t size) -> shared_memory;

auto open_shared_memory(std::string path, std::size_t size) -> shared_memory;
} // namespace sc
#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_SHARED_MEMORY_HPP_INCLUDED
