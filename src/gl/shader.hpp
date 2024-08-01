#ifndef SHADOW_CAST_GL_SHADER_HPP_INCLUDED
#define SHADOW_CAST_GL_SHADER_HPP_INCLUDED

#include "gl/object.hpp"
#include <GL/gl.h>
#include <string_view>

namespace sc
{
namespace opengl
{

struct ShaderCategory
{
};

enum class ShaderType
{
    vertex,
    fragment,
};

struct ShaderTraits
{
    using category = ShaderCategory;

    auto create(GLenum shader_type) const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
};

using Shader = ObjectBase<ShaderTraits>;

auto create_shader(ShaderType type) -> Shader;
auto shader_source(Shader& shader, std::string_view) -> void;
auto compile_shader(Shader& shader) -> void;

} // namespace opengl

} // namespace sc
#endif // SHADOW_CAST_GL_SHADER_HPP_INCLUDED
