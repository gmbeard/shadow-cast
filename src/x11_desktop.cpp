#include "./x11_desktop.hpp"
#include "av/codec.hpp"
#include "display/display.hpp"
#include "logging.hpp"
#include "platform/egl.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include "wayland_desktop.hpp"
#include <EGL/egl.h>
#include <GL/gl.h>
#include <X11/Xlib.h>

namespace
{
auto initialize_window(Display* display, Window& window) noexcept -> bool
{
    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display));

    window = XCreateSimpleWindow(display,
                                 XRootWindow(display, XDefaultScreen(display)),
                                 0,
                                 0,
                                 output_width,
                                 output_height,
                                 0,
                                 XBlackPixel(display, XDefaultScreen(display)),
                                 XBlackPixel(display, XDefaultScreen(display)));

    return true;
}

auto initialize_x11_egl(Display* display,
                        Window window,
                        sc::detail::EGL& x11egl) noexcept -> bool
{
    using sc::egl;
    using sc::log;
    using sc::LogLevel;

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
        return false;
    }

    x11egl.display = egl().eglGetDisplay(display);

    if (!x11egl.display) {
        log(LogLevel::error, "Failed to get EGL display");
        return false;
    }

    if (!egl().eglInitialize(x11egl.display, nullptr, nullptr)) {
        log(LogLevel::error, "Failed to initialize EGL display");
        return false;
    }

    EGLConfig egl_config;
    std::int32_t num_egl_config = 0;
    if (!egl().eglChooseConfig(
            x11egl.display, attr, &egl_config, 1, &num_egl_config) ||
        num_egl_config < 1) {
        log(LogLevel::error, "Failed to select EGL config");
        return false;
    }

    x11egl.surface = egl().eglCreateWindowSurface(
        x11egl.display,
        egl_config,
        reinterpret_cast<EGLNativeWindowType>(window),
        nullptr);

    if (!x11egl.surface) {
        log(LogLevel::error, "Failed to create EGL surface");
        return false;
    }

    x11egl.context =
        egl().eglCreateContext(x11egl.display, egl_config, nullptr, ctxattr);

    if (!x11egl.context) {
        log(LogLevel::error, "Failed to create EGL context");
        return false;
    }

    return true;
}

} // namespace

namespace sc
{
X11Desktop::X11Desktop() noexcept
    : display_ { XOpenDisplay(nullptr) }
{
    /* TODO:
     *  Handle errors by setting...
     *      int (*XSetErrorHandler(handler))()
     *            int (*handler)(Display *, XErrorEvent *)
     */
    initialized_ = !!display_ && initialize_window(display_.get(), window_) &&
                   initialize_x11_egl(display_.get(), window_, x11egl_);
}

X11Desktop::operator bool() const noexcept { return initialized_; }

auto X11Desktop::egl_display() const noexcept -> EGLDisplay
{
    SC_EXPECT(initialized_);

    return x11egl_.display;
}

auto X11Desktop::size() const noexcept -> VideoOutputSize
{
    SC_EXPECT(initialized_);

    std::uint32_t output_width =
        XWidthOfScreen(DefaultScreenOfDisplay(display_.get()));
    std::uint32_t output_height =
        XHeightOfScreen(DefaultScreenOfDisplay(display_.get()));

    return { .width = output_width, .height = output_height };
}

auto X11Desktop::gpu_vendor() const noexcept -> std::string_view
{
    SC_EXPECT(initialized_);
    return egl().eglQueryString(x11egl_.display, EGL_VENDOR);
}

auto X11Desktop::gpu_id() const noexcept -> std::string_view
{
    SC_EXPECT(initialized_);
    if (!egl().eglMakeCurrent(x11egl_.display,
                              x11egl_.surface,
                              x11egl_.surface,
                              x11egl_.context)) {
        log(LogLevel::error, "Couldn't select context");
        return "";
    }

    SC_SCOPE_GUARD([&] {
        egl().eglMakeCurrent(
            x11egl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    });

    return reinterpret_cast<char const*>(gl().glGetString(GL_RENDERER));
}
} // namespace sc
