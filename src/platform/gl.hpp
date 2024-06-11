#ifndef SHADOW_CAST_PLATFORM_GL_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_GL_HPP_INCLUDED

#include <GL/gl.h>

namespace sc
{

struct GL
{
    GLubyte* (*glGetString)(GLenum /* name */);
    GLubyte* (*glGetStringi)(GLenum /* name */, GLint /* index */);
    GLubyte* (*glGetIntegerv)(GLenum /* target */, GLint* /* data */);
};

auto load_gl() -> GL;

} // namespace sc
#endif // SHADOW_CAST_PLATFORM_GL_HPP_INCLUDED
