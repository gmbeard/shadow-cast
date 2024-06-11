#include "gl.hpp"
#include "error.hpp"
#include "utils/symbol.hpp"

namespace sc
{
auto load_gl() -> GL
{
    auto lib = dlopen("libGL.so.1", RTLD_LAZY);
    if (!lib)
        throw ModuleError { std::string { "Couldn't load libGL.so.1: " } +
                            dlerror() };

    GL egl {};
    TRY_ATTACH_SYMBOL(&egl.glGetString, "glGetString", lib);
    TRY_ATTACH_SYMBOL(&egl.glGetStringi, "glGetStringi", lib);
    TRY_ATTACH_SYMBOL(&egl.glGetIntegerv, "glGetIntegerv", lib);

    return egl;
}
} // namespace sc
