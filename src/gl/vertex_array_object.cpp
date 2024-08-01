#include "gl/vertex_array_object.hpp"
#include "gl/error.hpp"
#include "gl/program.hpp"
#include "platform/opengl.hpp"

namespace sc
{
namespace opengl
{
auto VertexArrayTraits::create() const -> GLuint
{
    GLuint name;
    gl().glGenVertexArrays(1, &name);
    SC_CHECK_GL_ERROR("glGenVertexArrays");
    return name;
}

auto VertexArrayTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteVertexArrays(1, &name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteVertexArrays");
}

auto VertexArrayTraits::bind(GLenum /*target*/, GLuint name) const noexcept
    -> void
{
    gl().glBindVertexArray(name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glBindVertexArray");
}

auto enable_vertex_array_attrib(BoundVertexArray& vertex_array, GLuint index)
    -> void
{
    gl().glEnableVertexArrayAttrib(vertex_array.name(), index);
    SC_CHECK_GL_ERROR("glEnableVertexArrayAttrib");
}

auto disable_vertex_array_attrib(BoundVertexArray& vertex_array, GLuint index)
    -> void
{
    gl().glDisableVertexArrayAttrib(vertex_array.name(), index);
    SC_CHECK_GL_ERROR("glDisableVertexArrayAttrib");
}

auto draw_arrays(BoundVertexArray const&,
                 BoundProgram const& bound_program,
                 GLenum mode,
                 GLint first,
                 GLsizei count) -> void
{
    SC_CHECK_GL_BINDING(bound_program);
    gl().glDrawArrays(mode, first, count);
    SC_CHECK_GL_ERROR("glDrawArrays");
}

auto draw_elements(BoundVertexArray const& bound_target,
                   BoundTarget<BufferTarget<GL_ELEMENT_ARRAY_BUFFER>,
                               Buffer> const& bound_element_array,
                   BoundProgram const& bound_program,
                   GLenum mode,
                   GLsizei count,
                   GLenum type,
                   const void* indices) -> void
{
    SC_CHECK_GL_BINDING(bound_target);
    SC_CHECK_GL_BINDING(bound_element_array);
    SC_CHECK_GL_BINDING(bound_program);
    gl().glDrawElements(mode, count, type, indices);
    SC_CHECK_GL_ERROR("glDrawElements");
}

} // namespace opengl

} // namespace sc
