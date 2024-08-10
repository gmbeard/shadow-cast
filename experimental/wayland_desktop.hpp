#ifndef SHADOW_CAST_WAYLAND_DESKTOP_HPP_INCLUDED
#define SHADOW_CAST_WAYLAND_DESKTOP_HPP_INCLUDED

#include "av/codec.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string_view>
#include <wayland-client-protocol.h>
#include <wayland-egl-core.h>

namespace sc
{
namespace detail
{
struct DisplayDeleter
{
    auto operator()(wl_display* ptr) const noexcept -> void;
};

using DisplayPtr = std::unique_ptr<wl_display, DisplayDeleter>;

struct RegistryDeleter
{
    auto operator()(wl_registry* ptr) const noexcept -> void;
};

using RegistryPtr = std::unique_ptr<wl_registry, RegistryDeleter>;

struct CompositorDeleter
{
    auto operator()(wl_compositor* ptr) const noexcept -> void;
};

using CompositorPtr = std::unique_ptr<wl_compositor, CompositorDeleter>;

struct SurfaceDeleter
{
    auto operator()(wl_surface* ptr) const noexcept -> void;
};

using SurfacePtr = std::unique_ptr<wl_surface, SurfaceDeleter>;

struct WindowDeleter
{
    auto operator()(wl_egl_window* ptr) const noexcept -> void;
};

using WindowPtr = std::unique_ptr<wl_egl_window, WindowDeleter>;

struct OutputDeleter
{
    auto operator()(wl_output* ptr) const noexcept -> void;
};

using OutputPtr = std::unique_ptr<wl_output, OutputDeleter>;

struct EGLDeleter
{
    EGLDeleter(EGLDisplay d) noexcept;
    EGLDisplay display;
};

struct EGLDisplayDeleter
{
    auto operator()(EGLDisplay ptr) const noexcept -> void;
};

using EGLDisplayPtr =
    std::unique_ptr<std::remove_pointer_t<EGLDisplay>, EGLDisplayDeleter>;

struct EGLContextDeleter : EGLDeleter
{
    using EGLDeleter::EGLDeleter;

    auto operator()(EGLContext ptr) const noexcept -> void;
};

using EGLContextPtr =
    std::unique_ptr<std::remove_pointer_t<EGLContext>, EGLContextDeleter>;

struct EGLSurfaceDeleter : EGLDeleter
{
    using EGLDeleter::EGLDeleter;

    auto operator()(EGLSurface ptr) const noexcept -> void;
};

using EGLSurfacePtr =
    std::unique_ptr<std::remove_pointer_t<EGLSurface>, EGLSurfaceDeleter>;

struct WaylandData
{
    detail::DisplayPtr display;
    detail::RegistryPtr registry;
    detail::OutputPtr output;
    detail::CompositorPtr compositor;
    detail::SurfacePtr surface;
    detail::WindowPtr window;
    VideoOutputSize video_output_size {};
};

struct EGL
{
    EGL() noexcept = default;
    EGL(EGL&&) noexcept;
    ~EGL();
    auto operator=(EGL&&) noexcept -> EGL&;

    EGLDisplay display { nullptr };
    EGLSurface surface { nullptr };
    EGLContext context { nullptr };
};

} // namespace detail

struct WaylandDesktop
{
    WaylandDesktop() noexcept;

    operator bool() const noexcept;
    auto size() const noexcept -> VideoOutputSize const&;
    auto egl_display() const noexcept -> EGLDisplay;
    auto gpu_vendor() const noexcept -> std::string_view;
    auto gpu_id() const noexcept -> std::string_view;

private:
    bool initialized_ { false };
    detail::WaylandData data_;
    detail::EGL egl_;
};
} // namespace sc

#endif // SHADOW_CAST_WAYLAND_DESKTOP_HPP_INCLUDED
