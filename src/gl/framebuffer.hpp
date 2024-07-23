#ifndef SHADOW_CAST_GL_FRAMEBUFFER_HPP_INCLUDED
#define SHADOW_CAST_GL_FRAMEBUFFER_HPP_INCLUDED

#include "gl/error.hpp"
#include "gl/object.hpp"
#include "gl/texture.hpp"
#include "platform/opengl.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
namespace sc::opengl
{
struct FramebufferCategory
{
};

struct FramebufferTraits
{
    using category = FramebufferCategory;

    auto create() const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
    auto bind(GLenum target, GLuint name) const noexcept -> void;
};

using Framebuffer = ObjectBase<FramebufferTraits>;
template <GLenum T>
using FramebufferTarget = TargetBase<T, FramebufferTraits>;

template <typename T>
concept BoundFramebufferConcept =
    BindingConcept<T> &&
    std::is_convertible_v<typename T::category, FramebufferCategory>;

template <BoundFramebufferConcept T>
auto framebuffer_texture(T const& bound_target,
                         GLenum attachment,
                         Texture const& texture,
                         GLint level) -> void
{
    SC_CHECK_GL_BINDING(bound_target);
    gl().glFramebufferTexture(
        bound_target.target(), attachment, texture.name(), level);
    SC_CHECK_GL_ERROR("glFramebufferTexture");
}

template <BoundFramebufferConcept T>
auto get_framebuffer_status(T const& bound_target) -> GLenum
{
    SC_CHECK_GL_BINDING(bound_target);
    auto const val = gl().glCheckFramebufferStatus(bound_target.target());
    SC_CHECK_GL_ERROR("glCheckFramebufferStatus");

    return val;
}

template <BoundFramebufferConcept T>
auto check_framebuffer_status(T const& bound_target) -> void
{
    auto const val = get_framebuffer_status(bound_target);
    if (val != GL_FRAMEBUFFER_COMPLETE)
        throw_gl_error(framebuffer_status_to_string(val));
}

template <BoundFramebufferConcept T>
auto draw_buffers(T const& bound_target, std::span<GLenum const> bufs) -> void
{
    SC_CHECK_GL_BINDING(bound_target);
    gl().glDrawBuffers(bufs.size(), bufs.data());
    SC_CHECK_GL_ERROR("glDrawBuffers");
}

template <BoundFramebufferConcept T, typename... Ts>
auto draw_buffers(T const& bound_target, GLenum arg, Ts... rest) -> void
requires((sizeof...(rest) == 0) || (std::is_same_v<Ts, GLenum> && ...))
{
    if constexpr (sizeof...(rest) == 0) {
        draw_buffers(bound_target, std::span<GLenum> { &arg, 1 });
    }
    else {
        std::array<GLenum, 1 + sizeof...(rest)> const bufs = { arg, rest... };
        draw_buffers(bound_target,
                     std::span<GLenum const> { bufs.data(), bufs.size() });
    }
}

constexpr FramebufferTarget<GL_DRAW_FRAMEBUFFER> draw_framebuffer_target {};
constexpr FramebufferTarget<GL_READ_FRAMEBUFFER> read_framebuffer_target {};
} // namespace sc::opengl

#endif // SHADOW_CAST_GL_FRAMEBUFFER_HPP_INCLUDED
