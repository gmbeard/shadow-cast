#ifndef SHADOW_CAST_PLATFORM_WAYLAND_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_WAYLAND_HPP_INCLUDED

#include "./egl.hpp"
#include <memory>
#include <type_traits>
#include <wayland-client.h>
#include <wayland-egl.h>

namespace sc
{

namespace wayland
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

} // namespace wayland

namespace egl
{
struct EGLDeleter
{
    EGLDeleter(EGL e, EGLDisplay d) noexcept;
    EGL egl;
    EGLDisplay display;
};

struct EGLDisplayDeleter
{
    auto operator()(EGLDisplay ptr) const noexcept -> void;
    EGL egl;
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

} // namespace egl

struct Wayland
{
    wayland::DisplayPtr display;
    wayland::RegistryPtr registry;
    wayland::OutputPtr output;
    wayland::CompositorPtr compositor;
    wayland::SurfacePtr surface;
    wayland::WindowPtr window;

    std::uint32_t output_width { 0 };
    std::uint32_t output_height { 0 };
};

struct WaylandEGL
{
    egl::EGLDisplayPtr egl_display;
    egl::EGLSurfacePtr egl_surface;
    egl::EGLContextPtr egl_context;
};

auto initialize_wayland(wayland::DisplayPtr) -> std::unique_ptr<Wayland>;
auto initialize_wayland_egl(EGL, Wayland const&) -> WaylandEGL;

} // namespace sc

#endif // SHADOW_CAST_PLATFORM_WAYLAND_HPP_INCLUDED
