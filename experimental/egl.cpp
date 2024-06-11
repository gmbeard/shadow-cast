#include "egl.hpp"

namespace sc
{
namespace gl
{
auto lib() -> GL&
{
    static GL gl_library = load_gl();
    return gl_library;
}

} // namespace gl

namespace egl
{
auto lib() -> EGL&
{
    static EGL egl_library = load_egl();
    return egl_library;
}

auto DisplayDeleter::operator()(EGLDisplay ptr) const noexcept -> void
{
    lib().eglTerminate(ptr);
}

auto ContextDeleter::operator()(EGLContext ptr) const noexcept -> void
{
    lib().eglDestroyContext(display, ptr);
}

auto SurfaceDeleter::operator()(EGLSurface ptr) const noexcept -> void
{
    lib().eglDestroySurface(display, ptr);
}
} // namespace egl

} // namespace sc
