#include "platform/wayland.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <EGL/eglext.h>
#include <cinttypes>
#include <string_view>

namespace
{

wl_output_listener const output_listener = {
    .geometry = [](void* /*data*/,
                   struct wl_output* /*wl_output*/,
                   int32_t /*x*/,
                   int32_t /*y*/,
                   int32_t /*physical_width*/,
                   int32_t /*physical_height*/,
                   int32_t /*subpixel*/,
                   const char* /*make*/,
                   const char* /*model*/,
                   int32_t /*transform*/) {},

    .mode =
        [](void* data,
           struct wl_output* /*output*/,
           uint32_t flags,
           int32_t width,
           int32_t height,
           int32_t /*refresh*/) {
            if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
                return;

            auto& platform = *reinterpret_cast<sc::Wayland*>(data);
            platform.output_width = width;
            platform.output_height = height;
        },

    .done = [](void* /*data*/, struct wl_output* /*wl_output*/) {},

    .scale = [](void* /*data*/,
                struct wl_output* /*wl_output*/,
                int32_t /*factor*/) {},

    .name = [](void* /*data*/,
               struct wl_output* /*wl_output*/,
               const char* /*name*/) {},

    .description = [](void* /*data*/,
                      struct wl_output* /*wl_output*/,
                      const char* /*description*/) {},
};

wl_registry_listener const registry_listener = {
    .global =
        [](void* data,
           wl_registry* registry,
           std::uint32_t name,
           char const* iface,
           std::uint32_t version) {
            auto* platform = reinterpret_cast<sc::Wayland*>(data);
            std::string_view interface = iface;

            if (interface == wl_compositor_interface.name) {
                platform->compositor.reset(
                    reinterpret_cast<wl_compositor*>(wl_registry_bind(
                        registry, name, &wl_compositor_interface, version)));
            }
            else if (interface == wl_output_interface.name) {
                platform->output.reset(
                    reinterpret_cast<wl_output*>(wl_registry_bind(
                        registry, name, &wl_output_interface, version)));

                wl_output_add_listener(
                    platform->output.get(), &output_listener, platform);
            }
        },
    .global_remove = [](void* /*data*/,
                        wl_registry* /*registry*/,
                        std::uint32_t /*name*/) {},
};

} // namespace

namespace sc
{

namespace wayland
{

auto DisplayDeleter::operator()(wl_display* ptr) const noexcept -> void
{
    wl_display_disconnect(ptr);
}

auto RegistryDeleter::operator()(wl_registry* ptr) const noexcept -> void
{
    wl_registry_destroy(ptr);
}

auto CompositorDeleter::operator()(wl_compositor* ptr) const noexcept -> void
{
    wl_compositor_destroy(ptr);
}

auto SurfaceDeleter::operator()(wl_surface* ptr) const noexcept -> void
{
    wl_surface_destroy(ptr);
}

auto WindowDeleter::operator()(wl_egl_window* ptr) const noexcept -> void
{
    wl_egl_window_destroy(ptr);
}

auto OutputDeleter::operator()(wl_output* ptr) const noexcept -> void
{
    wl_output_release(ptr);
}

} // namespace wayland

EGLDeleter::EGLDeleter(EGL e, EGLDisplay d) noexcept
    : egl { e }
    , display { d }
{
}

auto EGLDisplayDeleter::operator()(EGLDisplay ptr) const noexcept -> void
{
    egl.eglTerminate(ptr);
}

auto EGLContextDeleter::operator()(EGLContext ptr) const noexcept -> void
{
    egl.eglDestroyContext(display, ptr);
}

auto EGLSurfaceDeleter::operator()(EGLSurface ptr) const noexcept -> void
{
    egl.eglDestroySurface(display, ptr);
}

auto initialize_wayland(wayland::DisplayPtr display) -> std::unique_ptr<Wayland>
{
    auto platform = std::make_unique<Wayland>();

    platform->display = std::move(display);
    if (!platform->display)
        throw std::runtime_error { "Failed to connect to Wayland display" };

    platform->registry.reset(wl_display_get_registry(platform->display.get()));
    if (!platform->registry)
        throw std::runtime_error { "Failed to get Wayland registry" };

    SC_SCOPE_GUARD([&] { platform->registry.reset(nullptr); });

    wl_registry_add_listener(
        platform->registry.get(), &registry_listener, platform.get());

    wl_display_roundtrip(platform->display.get());

    wl_display_roundtrip(platform->display.get());

    SC_EXPECT(platform->output_width);
    SC_EXPECT(platform->output_height);

    SC_SCOPE_GUARD([&] { platform->output.reset(nullptr); });

    if (!platform->compositor)
        throw std::runtime_error { "Failed to get Wayland compositor" };

    platform->surface.reset(
        wl_compositor_create_surface(platform->compositor.get()));
    if (!platform->surface)
        throw std::runtime_error { "Failed to create Wayland surface" };

    platform->window.reset(
        wl_egl_window_create(platform->surface.get(), 16, 16));
    if (!platform->window)
        throw std::runtime_error { "Failed to create Wayland window" };

    return platform;
}

auto initialize_wayland_egl(EGL egl, Wayland const& platform) -> WaylandEGL
{
    // clang-format off
    const int32_t attr[] = {
        EGL_BUFFER_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE,
    };

    const int32_t ctxattr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG, /* requires cap_sys_nice, ignored
                                          otherwise */
        EGL_NONE,
    };
    // clang-format on

    if (!egl.eglBindAPI(EGL_OPENGL_API))
        throw std::runtime_error { "Failed to bind EGL API" };

    EGLDisplayPtr egl_display { egl.eglGetDisplay(platform.display.get()),
                                { egl } };

    if (!egl_display)
        throw std::runtime_error { "Failed to get EGL display" };

    if (!egl.eglInitialize(egl_display.get(), nullptr, nullptr)) {
        egl_display.release();
        throw std::runtime_error { "Failed to initialize EGL display" };
    }

    EGLConfig egl_config;
    std::int32_t num_egl_config = 0;
    if (!egl.eglChooseConfig(
            egl_display.get(), attr, &egl_config, 1, &num_egl_config)) {
        throw std::runtime_error { "Failed to select EGL config" };
    }

    EGLSurfacePtr egl_surface { egl.eglCreateWindowSurface(
                                    egl_display.get(),
                                    egl_config,
                                    reinterpret_cast<EGLNativeWindowType>(
                                        platform.window.get()),
                                    nullptr),
                                { egl, egl_display.get() } };

    if (!egl_surface)
        throw std::runtime_error { "Failed to create EGL surface" };

    EGLContextPtr egl_context {
        egl.eglCreateContext(egl_display.get(), egl_config, nullptr, ctxattr),
        { egl, egl_display.get() }
    };

    if (!egl_context)
        throw std::runtime_error { "Failed to create EGL context" };

    if (!egl.eglMakeCurrent(egl_display.get(),
                            egl_surface.get(),
                            egl_surface.get(),
                            egl_context.get()))
        throw std::runtime_error { "Couldn't select context" };

    return {
        .egl_display = std::move(egl_display),
        .egl_surface = std::move(egl_surface),
        .egl_context = std::move(egl_context),
    };
}

} // namespace sc
