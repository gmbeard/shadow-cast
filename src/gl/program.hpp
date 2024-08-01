#ifndef SHADOW_CAST_GL_PROGRAM_HPP_INCLUDED
#define SHADOW_CAST_GL_PROGRAM_HPP_INCLUDED

#include "gl/error.hpp"
#include "gl/object.hpp"
#include "gl/shader.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include <GL/gl.h>
#include <cstddef>
#include <span>
#include <string>
#include <type_traits>

namespace sc
{

namespace opengl
{
struct ProgramCategory
{
};
struct ProgramTraits
{
    using category = ProgramCategory;
    auto create() const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
    auto bind(GLenum, GLuint name) const noexcept -> void;
};

struct Program : ObjectBase<ProgramTraits>
{
    using ObjectBase<ProgramTraits>::ObjectBase;

    std::size_t num_shaders_attached { 0 };
};

using ProgramTarget = TargetBase<0, ProgramTraits>;
using BoundProgram = BoundTarget<ProgramTarget, Program>;
constexpr ProgramTarget program_target {};

auto attach_shader(Program& program, Shader const& shader) -> void;
auto link_program(Program& program) -> void;
auto validate_program(Program& program) -> void;
auto bind_attrib_location(Program& program,
                          GLuint index,
                          std::string const& name) -> void;
auto get_uniform_location(Program const& program, std::string const& name)
    -> GLint;

template <typename T, typename... Ts>
auto uniform(BoundProgram& bound_program,
             GLint location,
             T&& param,
             Ts&&... rest) -> void
requires(
    (sizeof...(rest) <= 3) &&
    std::is_arithmetic_v<std::remove_reference_t<T>> &&
    (sizeof...(rest) == 0 ||
     (std::is_same_v<std::remove_reference_t<T>, std::remove_reference_t<Ts>> &&
      ...)))
{
    SC_CHECK_GL_BINDING(bound_program);
    std::array<std::decay_t<T>, 1 + sizeof...(rest)> const params { param,
                                                                    rest... };

    if constexpr (std::is_convertible_v<std::decay_t<T>, GLfloat>) {
        if constexpr (params.size() == 1) {
            gl().glUniform1fv(location, 1, params.data());
            SC_CHECK_GL_ERROR("glUniform1fv");
        }
        else if constexpr (params.size() == 2) {
            gl().glUniform2fv(location, 1, params.data());
            SC_CHECK_GL_ERROR("glUniform2fv");
        }
        else if constexpr (params.size() == 3) {
            gl().glUniform3fv(location, 1, params.data());
            SC_CHECK_GL_ERROR("glUniform3fv");
        }
        else {
            gl().glUniform4fv(location, 1, params.data());
            SC_CHECK_GL_ERROR("glUniform4fv");
        }
    }
    else {
        contract_check_failed("Not implemented");
    }
}

template <typename T, typename... Ts>
auto uniform(BoundProgram& program,
             std::string const& name,
             T&& param,
             Ts&&... rest) -> void
{
    uniform(program,
            get_uniform_location(program.object(), name),
            std::forward<T>(param),
            std::forward<Ts>(rest)...);
}

template <typename T>
auto uniform(BoundProgram& bound_program,
             GLint location,
             std::span<T const> value) -> void
{
    // TODO:
    SC_CHECK_GL_BINDING(bound_program);
    static_cast<void>(location);
    static_cast<void>(value);
    contract_check_failed("Not implemented");
}

template <typename T>
auto uniform(BoundProgram& program,
             std::string const& name,
             std::span<T const> value) -> void
{
    uniform(program, get_uniform_location(program.object(), name), value);
}

auto uniform_matrix(BoundProgram& bound_program,
                    GLint location,
                    GLboolean transpose,
                    std::span<float const> values) -> void;

auto uniform_matrix(BoundProgram& program,
                    std::string const& name,
                    GLboolean transpose,
                    std::span<float const> values) -> void;

} // namespace opengl

} // namespace sc
#endif // SHADOW_CAST_GL_PROGRAM_HPP_INCLUDED
