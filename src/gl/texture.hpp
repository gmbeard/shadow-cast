#ifndef SHADOW_CAST_GL_TEXTURE_HPP_INCLUDED
#define SHADOW_CAST_GL_TEXTURE_HPP_INCLUDED

#include "gl/error.hpp"
#include "gl/object.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include <GL/gl.h>
#include <span>
#include <type_traits>

namespace sc
{
namespace opengl
{

template <typename T>
concept GlScalarValue = std::is_same_v<std::decay_t<T>, GLfloat> ||
                        std::is_same_v<std::decay_t<T>, GLint>;

template <typename T>
concept GlVectorValue =
    GlScalarValue<T> || std::is_same_v<std::decay_t<T>, GLuint>;

struct TextureCategory
{
};

struct TextureTraits
{
    using category = TextureCategory;

    auto bind(GLenum target, GLuint name) const noexcept -> void;
    auto create() const -> GLuint;
    auto destroy(GLuint name) const noexcept -> void;
};

using Texture = ObjectBase<TextureTraits>;
template <GLenum T>
using TextureTarget = TargetBase<T, TextureTraits>;

template <typename T>
concept BoundTextureTargetConcept =
    std::is_convertible_v<typename T::category, TextureCategory>;

template <typename T>
concept TextureConcept =
    std::is_convertible_v<typename T::category, TextureCategory>;

template <BoundTextureTargetConcept T>
auto texture_image_2d(T& binding,
                      GLint level,
                      GLint internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLint border,
                      GLenum format,
                      GLenum type,
                      const void* data) -> void
{
    SC_CHECK_GL_BINDING(binding);
    gl().glTexImage2D(binding.target(),
                      level,
                      internal_format,
                      width,
                      height,
                      border,
                      format,
                      type,
                      data);
    SC_THROW_IF_GL_ERROR("glTexImage2D");
}

template <BoundTextureTargetConcept B, GlVectorValue T>
auto texture_parameter(B& binding, GLenum param_name, std::span<T> vector_value)
    -> void
{
    SC_CHECK_GL_BINDING(binding);
    SC_EXPECT(vector_value.size() > 0);

    if ((param_name == GL_TEXTURE_BORDER_COLOR ||
         param_name == GL_TEXTURE_SWIZZLE_RGBA)) {
        SC_EXPECT(vector_value.size() == 4);
    }

    using T_ = std::decay_t<T>;
    if constexpr (std::is_same_v<T_, GLfloat>) {
        gl().glTexParameterfv(
            binding.target(), param_name, vector_value.data());
    }
    else if constexpr (std::is_same_v<T_, GLint>) {
        gl().glTexParameteriv(
            binding.target(), param_name, vector_value.data());
    }
    else {
        gl().glTexParameterIuiv(
            binding.target(), param_name, vector_value.data());
    }

    SC_THROW_IF_GL_ERROR("glTexParameterTv");
}

template <BoundTextureTargetConcept B, GlScalarValue T>
auto texture_parameter(B& binding, GLenum param_name, T scalar_value) -> void
{
    SC_CHECK_GL_BINDING(binding);
    using T_ = std::decay_t<T>;
    if constexpr (std::is_same_v<T_, GLfloat>) {
        gl().glTexParameterf(binding.target(), param_name, scalar_value);
    }
    else {
        gl().glTexParameteri(binding.target(), param_name, scalar_value);
    }

    SC_THROW_IF_GL_ERROR("glTexParameterT");
}

template <GlScalarValue T, BoundTextureTargetConcept B>
auto get_texture_level_parameter(B& binding, GLint level, GLenum param_name)
    -> T
{
    switch (param_name) {
    case GL_TEXTURE_WIDTH:
    case GL_TEXTURE_HEIGHT:
        break;
    default:
        contract_check_failed("param_name not supported");
    }

    SC_CHECK_GL_BINDING(binding);
    using T_ = std::decay_t<T>;
    T_ val {};
    if constexpr (std::is_same_v<T_, GLfloat>) {
        gl().glGetTexLevelParameterfv(
            binding.target(), level, param_name, &val);
    }
    else {
        gl().glGetTexLevelParameteriv(
            binding.target(), level, param_name, &val);
    }

    SC_THROW_IF_GL_ERROR("glTexParameterT");

    return val;
}

template <typename From>
struct TypeToGLenum : std::false_type
{
};

template <typename To>
struct GLenumToType : std::false_type
{
};

#define TYPE_TO_GLENUM(From, To)                                               \
    template <>                                                                \
    struct TypeToGLenum<From> : std::true_type                                 \
    {                                                                          \
        static constexpr GLenum enum_value = To;                               \
    }

#define GLENUM_TO_TYPE(From, To)                                               \
    template <>                                                                \
    struct GLenumToType<To> : std::true_type                                   \
    {                                                                          \
        using to_type = To;                                                    \
    }

TYPE_TO_GLENUM(unsigned char, GL_UNSIGNED_BYTE);
TYPE_TO_GLENUM(char, GL_BYTE);
TYPE_TO_GLENUM(float, GL_FLOAT);

GLENUM_TO_TYPE(GL_UNSIGNED_BYTE, unsigned char);
GLENUM_TO_TYPE(GL_FLOAT, float);
GLENUM_TO_TYPE(GL_BYTE, char);

template <BoundTextureTargetConcept B, typename T>
auto get_texture_image(B const& binding,
                       GLint level,
                       GLenum format,
                       std::span<T> pixels) -> void
requires(TypeToGLenum<std::decay_t<T>>::value)
{
    SC_CHECK_GL_BINDING(binding);

    using T_ = std::decay_t<T>;

    auto const width =
        get_texture_level_parameter<GLint>(binding, level, GL_TEXTURE_WIDTH);
    auto const height =
        get_texture_level_parameter<GLint>(binding, level, GL_TEXTURE_HEIGHT);

    auto const format_to_component_length =
        [](GLenum fmt) noexcept -> std::size_t {
        switch (fmt) {
        case GL_RGB:
        case GL_BGR:
            return 3;
        case GL_RGBA:
        case GL_BGRA:
            return 4;
        default:
            contract_check_failed("Unsupported format");
        }
    };

    SC_EXPECT(pixels.size() >= static_cast<std::size_t>(width * height) *
                                   sizeof(T_) *
                                   format_to_component_length(format));

    gl().glGetTexImage(binding.target(),
                       level,
                       format,
                       TypeToGLenum<T_>::enum_value,
                       pixels.data());
    SC_CHECK_GL_ERROR("glGetTexImage");
}

constexpr TextureTarget<GL_TEXTURE_2D> texture_2d_target {};

} // namespace opengl
} // namespace sc

#endif // SHADOW_CAST_GL_TEXTURE_HPP_INCLUDED
