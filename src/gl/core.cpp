#include "gl/core.hpp"
#include "gl/error.hpp"
#include "platform/opengl.hpp"

namespace sc::opengl
{

auto viewport(GLint x, GLint y, GLsizei width, GLsizei height) -> void
{
    gl().glViewport(x, y, width, height);
    SC_CHECK_GL_ERROR("glViewport");
}

auto clear(GLenum mask) -> void
{
    gl().glClear(mask);
    SC_CHECK_GL_ERROR("glClear");
}

auto clear_color(float red, float green, float blue, float alpha) noexcept
    -> void
{
    gl().glClearColor(red, green, blue, alpha);
}

} // namespace sc::opengl
