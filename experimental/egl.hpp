#ifndef SHADOW_CAST_PLATFORM_EGL_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_EGL_HPP_INCLUDED

#include "platform/egl.hpp"
#include "platform/gl.hpp"
#include <EGL/egl.h>
#include <X11/Xlib.h>
#include <memory>

namespace sc
{

namespace gl
{
auto lib() -> GL&;
}

namespace egl
{
auto lib() -> EGL&;

struct DisplayDeleter
{
    auto operator()(EGLDisplay ptr) const noexcept -> void;
};

struct ContextDeleter
{
    auto operator()(EGLContext ptr) const noexcept -> void;
    EGLDisplay display;
};

struct SurfaceDeleter
{
    auto operator()(EGLSurface ptr) const noexcept -> void;
    EGLDisplay display;
};

using DisplayPtr =
    std::unique_ptr<std::remove_pointer_t<EGLDisplay>, DisplayDeleter>;
using ContextPtr =
    std::unique_ptr<std::remove_pointer_t<EGLContext>, ContextDeleter>;
using SurfacePtr =
    std::unique_ptr<std::remove_pointer_t<EGLSurface>, SurfaceDeleter>;
} // namespace egl

template <typename DisplayPlatform>
auto create_egl_display(DisplayPlatform const& display_platform)
    -> egl::DisplayPtr
{
    auto* display = display_platform.display();
    auto egl_display = egl::lib().eglGetDisplay(display);

    if (auto result = egl::lib().eglInitialize(egl_display, nullptr, nullptr);
        !result) {
        throw std::runtime_error { "Couldn't initialize EGL display" };
    }

    return egl::DisplayPtr { egl_display };
}

template <typename DisplayPlatform>
struct Gfx
{
    template <typename DisplayPlatform_>
    friend auto bind_gfx(DisplayPlatform&& display_platform);

    explicit Gfx(DisplayPlatform&& display_platform,
                 egl::DisplayPtr&& egl_display,
                 egl::ContextPtr&& egl_context,
                 egl::SurfacePtr&& egl_surface) noexcept
        : display_platform_ { std::move(display_platform) }
        , egl_display_ { std::move(egl_display) }
        , egl_context_ { std::move(egl_context) }
        , egl_surface_ { std::move(egl_surface) }
    {
    }

    auto native_display() noexcept -> DisplayPlatform&
    {
        return display_platform_;
    }

    auto native_display() const noexcept -> DisplayPlatform const&
    {
        return display_platform_;
    }

    auto egl_display() const noexcept -> EGLDisplay
    {
        return egl_display_.get();
    }

    template <typename F>
    auto with_egl_context_args(F f) -> void
    {
        if (auto const result =
                sc::egl::lib().eglMakeCurrent(egl_display_.get(),
                                              egl_surface_.get(),
                                              egl_surface_.get(),
                                              egl_context_.get());
            !result) {
            throw std::runtime_error { "Couldn't push current EGL context" };
        }

        SC_SCOPE_GUARD([&] {
            sc::egl::lib().eglMakeCurrent(egl_display_.get(),
                                          EGL_NO_SURFACE,
                                          EGL_NO_SURFACE,
                                          EGL_NO_CONTEXT);
        });

        f(egl_display_.get(), egl_context_.get(), egl_surface_.get());
    }

    template <typename F>
    auto with_egl_context(F f) -> void
    {
        with_egl_context_args(
            [&](auto /*display*/, auto /*context*/, auto /*surface*/) { f(); });
    }

private:
    DisplayPlatform display_platform_;
    egl::DisplayPtr egl_display_;
    egl::ContextPtr egl_context_;
    egl::SurfacePtr egl_surface_;
};

template <typename DisplayPlatform>
auto bind_gfx(DisplayPlatform&& display_platform)
{
    egl::DisplayPtr egl_display { egl::lib().eglGetDisplay(
        display_platform.display()) };

    if (auto result =
            egl::lib().eglInitialize(egl_display.get(), nullptr, nullptr);
        !result) {
        throw std::runtime_error { "Couldn't initialize EGL display" };
    }

    EGLint attribute_list[] = {
        EGL_RED_SIZE, 1, EGL_GREEN_SIZE, 1, EGL_BLUE_SIZE, 1, EGL_NONE,
    };

    EGLConfig egl_config;
    EGLint num_egl_configs;
    if (auto const result = sc::egl::lib().eglChooseConfig(egl_display.get(),
                                                           attribute_list,
                                                           &egl_config,
                                                           1,
                                                           &num_egl_configs);
        result != EGL_TRUE) {
        throw std::runtime_error { "No suitable EGL config found" };
    }

    if (auto const result = sc::egl::lib().eglBindAPI(EGL_OPENGL_API); !result)
        throw std::runtime_error { "Couldn't bind OpenGL ES API" };

    egl::ContextPtr egl_context {
        sc::egl::lib().eglCreateContext(
            egl_display.get(), egl_config, EGL_NO_CONTEXT, nullptr),
        egl::ContextDeleter { egl_display.get() }
    };

    egl::SurfacePtr egl_surface {
        sc::egl::lib().eglCreateWindowSurface(
            egl_display.get(), egl_config, display_platform.window(), nullptr),
        egl::SurfaceDeleter { egl_display.get() }
    };

    if (egl_surface.get() == EGL_NO_SURFACE)
        throw std::runtime_error { "Failed to create EGL surface" };

    return Gfx { std::move(display_platform),
                 std::move(egl_display),
                 std::move(egl_context),
                 std::move(egl_surface) };
}

} // namespace sc

#endif // SHADOW_CAST_PLATFORM_EGL_HPP_INCLUDED
