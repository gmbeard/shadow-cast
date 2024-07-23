#include "gl/framebuffer.hpp"
#include "gl/error.hpp"
#include "platform/opengl.hpp"

namespace sc::opengl
{
auto FramebufferTraits::create() const -> GLuint
{
    GLuint name;
    gl().glGenFramebuffers(1, &name);
    SC_CHECK_GL_ERROR("glGenFramebuffers");
    return name;
}

auto FramebufferTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteFramebuffers(1, &name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteFramebuffers");
}

auto FramebufferTraits::bind(GLenum target, GLuint name) const noexcept -> void
{
    gl().glBindFramebuffer(target, name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glBindFramebuffer");
}

} // namespace sc::opengl
