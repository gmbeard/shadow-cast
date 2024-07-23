#include "gl/buffer.hpp"
#include "gl/error.hpp"

namespace sc::opengl
{
auto BufferTraits::create() const -> GLuint
{
    GLuint name;
    gl().glGenBuffers(1, &name);
    SC_CHECK_GL_ERROR("glGenBuffers");
    return name;
}

auto BufferTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteBuffers(1, &name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteBuffers");
}

auto BufferTraits::bind(GLenum target, GLint name) const noexcept -> void
{
    gl().glBindBuffer(target, name);
    SC_CHECK_GL_ERROR("glBindBuffer");
}

auto vertex_attrib_pointer(
    BoundTarget<BufferTarget<GL_ARRAY_BUFFER>, Buffer> const& /*array_buffer*/,
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    void const* pointer) -> void
{
    gl().glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    SC_CHECK_GL_ERROR("glVertexAttribPointer");
}
} // namespace sc::opengl
