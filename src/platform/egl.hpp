#ifndef SHADOW_CAST_EGL_HPP_INCLUDED
#define SHADOW_CAST_EGL_HPP_INCLUDED

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace sc
{

struct EGL
{
    EGLImage (*eglCreateImage)(EGLDisplay dpy,
                               EGLContext ctx,
                               unsigned int target,
                               EGLClientBuffer buffer,
                               const intptr_t* attrib_list);

    unsigned int (*eglDestroyImage)(EGLDisplay dpy, EGLImage image);

    unsigned int (*eglGetError)(void);

    unsigned int (*eglSwapInterval)(EGLDisplay dpy, int32_t interval);

    EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType display_id);
    EGLDisplay (*eglGetPlatformDisplay)(EGLenum platfor,
                                        void* native_display,
                                        EGLAttrib const* attrib_list);

    unsigned int (*eglInitialize)(EGLDisplay dpy,
                                  int32_t* major,
                                  int32_t* minor);

    unsigned int (*eglTerminate)(EGLDisplay dpy);

    unsigned int (*eglBindAPI)(unsigned int api);

    unsigned int (*eglChooseConfig)(EGLDisplay dpy,
                                    const int32_t* attrib_list,
                                    EGLConfig* configs,
                                    int32_t config_size,
                                    int32_t* num_config);

    EGLContext (*eglCreateContext)(EGLDisplay dpy,
                                   EGLConfig config,
                                   EGLContext share_context,
                                   const int32_t* attrib_list);

    unsigned int (*eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);

    EGLSurface (*eglCreateWindowSurface)(EGLDisplay dpy,
                                         EGLConfig config,
                                         EGLNativeWindowType win,
                                         const int32_t* attrib_list);

    EGLSurface (*eglSwapBuffers)(EGLDisplay dpy, EGLSurface draw);
    unsigned int (*eglDestroySurface)(EGLDisplay dpy, EGLSurface ctx);

    char const* (*eglQueryString)(EGLDisplay dpy, EGLint name);
    void* (*eglGetProcAddress)(char const* name);
    unsigned int (*eglMakeCurrent)(EGLDisplay dpy,
                                   EGLSurface draw,
                                   EGLSurface read,
                                   EGLContext ctx);
};

auto load_egl() -> EGL;

} // namespace sc

#endif // SHADOW_CAST_EGL_HPP_INCLUDED
