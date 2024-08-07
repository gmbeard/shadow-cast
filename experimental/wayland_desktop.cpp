#include "wayland_desktop.hpp"
#include "logging.hpp"
#include "platform/egl.hpp"
#include "platform/wayland.hpp"
#include <optional>
#include <utility>
#include <wayland-client-core.h>

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

            auto& wayland_data =
                *reinterpret_cast<sc::detail::WaylandData*>(data);
            wayland_data.video_output_size.width = width;
            wayland_data.video_output_size.height = height;
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
            auto& wayland_data =
                *reinterpret_cast<sc::detail::WaylandData*>(data);
            std::string_view interface = iface;

            if (interface == wl_compositor_interface.name) {
                wayland_data.compositor.reset(
                    reinterpret_cast<wl_compositor*>(wl_registry_bind(
                        registry, name, &wl_compositor_interface, version)));
            }
            else if (interface == wl_output_interface.name) {
                wayland_data.output.reset(
                    reinterpret_cast<wl_output*>(wl_registry_bind(
                        registry, name, &wl_output_interface, version)));

                wl_output_add_listener(
                    wayland_data.output.get(), &output_listener, &wayland_data);
            }
        },
    .global_remove = [](void* /*data*/,
                        wl_registry* /*registry*/,
                        std::uint32_t /*name*/) {},
};

auto initialize_wayland_desktop() noexcept
    -> std::optional<sc::detail::WaylandData>
{
    using sc::log;
    using sc::LogLevel;
    sc::detail::WaylandData data;

    data.display.reset(wl_display_connect(nullptr));
    if (!data.display) {
        log(LogLevel::warn, "Couldn't connect to Wayland display");
        return std::nullopt;
    }

    data.registry.reset(wl_display_get_registry(data.display.get()));
    if (!data.registry) {
        log(LogLevel::error, "Failed to get Wayland registry");
        return std::nullopt;
    }

    SC_SCOPE_GUARD([&] { data.registry.reset(nullptr); });

    wl_registry_add_listener(data.registry.get(), &registry_listener, &data);

    wl_display_dispatch(data.display.get());
    wl_display_roundtrip(data.display.get());

    SC_EXPECT(data.video_output_size.width);
    SC_EXPECT(data.video_output_size.height);

    SC_SCOPE_GUARD([&] { data.output.reset(nullptr); });

    if (!data.compositor) {
        log(LogLevel::error, "Failed to get Wayland compositor");
        return std::nullopt;
    }

    data.surface.reset(wl_compositor_create_surface(data.compositor.get()));

    if (!data.surface) {
        log(LogLevel::error, "Failed to create Wayland surface");
        return std::nullopt;
    }

    data.window.reset(wl_egl_window_create(data.surface.get(), 16, 16));

    if (!data.window) {
        log(LogLevel::error, "Failed to create Wayland window");
        return std::nullopt;
    }

    return data;
}

auto initialize_desktop_egl(wl_display* display, wl_egl_window* window) noexcept
    -> std::optional<sc::detail::EGL>
{
    using sc::egl;
    using sc::log;
    using sc::LogLevel;

    sc::detail::EGL data;

    // clang-format off
    const int32_t attr[] = {
        EGL_BUFFER_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE,
    };

    const int32_t ctxattr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
        EGL_NONE,
    };
    // clang-format on

    if (!egl().eglBindAPI(EGL_OPENGL_API)) {
        log(LogLevel::error, "Failed to bind EGL API");
        return std::nullopt;
    }

    data.display = egl().eglGetDisplay(display);

    if (!data.display) {
        log(LogLevel::error, "Failed to get EGL display");
        return std::nullopt;
    }

    if (!egl().eglInitialize(data.display, nullptr, nullptr)) {
        log(LogLevel::error, "Failed to initialize EGL display");
        return std::nullopt;
    }

    EGLConfig egl_config;
    std::int32_t num_egl_config = 0;
    if (!egl().eglChooseConfig(
            data.display, attr, &egl_config, 1, &num_egl_config) ||
        num_egl_config < 1) {
        log(LogLevel::error, "Failed to select EGL config");
        return std::nullopt;
    }

    data.surface = egl().eglCreateWindowSurface(
        data.display,
        egl_config,
        reinterpret_cast<EGLNativeWindowType>(window),
        nullptr);

    if (!data.surface) {
        log(LogLevel::error, "Failed to create EGL surface");
        return std::nullopt;
    }

    data.context =
        egl().eglCreateContext(data.display, egl_config, nullptr, ctxattr);

    if (!data.context) {
        log(LogLevel::error, "Failed to create EGL context");
        return std::nullopt;
    }

    if (!egl().eglMakeCurrent(
            data.display, data.surface, data.surface, data.context)) {
        log(LogLevel::error, "Couldn't select context");
        return std::nullopt;
    }

    return data;
}

} // namespace

namespace sc
{
WaylandDesktop::WaylandDesktop() noexcept
{
    if (auto data = initialize_wayland_desktop(); data) {
        if (auto egl_data =
                initialize_desktop_egl(data->display.get(), data->window.get());
            egl_data) {
            data_ = std::move(*data);
            egl_ = std::move(*egl_data);
            initialized_ = true;
        }
    }
}

auto WaylandDesktop::size() const noexcept -> VideoOutputSize const&
{
    return data_.video_output_size;
}

auto WaylandDesktop::egl_display() const noexcept -> EGLDisplay
{
    return egl_.display;
}

WaylandDesktop::operator bool() const noexcept { return initialized_; }

namespace detail
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

EGL::EGL(EGL&& other) noexcept
    : display { std::exchange(other.display, nullptr) }
    , surface { std::exchange(other.surface, nullptr) }
    , context { std::exchange(other.context, nullptr) }
{
}

EGL::~EGL()
{
    if (context) {
        SC_EXPECT(display);
        egl().eglDestroyContext(display, context);
    }

    if (surface) {
        SC_EXPECT(display);
        egl().eglDestroySurface(display, surface);
    }

    if (display) {
        egl().eglTerminate(display);
    }
}

auto EGL::operator=(EGL&& rhs) noexcept -> EGL&
{
    using std::swap;
    auto tmp { std::move(rhs) };
    swap(display, tmp.display);
    swap(surface, tmp.surface);
    swap(context, tmp.context);

    return *this;
}

} // namespace detail

} // namespace sc
