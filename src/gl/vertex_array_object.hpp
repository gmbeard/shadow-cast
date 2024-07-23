#ifndef SHADOW_CAST_GL_VERTEX_ARRAY_OBJECT_HPP_INCLUDED
#define SHADOW_CAST_GL_VERTEX_ARRAY_OBJECT_HPP_INCLUDED

#include "gl/buffer.hpp"
#include "gl/object.hpp"
#include "gl/program.hpp"

namespace sc
{
namespace opengl
{
struct VertexArrayCategory
{
};

struct VertexArrayTraits
{
    using category = VertexArrayCategory;
    auto create() const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
    auto bind(GLenum /*target*/, GLuint name) const noexcept -> void;
};

using VertexArray = ObjectBase<VertexArrayTraits>;
using VertexArrayTarget = TargetBase<0, VertexArrayTraits>;
using BoundVertexArray = BoundTarget<VertexArrayTarget, VertexArray>;
constexpr VertexArrayTarget vertex_array_target {};

auto enable_vertex_array_attrib(BoundVertexArray& vertex_array, GLuint index)
    -> void;
auto disable_vertex_array_attrib(BoundVertexArray& vertex_array, GLuint index)
    -> void;
auto draw_arrays(BoundVertexArray const& vertex_array,
                 BoundProgram const& program,
                 GLenum mode,
                 GLint first,
                 GLsizei count) -> void;

auto draw_elements(BoundVertexArray const& bound_target,
                   BoundTarget<BufferTarget<GL_ELEMENT_ARRAY_BUFFER>,
                               Buffer> const& bound_element_array,
                   BoundProgram const& program,
                   GLenum mode,
                   GLsizei count,
                   GLenum type,
                   const void* indices) -> void;
} // namespace opengl

} // namespace sc

#endif // SHADOW_CAST_GL_VERTEX_ARRAY_OBJECT_HPP_INCLUDED
