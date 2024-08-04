#include "services/color_converter.hpp"
#include "gl/core.hpp"
#include "gl/program.hpp"
#include "gl/texture.hpp"
#include "gl/vertex_array_object.hpp"
#include "platform/opengl.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <GL/gl.h>
#include <optional>
#include <string_view>

#define SHADER_SOURCE(shader_symbol)                                           \
    [] {                                                                       \
        auto&& first = _binary_##shader_symbol##_glsl_start;                   \
        auto&& last = _binary_##shader_symbol##_glsl_end;                      \
        return get_shader_source(first, last);                                 \
    }()

/* NOTE:
 * These symbols are used to access the embedded GLSL sources. see
 * `cmake/EmbeddedGLSLTarget.cmake` for details on how this is done. It appears
 * to matter that they are declared as unsized arrays instead of `char
 * const*`s...
 */
extern char const _binary_default_vertex_glsl_start[];
extern char const _binary_default_vertex_glsl_end[];
extern char const _binary_default_fragment_glsl_start[];
extern char const _binary_default_fragment_glsl_end[];
extern char const _binary_mouse_vertex_glsl_start[];
extern char const _binary_mouse_vertex_glsl_end[];
extern char const _binary_mouse_fragment_glsl_start[];
extern char const _binary_mouse_fragment_glsl_end[];

namespace
{

auto get_shader_source(char const* first, char const* last) noexcept
    -> std::string_view
{
    return std::string_view { first, static_cast<std::size_t>(last - first) };
}

} // namespace

