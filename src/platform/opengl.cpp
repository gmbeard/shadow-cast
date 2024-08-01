#include "platform/opengl.hpp"
#include "error.hpp"
#include "utils/symbol.hpp"
#include <dlfcn.h>

namespace sc
{
auto load_opengl() -> OpenGL
{
    auto lib = dlopen("libGL.so.1", RTLD_LAZY);
    if (!lib)
        throw ModuleError { std::string { "Couldn't load libGL.so.1: " } +
                            dlerror() };

    OpenGL opengl {};
    TRY_ATTACH_SYMBOL(&opengl.glCreateShader, "glCreateShader", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDeleteShader, "glDeleteShader", lib);
    TRY_ATTACH_SYMBOL(&opengl.glShaderSource, "glShaderSource", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetError, "glGetError", lib);
    TRY_ATTACH_SYMBOL(&opengl.glCompileShader, "glCompileShader", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetShaderInfoLog, "glGetShaderInfoLog", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetShaderiv, "glGetShaderiv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glCreateProgram, "glCreateProgram", lib);
    TRY_ATTACH_SYMBOL(&opengl.glAttachShader, "glAttachShader", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glGetAttachedShaders, "glGetAttachedShaders", lib);
    TRY_ATTACH_SYMBOL(&opengl.glLinkProgram, "glLinkProgram", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDeleteProgram, "glDeleteProgram", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetProgramiv, "glGetProgramiv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetProgramInfoLog, "glGetProgramInfoLog", lib);
    TRY_ATTACH_SYMBOL(&opengl.glValidateProgram, "glValidateProgram", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDetachShader, "glDetachShader", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glGetUniformLocation, "glGetUniformLocation", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glEnableVertexArrayAttrib, "glEnableVertexArrayAttrib", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glDisableVertexArrayAttrib, "glDisableVertexArrayAttrib", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGenVertexArrays, "glGenVertexArrays", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glDeleteVertexArrays, "glDeleteVertexArrays", lib);
    TRY_ATTACH_SYMBOL(&opengl.glBindVertexArray, "glBindVertexArray", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGenBuffers, "glGenBuffers", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDeleteBuffers, "glDeleteBuffers", lib);
    TRY_ATTACH_SYMBOL(&opengl.glBindBuffer, "glBindBuffer", lib);
    TRY_ATTACH_SYMBOL(&opengl.glBufferData, "glBufferData", lib);
    TRY_ATTACH_SYMBOL(&opengl.glNamedBufferData, "glNamedBufferData", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGenTextures, "glGenTextures", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDeleteTextures, "glDeleteTextures", lib);
    TRY_ATTACH_SYMBOL(&opengl.glBindTexture, "glBindTexture", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexImage2D, "glTexImage2D", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameterf, "glTexParameterf", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameteri, "glTexParameteri", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameterfv, "glTexParameterfv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameteriv, "glTexParameteriv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameterIiv, "glTexParameterIiv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glTexParameterIuiv, "glTexParameterIuiv", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glVertexAttribPointer, "glVertexAttribPointer", lib);
    TRY_ATTACH_SYMBOL(&opengl.glUseProgram, "glUseProgram", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDrawArrays, "glDrawArrays", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glFramebufferTexture, "glFramebufferTexture", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glCheckFramebufferStatus, "glCheckFramebufferStatus", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGenFramebuffers, "glGenFramebuffers", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glDeleteFramebuffers, "glDeleteFramebuffers", lib);
    TRY_ATTACH_SYMBOL(&opengl.glBindFramebuffer, "glBindFramebuffer", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDrawBuffers, "glDrawBuffers", lib);
    TRY_ATTACH_SYMBOL(&opengl.glViewport, "glViewport", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetTexImage, "glGetTexImage", lib);
    TRY_ATTACH_SYMBOL(&opengl.glGetTexLevelParameterfv, "glGetTexImage", lib);
    TRY_ATTACH_SYMBOL(
        &opengl.glGetTexLevelParameteriv, "glGetTexLevelParameteriv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glClear, "glClear", lib);
    TRY_ATTACH_SYMBOL(&opengl.glClearColor, "glClearColor", lib);
    TRY_ATTACH_SYMBOL(&opengl.glDrawElements, "glDrawElements", lib);
    TRY_ATTACH_SYMBOL(&opengl.glUniform1fv, "glUniform1fv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glUniform2fv, "glUniform2fv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glUniform3fv, "glUniform3fv", lib);
    TRY_ATTACH_SYMBOL(&opengl.glUniform4fv, "glUniform4fv", lib);

    return opengl;
}

namespace detail
{

auto get_opengl_module() -> OpenGL&
{
    static OpenGL opengl = load_opengl();
    return opengl;
}

} // namespace detail

auto gl() -> OpenGL const& { return detail::get_opengl_module(); }
} // namespace sc
