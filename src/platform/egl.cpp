#include "platform/egl.hpp"
#include "error.hpp"
#include "utils/symbol.hpp"

namespace sc
{

auto load_egl() -> EGL
{
    auto lib = dlopen("libEGL.so.1", RTLD_LAZY);
    if (!lib)
        throw ModuleError { std::string { "Couldn't load libEGL.so.1: " } +
                            dlerror() };

    EGL egl {};
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

} // namespace sc
