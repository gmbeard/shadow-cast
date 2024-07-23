#ifndef SHADOW_CAST_GL_ERROR_HPP_INCLUDED
#define SHADOW_CAST_GL_ERROR_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <GL/gl.h>
#include <source_location>
#include <string>

#define SC_CHECK_GL_ERROR(src)                                                 \
    if (auto const glerror = ::sc::gl().glGetError(); glerror != GL_NO_ERROR)  \
    ::sc::throw_gl_error(::std::string { src " failed: " } +                   \
                         ::sc::gl_error_to_string(glerror))

#define SC_CHECK_GL_ERROR_NOEXCEPT(src)                                        \
    if (auto const glerror = ::sc::gl().glGetError(); glerror != GL_NO_ERROR)  \
    ::sc::contract_check_failed(src)

#define SC_THROW_IF_GL_ERROR SC_CHECK_GL_ERROR
#define SC_ABORT_IF_GL_ERROR SC_CHECK_GL_ERROR_NOEXCEPT

#if defined(SHADOW_CAST_STRICT_GL_BINDING_CHECK)
#define SC_CHECK_GL_BINDING(binding)                                           \
    if (auto&& bound = binding.is_bound(); !bound)                             \
    ::sc::contract_check_failed("GL object is not bound")
#else
#define SC_CHECK_GL_BINDING(binding) static_cast<void>(binding)
#endif

namespace sc
{
[[noreturn]] auto
throw_gl_error(std::string const& msg,
               std::source_location = std::source_location::current()) -> void;
auto gl_error_to_string(GLenum) noexcept -> char const*;

namespace opengl
{
auto framebuffer_status_to_string(GLenum) noexcept -> char const*;
}
} // namespace sc

#endif // SHADOW_CAST_GL_ERROR_HPP_INCLUDED
