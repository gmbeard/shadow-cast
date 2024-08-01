#include "gl/program.hpp"
#include "gl/error.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include <GL/glext.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace
{
thread_local std::array<char, 1024> program_info_log {};

auto detach_shaders(sc::opengl::Program& program) -> void
{
    GLsizei num;
    std::array<GLuint, 10> shaders {};

    sc::gl().glGetAttachedShaders(
        program.name(),
        std::min(program.num_shaders_attached, shaders.size()),
        &num,
        &shaders[0]);
    SC_CHECK_GL_ERROR("glGetAttachedShaders");

    for (auto shader :
         std::span { shaders.data(), static_cast<std::size_t>(num) }) {
        sc::gl().glDetachShader(program.name(), shader);
        SC_CHECK_GL_ERROR("glDetachShader");
    }
}

} // namespace

namespace sc
{
namespace opengl
{
auto ProgramTraits::create() const -> GLuint
{
    auto const name = gl().glCreateProgram();
    SC_CHECK_GL_ERROR("glCreateProgram");
    return name;
}

auto ProgramTraits::destroy(GLuint name) const noexcept -> void
{
    gl().glDeleteProgram(name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glDeleteProgram");
}

auto ProgramTraits::bind(GLenum /* target_not_used */,
                         GLuint name) const noexcept -> void
{
    gl().glUseProgram(name);
    SC_CHECK_GL_ERROR_NOEXCEPT("glUseProgram");
}

auto attach_shader(Program& program, Shader const& shader) -> void
{
    gl().glAttachShader(program.name(), shader.name());
    SC_CHECK_GL_ERROR("glAttachShader");
}

auto link_program(Program& program) -> void
{
    gl().glLinkProgram(program.name());
    SC_CHECK_GL_ERROR("glLinkProgram");

    GLint link_status;
    gl().glGetProgramiv(program.name(), GL_LINK_STATUS, &link_status);
    SC_CHECK_GL_ERROR("glGetProgramiv GL_LINK_STATUS");

    GLsizei log_length;
    if (link_status != GL_TRUE) {
        gl().glGetProgramInfoLog(program.name(),
                                 program_info_log.size(),
                                 &log_length,
                                 program_info_log.data());
        SC_CHECK_GL_ERROR("glGetProgramInfoLog");

        sc::throw_gl_error("failed linking program - " +
                           std::string(program_info_log.data(), log_length));
    }

    detach_shaders(program);
    program.num_shaders_attached = 0;
}

auto validate_program(Program& program) -> void
{
    gl().glValidateProgram(program.name());
    SC_CHECK_GL_ERROR("glValidateProgram");

    GLint validation_status;
    gl().glGetProgramiv(program.name(), GL_VALIDATE_STATUS, &validation_status);
    SC_CHECK_GL_ERROR("glGetProgramiv GL_VALIDATE_STATUS");

    if (validation_status != GL_TRUE) {

        GLsizei log_length;
        gl().glGetProgramInfoLog(program.name(),
                                 program_info_log.size(),
                                 &log_length,
                                 program_info_log.data());
        SC_CHECK_GL_ERROR("glGetProgramInfoLog");

        sc::throw_gl_error("failed validation - " +
                           std::string(program_info_log.data(), log_length));
    }
}

auto bind_attrib_location(Program& program,
                          GLuint index,
                          std::string const& name) -> void
{
    static_cast<void>(program);
    static_cast<void>(index);
    static_cast<void>(name);
    contract_check_failed("Not implemented");
}

auto get_uniform_location(Program const& program, std::string const& name)
    -> GLint
{
    auto const location =
        gl().glGetUniformLocation(program.name(), name.c_str());
    SC_CHECK_GL_ERROR("glGetUniformLocation");

    return location;
}

auto uniform_matrix(BoundProgram& bound_program,
                    GLint location,
                    GLboolean transpose,
                    std::span<float const> values) -> void
{
    // TODO:
    SC_CHECK_GL_BINDING(bound_program);
    static_cast<void>(location);
    static_cast<void>(transpose);
    static_cast<void>(values);
    contract_check_failed("Not implemented");
}

auto uniform_matrix(BoundProgram& program,
                    std::string const& name,
                    GLboolean transpose,
                    std::span<float const> values) -> void
{
    uniform_matrix(program,
                   get_uniform_location(program.object(), name),
                   transpose,
                   values);
}

} // namespace opengl

} // namespace sc
