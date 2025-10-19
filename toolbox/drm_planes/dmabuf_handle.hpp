#ifndef SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_HANDLE_HPP_INCLUDED
#define SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_HANDLE_HPP_INCLUDED

struct drm_plane_framebuffer;

struct dmabuf_handle
{
    dmabuf_handle() noexcept = default;
    dmabuf_handle(int fd) noexcept;
    dmabuf_handle(dmabuf_handle&&) noexcept;
    ~dmabuf_handle();
    auto operator=(dmabuf_handle) noexcept -> dmabuf_handle&;
    explicit operator int() const noexcept;
    friend auto swap(dmabuf_handle&, dmabuf_handle&) noexcept -> void;
    auto reset() noexcept -> void;
    auto release() noexcept -> int;

private:
    friend struct drm_plane_framebuffer;

    int fd_ { -1 };
};

#endif // SHADOWCAST_TOOLBOX_DRM_PLANES_DMABUF_HANDLE_HPP_INCLUDED
