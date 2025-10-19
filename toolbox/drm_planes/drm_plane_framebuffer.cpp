#include "drm_plane_framebuffer.hpp"
#include "drm_device.hpp"
#include "drm_plane.hpp"
#include "utils/contracts.hpp"
#include <fcntl.h>
#include <iterator>
#include <unistd.h>
#include <utility>
#include <xf86drm.h>
#include <xf86drmMode.h>

drm_plane_framebuffer::drm_plane_framebuffer(int drm_fd,
                                             std::uint32_t fb_id) noexcept
    : drm_fd_ { drm_fd }
    , fb_id_ { fb_id }
    , frame_buffer_ { nullptr }
{
    frame_buffer_ = drmModeGetFB2(drm_fd_, fb_id_);
}

drm_plane_framebuffer::drm_plane_framebuffer(int drm_fd,
                                             drm_plane const& plane) noexcept
    : drm_plane_framebuffer(drm_fd, plane->fb_id)
{
}

drm_plane_framebuffer::drm_plane_framebuffer(
    drm_plane_framebuffer&& other) noexcept
    : drm_fd_ { std::exchange(other.drm_fd_, drm_device::kNoDevice) }
    , fb_id_ { other.fb_id_ }
    , frame_buffer_ { std::exchange(other.frame_buffer_, nullptr) }
{
}

drm_plane_framebuffer::~drm_plane_framebuffer()
{
    if (frame_buffer_) {
        drmModeFreeFB2(frame_buffer_);
    }
}

auto drm_plane_framebuffer::operator=(drm_plane_framebuffer rhs) noexcept
    -> drm_plane_framebuffer&
{
    swap(*this, rhs);
    return *this;
}

auto swap(drm_plane_framebuffer& lhs, drm_plane_framebuffer& rhs) noexcept
    -> void
{
    using std::swap;
    swap(lhs.drm_fd_, rhs.drm_fd_);
    swap(lhs.fb_id_, rhs.fb_id_);
    swap(lhs.frame_buffer_, rhs.frame_buffer_);
}

drm_plane_framebuffer::operator bool() const noexcept
{
    return frame_buffer_ != nullptr;
}

auto drm_plane_framebuffer::operator->() const noexcept -> const_pointer
{
    SC_EXPECT(frame_buffer_);
    return frame_buffer_;
}

auto drm_plane_framebuffer::get_dmabuf_handle(std::size_t n) const noexcept
    -> std::pair<bool, dmabuf_handle>
{
    SC_EXPECT(frame_buffer_);
    SC_EXPECT(n < std::size(frame_buffer_->handles));

    int fd = -1;
    auto const result =
        drmPrimeHandleToFD(drm_fd_, frame_buffer_->handles[n], O_RDONLY, &fd);
    return std::make_pair(result == 0 && fd != -1, dmabuf_handle(fd));
}
