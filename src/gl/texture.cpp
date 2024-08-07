#include "gl/texture.hpp"
#include "gl/error.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include <GL/gl.h>
#include <cstdio>

namespace sc::opengl
{

auto TextureTraits::bind(GLenum target, GLuint name) const noexcept -> void
{
    gl().glBindTexture(target, name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glBindTexture");
}

auto TextureTraits::create() const -> GLuint
{
    GLuint name;
    gl().glGenTextures(1, &name);
    SC_CHECK_GL_ERROR("glGenTextures");
    return name;
}

auto TextureTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteTextures(1, &name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteTextures");
}

} // namespace sc::opengl
