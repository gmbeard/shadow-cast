#ifndef SHADOW_CAST_GL_BUFFER_HPP_INCLUDED
#define SHADOW_CAST_GL_BUFFER_HPP_INCLUDED

#include "gl/error.hpp"
#include "gl/object.hpp"
#include "platform/opengl.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <span>
#include <tuple>
#include <type_traits>
namespace sc
{

namespace opengl
{
template <typename T>
concept BufferableConcept = std::is_arithmetic_v<T>;

struct BufferCategory
{
};

struct BufferTraits
{
    using category = BufferCategory;

    auto create() const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
    auto bind(GLenum target, GLint name) const noexcept -> void;
};

using Buffer = ObjectBase<BufferTraits>;
template <GLenum T>
using BufferTarget = TargetBase<T, BufferTraits>;

template <typename T>
concept BufferConcept =
    std::is_convertible_v<typename T::category, BufferCategory>;

template <typename T>
concept BoundBufferConcept =
    BindingConcept<std::decay_t<T>> && BufferConcept<T>;

template <BoundBufferConcept A, BufferableConcept T>
auto named_buffer_data(A& array_buffer, std::span<T const> data, GLenum usage)
    -> void
{
    const auto [id, sz, d] =
        std::make_tuple(array_buffer.name(), data.size(), data.data());

    gl().glNamedBufferData(id, sz * sizeof(T), d, usage);
    SC_CHECK_GL_ERROR("glNamedBufferData");
}

template <BoundBufferConcept A, BufferableConcept T>
auto buffer_data(A& array_buffer, std::span<T> data, GLenum usage) -> void
{
    const auto [target, sz, d] =
        std::make_tuple(array_buffer.target(), data.size(), data.data());

    gl().glBufferData(target, sz * sizeof(T), d, usage);
    SC_CHECK_GL_ERROR("glBufferData");
}

auto vertex_attrib_pointer(
    BoundTarget<BufferTarget<GL_ARRAY_BUFFER>, Buffer> const& array_buffer,
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    void const* pointer) -> void;

constexpr BufferTarget<GL_ARRAY_BUFFER> array_buffer_target {};
constexpr BufferTarget<GL_ELEMENT_ARRAY_BUFFER> element_array_buffer_target {};

} // namespace opengl

} // namespace sc

#endif // SHADOW_CAST_GL_BUFFER_HPP_INCLUDED
