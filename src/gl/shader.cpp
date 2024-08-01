#include "gl/shader.hpp"
#include "gl/error.hpp"
#include "gl/object.hpp"
#include "platform/opengl.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <array>

namespace
{
thread_local std::array<char, 1024> shader_compile_info {};
}

namespace sc
{

namespace opengl
{
auto ShaderTraits::create(GLenum shader_type) const -> GLuint
{
    auto const name = gl().glCreateShader(shader_type);
    SC_CHECK_GL_ERROR("glCreateShader");
    return name;
}

auto ShaderTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteShader(name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteShader");
}

auto create_shader(ShaderType type) -> Shader
{
    switch (type) {
    case ShaderType::vertex:
        return create<Shader>(GL_VERTEX_SHADER);
    case ShaderType::fragment:
    default:
        return create<Shader>(GL_FRAGMENT_SHADER);
    }
}

auto shader_source(Shader& shader, std::string_view src) -> void
{
    GLint const length = static_cast<GLint>(src.size());
    char const* data = src.data();
    gl().glShaderSource(shader.name(), 1, &data, &length);
    SC_CHECK_GL_ERROR("glShaderSource");
}

auto compile_shader(Shader& shader) -> void
{
    gl().glCompileShader(shader.name());
    SC_CHECK_GL_ERROR("glCompileShader");

    GLint compile_status;
    gl().glGetShaderiv(shader.name(), GL_COMPILE_STATUS, &compile_status);
    SC_CHECK_GL_ERROR("glGetShaderiv");

    if (compile_status != GL_TRUE) {
        GLsizei compile_log_length;
        gl().glGetShaderInfoLog(shader.name(),
                                shader_compile_info.size(),
                                &compile_log_length,
                                shader_compile_info.data());
        SC_CHECK_GL_ERROR("glGetShaderInfoLog");

        sc::throw_gl_error(
            "Shader compilation failed - " +
            std::string(shader_compile_info.data(), compile_log_length));
    }
}

} // namespace opengl

} // namespace sc
