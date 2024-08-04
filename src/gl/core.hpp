#ifndef SHADOW_CAST_GL_CORE_HPP_INCLUDED
#define SHADOW_CAST_GL_CORE_HPP_INCLUDED

#include <GL/gl.h>

namespace sc::opengl
{
auto viewport(GLint x, GLint y, GLsizei width, GLsizei height) -> void;
auto clear(GLenum mask) -> void;
auto clear_color(float red, float green, float blue, float alpha) noexcept
    -> void;

auto enable(GLenum cap) -> void;
auto blend_function(GLenum sfactor, GLenum dfactor) -> void;

} // namespace sc::opengl

#endif // SHADOW_CAST_GL_CORE_HPP_INCLUDED
