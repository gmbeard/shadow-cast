#include "platform/egl.hpp"
#include "error.hpp"
#include "platform/opengl.hpp"
#include "utils/symbol.hpp"

#define TRY_ATTACH_EXTENSION_SYMBOL(target, name, egl)                         \
    ::attach_extension_symbol(target, name, egl)

namespace
{
template <typename F>
auto attach_extension_symbol(F** fn, char const* name, sc::EGL egl) -> void
{
    *fn = reinterpret_cast<F*>(egl.eglGetProcAddress(name));
    if (!*fn)
        throw sc::ModuleError {
            std::string { "Couldn't load extension symbol: " } + name
        };
}

auto load_egl() -> sc::EGL
{
    auto lib = dlopen("libEGL.so.1", RTLD_LAZY);
    if (!lib)
        throw sc::ModuleError { std::string { "Couldn't load libEGL.so.1: " } +
                                dlerror() };

    sc::EGL egl {};
    TRY_ATTACH_SYMBOL(&egl.eglCreateImage, "eglCreateImage", lib);
    TRY_ATTACH_SYMBOL(&egl.eglDestroyImage, "eglDestroyImage", lib);
    TRY_ATTACH_SYMBOL(&egl.eglGetError, "eglGetError", lib);
    TRY_ATTACH_SYMBOL(&egl.eglSwapInterval, "eglSwapInterval", lib);
    TRY_ATTACH_SYMBOL(&egl.eglGetDisplay, "eglGetDisplay", lib);
    TRY_ATTACH_SYMBOL(&egl.eglGetPlatformDisplay, "eglGetPlatformDisplay", lib);
    TRY_ATTACH_SYMBOL(&egl.eglInitialize, "eglInitialize", lib);
    TRY_ATTACH_SYMBOL(&egl.eglTerminate, "eglTerminate", lib);
    TRY_ATTACH_SYMBOL(&egl.eglBindAPI, "eglBindAPI", lib);
    TRY_ATTACH_SYMBOL(&egl.eglChooseConfig, "eglChooseConfig", lib);
    TRY_ATTACH_SYMBOL(&egl.eglCreateContext, "eglCreateContext", lib);
    TRY_ATTACH_SYMBOL(&egl.eglDestroyContext, "eglDestroyContext", lib);
    TRY_ATTACH_SYMBOL(
        &egl.eglCreateWindowSurface, "eglCreateWindowSurface", lib);
    TRY_ATTACH_SYMBOL(&egl.eglDestroySurface, "eglDestroySurface", lib);
    TRY_ATTACH_SYMBOL(&egl.eglQueryString, "eglQueryString", lib);
    TRY_ATTACH_SYMBOL(&egl.eglGetProcAddress, "eglGetProcAddress", lib);
    TRY_ATTACH_SYMBOL(&egl.eglMakeCurrent, "eglMakeCurrent", lib);
    TRY_ATTACH_SYMBOL(&egl.eglSwapBuffers, "eglSwapBuffers", lib);

    return egl;
}

} // namespace

namespace sc
{

auto egl() -> EGL const&
{
    static EGL egl_module = load_egl();
    return egl_module;
}

auto load_gl_extensions() -> void
{
    auto& opengl = detail::get_opengl_module();
    /* NOTE:
     *  Extensions...
     */
    TRY_ATTACH_EXTENSION_SYMBOL(&opengl.glEGLImageTargetTexture2DOES,
                                "glEGLImageTargetTexture2DOES",
                                egl());
}

} // namespace sc
