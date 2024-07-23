#include "gl/error.hpp"
#include <GL/gl.h>
#include <stdexcept>
#include <string>
namespace sc
{
auto gl_error_to_string(GLenum glerror) noexcept -> char const*
{
#define SC_GL_ERROR_STR(val)                                                   \
    case val:                                                                  \
        return "" #val

    switch (glerror) {
        SC_GL_ERROR_STR(GL_INVALID_VALUE);
        SC_GL_ERROR_STR(GL_INVALID_OPERATION);
        SC_GL_ERROR_STR(GL_INVALID_ENUM);
    default:
        return "GL_UNKNOWN";
    }

#undef SC_GL_ERROR_STR
}
auto throw_gl_error(std::string const& msg, std::source_location loc) -> void
{
    throw std::runtime_error { msg + " " + loc.file_name() + ":" +
                               std::to_string(loc.line()) };
}

namespace opengl
{
auto framebuffer_status_to_string(GLenum glerror) noexcept -> char const*
{
#define SC_GL_FRAMEBUFFER_STATUS_STR(val)                                      \
    case val:                                                                  \
        return "" #val

    switch (glerror) {
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_UNDEFINED);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        SC_GL_FRAMEBUFFER_STATUS_STR(
            GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_UNSUPPORTED);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
        SC_GL_FRAMEBUFFER_STATUS_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);
    default:
        return "GL_UNKNOWN";
    }

#undef SC_GL_FRAMEBUFFER_STATUS_STR
}
} // namespace opengl

} // namespace sc