namespace sc
{
ColorConverter::ColorConverter(std::uint32_t output_width,
                               std::uint32_t output_height) noexcept
    : output_width_ { output_width }
    , output_height_ { output_height }
{
}

auto ColorConverter::initialize() -> void
{
    if (initialized_)
        return;

    auto vao = opengl::create<opengl::VertexArray>();
    auto vertex_buffer = opengl::create<opengl::Buffer>();
    auto texture_coords_buffer = opengl::create<opengl::Buffer>();
    auto index_buffer = opengl::create<opengl::Buffer>();

    opengl::bind(opengl::vertex_array_target, vao, [&](auto vao_binding) {
        // clang-format off
        constexpr std::array<float, 12> const vertices = {
            1.0f, -1.0f, 0.0f, /* Bottom right */
            -1.0f, -1.0f, 0.0f, /* Bottom left */
            -1.0f,  1.0f, 0.0f, /* Top left */
            1.0f, 1.0f, 0.0f, /* Top right */
        };

        constexpr std::array<float, 8> const texture_coords = {
            1.0f, 0.0f,
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
        };

        constexpr std::array<std::uint32_t, 6> const indices = {
            0, 1, 2,
            0, 2, 3,
        };
        // clang-format on

        opengl::bind(
            opengl::array_buffer_target, vertex_buffer, [&](auto binding) {
                opengl::buffer_data(
                    binding,
                    std::span { vertices.data(), vertices.size() },
                    GL_STATIC_DRAW);

                opengl::vertex_attrib_pointer(
                    binding, 0, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);
                opengl::enable_vertex_array_attrib(vao_binding, 0);
            });

        opengl::bind(
            opengl::array_buffer_target,
            texture_coords_buffer,
            [&](auto binding) {
                opengl::buffer_data(
                    binding,
                    std::span { texture_coords.data(), texture_coords.size() },
                    GL_STATIC_DRAW);

                opengl::vertex_attrib_pointer(
                    binding, 1, 2, GL_FLOAT, false, 2 * sizeof(float), nullptr);

                opengl::enable_vertex_array_attrib(vao_binding, 1);
            });

        opengl::bind(opengl::element_array_buffer_target,
                     index_buffer,
                     [&](auto binding) {
                         opengl::buffer_data(
                             binding,
                             std::span { indices.data(), indices.size() },
                             GL_STATIC_DRAW);
                     });
    });

    auto output_texture = opengl::create<opengl::Texture>();

    opengl::bind(opengl::texture_2d_target, output_texture, [&](auto binding) {
        opengl::texture_image_2d(binding,
                                 0,
                                 GL_RGBA,
                                 output_width_,
                                 output_height_,
                                 0,
                                 GL_BGRA,
                                 GL_UNSIGNED_BYTE,
                                 0);

        opengl::texture_parameter(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        opengl::texture_parameter(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    });

    auto vertex_shader = opengl::create_shader(opengl::ShaderType::vertex);
    std::string_view const vertex_shader_source = SHADER_SOURCE(default_vertex);
    opengl::shader_source(vertex_shader, vertex_shader_source);
    opengl::compile_shader(vertex_shader);

    auto fragment_shader = opengl::create_shader(opengl::ShaderType::fragment);
    std::string_view const fragment_shader_source =
        SHADER_SOURCE(default_fragment);
    opengl::shader_source(fragment_shader, fragment_shader_source);
    opengl::compile_shader(fragment_shader);

    auto program = opengl::create<opengl::Program>();

    opengl::attach_shader(program, vertex_shader);
    opengl::attach_shader(program, fragment_shader);

    opengl::link_program(program);
    opengl::validate_program(program);

    auto mouse_vertex_shader =
        opengl::create_shader(opengl::ShaderType::vertex);
    std::string_view const mouse_vertex_shader_source =
        SHADER_SOURCE(mouse_vertex);
    opengl::shader_source(mouse_vertex_shader, mouse_vertex_shader_source);
    opengl::compile_shader(mouse_vertex_shader);

    auto mouse_fragment_shader =
        opengl::create_shader(opengl::ShaderType::fragment);
    std::string_view const mouse_fragment_shader_source =
        SHADER_SOURCE(mouse_fragment);
    opengl::shader_source(mouse_fragment_shader, mouse_fragment_shader_source);
    opengl::compile_shader(mouse_fragment_shader);

    auto mouse_program = opengl::create<opengl::Program>();
    opengl::attach_shader(mouse_program, mouse_vertex_shader);
    opengl::attach_shader(mouse_program, mouse_fragment_shader);

    opengl::link_program(mouse_program);
    opengl::validate_program(mouse_program);

    opengl::bind(
        opengl::program_target, mouse_program, [&](auto program_binding) {
            opengl::uniform(program_binding,
                            "screen_dimensions",
                            float(output_width_),
                            float(output_height_));

            mouse_dimensions_uniform_ =
                opengl::get_uniform_location(mouse_program, "mouse_dimensions");
            mouse_position_uniform_ =
                opengl::get_uniform_location(mouse_program, "mouse_position");
        });

    auto fbo = opengl::create<opengl::Framebuffer>();
    opengl::bind(opengl::draw_framebuffer_target, fbo, [&](auto binding) {
        opengl::enable(GL_BLEND);
        opengl::blend_function(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        opengl::framebuffer_texture(
            binding, GL_COLOR_ATTACHMENT0, output_texture, 0);

        opengl::draw_buffers(binding, GL_COLOR_ATTACHMENT0);

        opengl::check_framebuffer_status(binding);
    });

    fbo_ = std::move(fbo);
    input_texture_ = opengl::create<opengl::Texture>();
    mouse_texture_ = opengl::create<opengl::Texture>();
    output_texture_ = std::move(output_texture);
    vao_ = std::move(vao);
    vertex_buffer_ = std::move(vertex_buffer);
    texture_coords_buffer_ = std::move(texture_coords_buffer);
    index_buffer_ = std::move(index_buffer);
    program_ = std::move(program);
    mouse_program_ = std::move(mouse_program);

    initialized_ = true;
}

auto ColorConverter::input_texture() noexcept -> opengl::Texture&
{
    return input_texture_;
}

auto ColorConverter::mouse_texture() noexcept -> opengl::Texture&
{
    return mouse_texture_;
}

auto ColorConverter::output_texture() noexcept -> opengl::Texture&
{
    return output_texture_;
}

auto ColorConverter::convert(std::optional<MouseParameters> mouse_params)
    -> void
{
    opengl::bind(opengl::vertex_array_target, vao_, [&](auto vao_binding) {
        /* TODO:
         *  It would be nice to be able to call `bind(...)` with
         *  multiple targets so we don't have to do this...
         */
        auto fbo_binding = opengl::bind(opengl::draw_framebuffer_target, fbo_);
        auto element_buffer_binding =
            opengl::bind(opengl::element_array_buffer_target, index_buffer_);

        opengl::viewport(0, 0, output_width_, output_height_);
        opengl::clear_color(1.f, 0.f, 0.f, 1.f);
        opengl::clear(GL_COLOR_BUFFER_BIT);

        opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                     input_texture_,
                     [&](auto /*texture_binding*/) {
                         auto program_in_use =
                             opengl::bind(opengl::program_target, program_);
                         opengl::draw_elements(vao_binding,
                                               element_buffer_binding,
                                               program_in_use,
                                               GL_TRIANGLES,
                                               6,
                                               GL_UNSIGNED_INT,
                                               nullptr);
                     });

        if (mouse_params) {
            opengl::bind(opengl::TextureTarget<GL_TEXTURE_EXTERNAL_OES> {},
                         mouse_texture_,
                         [&](auto /*texture_binding*/) {
                             auto program_in_use = opengl::bind(
                                 opengl::program_target, mouse_program_);
                             opengl::uniform(program_in_use,
                                             mouse_dimensions_uniform_,
                                             float(mouse_params->width),
                                             float(mouse_params->height));
                             opengl::uniform(program_in_use,
                                             mouse_position_uniform_,
                                             float(mouse_params->x),
                                             float(mouse_params->y));
                             opengl::draw_elements(vao_binding,
                                                   element_buffer_binding,
                                                   program_in_use,
                                                   GL_TRIANGLES,
                                                   6,
                                                   GL_UNSIGNED_INT,
                                                   nullptr);
                         });
        }
    });
}

} // namespace sc
