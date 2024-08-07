#ifndef SHADOW_CAST_PLATFORM_OPENGL_HPP_INCLUDED
#define SHADOW_CAST_PLATFORM_OPENGL_HPP_INCLUDED

#include "utils/contracts.hpp"
#include <GL/gl.h>

#define GL_TEXTURE_EXTERNAL_OES 0x8D65

#define SAFE_EMPTY_FUNCTION_POINTER(name)                                      \
    [](auto...) {                                                              \
        ::sc::contract_check_failed("GL function not attached: " #name);       \
    }

namespace sc
{
struct OpenGL
{
    GLuint (*glCreateShader)(GLenum shader_type);
    GLuint (*glDeleteShader)(GLuint shader_id);
    void (*glShaderSource)(GLuint shader,
                           GLsizei count,
                           const GLchar** string,
                           const GLint* length);
    GLenum (*glGetError)(void);
    void (*glCompileShader)(GLuint shader);
    void (*glGetShaderInfoLog)(GLuint shader,
                               GLsizei maxLength,
                               GLsizei* length,
                               GLchar* infoLog);
    void (*glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
    GLuint (*glCreateProgram)(void);
    void (*glAttachShader)(GLuint program, GLuint shader);
    void (*glGetAttachedShaders)(GLuint program,
                                 GLsizei maxCount,
                                 GLsizei* count,
                                 GLuint* shaders);
    void (*glLinkProgram)(GLuint program);
    void (*glDeleteProgram)(GLuint program);
    void (*glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
    void (*glGetProgramInfoLog)(GLuint program,
                                GLsizei maxLength,
                                GLsizei* length,
                                GLchar* infoLog);
    void (*glValidateProgram)(GLuint program);
    void (*glDetachShader)(GLuint program, GLuint shader);
    GLint (*glGetUniformLocation)(GLuint program, const GLchar* name);
    void (*glEnableVertexArrayAttrib)(GLuint vaobj, GLuint index);
    void (*glDisableVertexArrayAttrib)(GLuint vaobj, GLuint index);
    void (*glBindVertexArray)(GLuint array);
    void (*glGenBuffers)(GLsizei n, GLuint* buffers);
    void (*glDeleteBuffers)(GLsizei n, const GLuint* buffers);
    void (*glBindBuffer)(GLenum target, GLuint buffer);
    void (*glBufferData)(GLenum target,
                         GLsizeiptr size,
                         const void* data,
                         GLenum usage);
    void (*glNamedBufferData)(GLuint buffer,
                              GLsizeiptr size,
                              const void* data,
                              GLenum usage);
    void (*glGenVertexArrays)(GLsizei n, GLuint* arrays);
    void (*glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
    void (*glGenTextures)(GLsizei n, GLuint* textures);
    void (*glDeleteTextures)(GLsizei n, const GLuint* textures);
    void (*glBindTexture)(GLenum target, GLuint texture);
    void (*glTexImage2D)(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const void* data);
    void (*glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
    void (*glTexParameteri)(GLenum target, GLenum pname, GLint param);
    void (*glTexParameterfv)(GLenum target,
                             GLenum pname,
                             const GLfloat* params);
    void (*glTexParameteriv)(GLenum target, GLenum pname, const GLint* params);
    void (*glTexParameterIiv)(GLenum target, GLenum pname, const GLint* params);
    void (*glTexParameterIuiv)(GLenum target,
                               GLenum pname,
                               const GLuint* params);
    void (*glVertexAttribPointer)(GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLboolean normalized,
                                  GLsizei stride,
                                  const GLvoid* pointer);
    void (*glUseProgram)(GLuint program);
    void (*glDrawArrays)(GLenum mode, GLint first, GLsizei count);
    void (*glFramebufferTexture)(GLenum target,
                                 GLenum attachment,
                                 GLuint texture,
                                 GLint level);
    GLenum (*glCheckFramebufferStatus)(GLenum target);
    void (*glGenFramebuffers)(GLsizei n, GLuint* ids);
    void (*glDeleteFramebuffers)(GLsizei n, GLuint* framebuffers);
    void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
    void (*glDrawBuffers)(GLsizei n, const GLenum* bufs);
    void (*glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glGetTexImage)(
        GLenum target, GLint level, GLenum format, GLenum type, GLvoid* pixels);
    void (*glGetTexLevelParameterfv)(GLenum target,
                                     GLint level,
                                     GLenum pname,
                                     GLfloat* params);
    void (*glGetTexLevelParameteriv)(GLenum target,
                                     GLint level,
                                     GLenum pname,
                                     GLint* params);
    void (*glClear)(GLbitfield mask);
    void (*glClearColor)(GLfloat red,
                         GLfloat green,
                         GLfloat blue,
                         GLfloat alpha);
    void (*glDrawElements)(GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const void* indices);
    void (*glUniform1fv)(GLint location, GLsizei count, const GLfloat* value);
    void (*glUniform2fv)(GLint location, GLsizei count, const GLfloat* value);
    void (*glUniform3fv)(GLint location, GLsizei count, const GLfloat* value);
    void (*glUniform4fv)(GLint location, GLsizei count, const GLfloat* value);
    void (*glEnable)(GLenum cap);
    void (*glBlendFunc)(GLenum sfactor, GLenum dfactor);
    const GLubyte* (*glGetString)(GLenum name);

    /* NOTE:
     *  Extensions...
     */
    void (*glEGLImageTargetTexture2DOES)(GLenum target, void* image) =
        SAFE_EMPTY_FUNCTION_POINTER(glEGLImageTargetTexture2DOES);
};

namespace detail
{
auto get_opengl_module() -> OpenGL&;
}

auto gl() -> OpenGL const&;

} // namespace sc
#endif // SHADOW_CAST_PLATFORM_OPENGL_HPP_INCLUDED
