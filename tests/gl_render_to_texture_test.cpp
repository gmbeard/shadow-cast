#include "gl/buffer.hpp"
#include "gl/core.hpp"
#include "gl/framebuffer.hpp"
#include "gl/program.hpp"
#include "gl/shader.hpp"
#include "gl/texture.hpp"
#include "gl/vertex_array_object.hpp"
#include "platform/wayland.hpp"
#include "testing.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace ogl = sc::opengl;

namespace
{
struct Dimensions
{
    GLint width;
    GLint height;
};
} // namespace

template <typename Iter>
auto write_to_file(std::filesystem::path const& path,
                   Dimensions const& dimensions,
                   bool swap_bytes,
                   Iter first,
                   Iter last) -> void
{
    std::string const file_dimensions =
        std::format("-{}x{}", dimensions.width, dimensions.height);

    std::ofstream file_stream {
        path.string() + file_dimensions + (swap_bytes ? ".bgra" : ".rgba"),
        std::ios::out | std::ios::binary | std::ios::trunc
    };

    for (; first != last; ++first) {
        EXPECT(file_stream.write(reinterpret_cast<char const*>(&*first),
                                 sizeof(*first)));
    }
}

auto read_pixels(std::filesystem::path path) -> std::vector<unsigned char>
{
    std::cerr << "PATH: " << path << '\n';
    std::ifstream file_stream { path };
    EXPECT(file_stream);

    std::vector<unsigned char> data;
    std::copy(std::istreambuf_iterator<char>(file_stream),
              std::istreambuf_iterator<char>(),
              std::back_inserter(data));

    return data;
}

extern char const _binary_flipped_y_vertex_glsl_start[];
extern char const _binary_flipped_y_vertex_glsl_end[];
extern char const _binary_identity_fragment_glsl_start[];
extern char const _binary_identity_fragment_glsl_end[];
extern char const _binary_textured_vertex_glsl_start[];
extern char const _binary_textured_vertex_glsl_end[];
extern char const _binary_textured_brightness_fragment_glsl_start[];
extern char const _binary_textured_brightness_fragment_glsl_end[];

auto get_shader_source(char const* first, char const* last) noexcept
    -> std::string_view
{
    return std::string_view { first, static_cast<std::size_t>(last - first) };
}

#define SHADER_SOURCE(shader_symbol)                                           \
    [] {                                                                       \
        auto&& first = _binary_##shader_symbol##_glsl_start;                   \
        auto&& last = _binary_##shader_symbol##_glsl_end;                      \
        return get_shader_source(first, last);                                 \
    }()

/* Adaptation of the "Hello Triangle" tutorial from
 * https://learnopengl.com/Getting-started/Hello-Triangle with the addition of
 * rendering to a texture
 */
auto render_triange_to_texture() -> void
{
    auto const vertex_shader_source = SHADER_SOURCE(flipped_y_vertex);
    auto const fragment_shader_source = SHADER_SOURCE(identity_fragment);

    constexpr Dimensions dimensions { .width = 800, .height = 800 };

    auto vao = ogl::create<ogl::VertexArray>();
    auto vertex_buffer = ogl::create<ogl::Buffer>();
    auto color_buffer = ogl::create<ogl::Buffer>();
    auto fbo = ogl::create<ogl::Framebuffer>();
    auto output_texture = ogl::create<ogl::Texture>();

    ogl::bind(ogl::vertex_array_target, vao, [&](auto vao_binding) {
        // clang-format off
        constexpr std::array<float, 9> const vertices = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            0.0f,  1.0f, 0.0f,
        };

        constexpr std::array<float, 9> const colors = {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
        };
        // clang-format on

        ogl::bind(ogl::array_buffer_target, vertex_buffer, [&](auto binding) {
            ogl::buffer_data(binding,
                             std::span { vertices.data(), vertices.size() },
                             GL_STATIC_DRAW);

            ogl::vertex_attrib_pointer(
                binding, 0, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);
            ogl::enable_vertex_array_attrib(vao_binding, 0);
        });

        ogl::bind(ogl::array_buffer_target, color_buffer, [&](auto binding) {
            ogl::buffer_data(binding,
                             std::span { colors.data(), colors.size() },
                             GL_STATIC_DRAW);

            ogl::vertex_attrib_pointer(
                binding, 1, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);

            ogl::enable_vertex_array_attrib(vao_binding, 1);
        });
    });

    ogl::bind(ogl::texture_2d_target, output_texture, [&](auto binding) {
        ogl::texture_image_2d(binding,
                              0,
                              GL_RGB,
                              dimensions.width,
                              dimensions.height,
                              0,
                              GL_RGB,
                              GL_UNSIGNED_BYTE,
                              0);

        ogl::texture_parameter(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ogl::texture_parameter(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    });

    auto vertex_shader = ogl::create_shader(ogl::ShaderType::vertex);
    ogl::shader_source(vertex_shader, vertex_shader_source);
    ogl::compile_shader(vertex_shader);

    auto fragment_shader = ogl::create_shader(ogl::ShaderType::fragment);
    ogl::shader_source(fragment_shader, fragment_shader_source);
    ogl::compile_shader(fragment_shader);

    auto shader_program = ogl::create<ogl::Program>();
    ogl::attach_shader(shader_program, vertex_shader);
    ogl::attach_shader(shader_program, fragment_shader);

    ogl::link_program(shader_program);
    ogl::validate_program(shader_program);

    ogl::bind(ogl::draw_framebuffer_target, fbo, [&](auto binding) {
        ogl::viewport(0, 0, dimensions.width, dimensions.height);
        ogl::framebuffer_texture(
            binding, GL_COLOR_ATTACHMENT0, output_texture, 0);

        ogl::draw_buffers(binding, GL_COLOR_ATTACHMENT0);

        ogl::check_framebuffer_status(binding);
    });

    ogl::bind(ogl::vertex_array_target, vao, [&](auto binding) {
        /* TODO:
         *  It would be nice to be able to call `bind(...)` with
         *  multiple targets so we don't have to do this...
         */
        auto _ = ogl::bind(ogl::draw_framebuffer_target, fbo);
        auto program_in_use = ogl::bind(ogl::program_target, shader_program);

        ogl::clear_color(0.f, 0.f, 0.f, 1.f);
        ogl::clear(GL_COLOR_BUFFER_BIT);
        ogl::draw_arrays(binding, program_in_use, GL_TRIANGLES, 0, 3);
    });

    std::vector<unsigned char> pixels(dimensions.width * dimensions.height * 4,
                                      0);

    ogl::bind(ogl::texture_2d_target, output_texture, [&](auto binding) {
        ogl::get_texture_image(
            binding,
            0,
            GL_RGBA,
            std::span<unsigned char> { pixels.data(), pixels.size() });
    });

    write_to_file("/tmp/render-to-texture",
                  dimensions,
                  false,
                  pixels.begin(),
                  pixels.end());
}

auto render_textured_quad_to_texture(std::string_view pixel_file_path) -> void
{
    std::string_view const vertex_shader_source =
        SHADER_SOURCE(textured_vertex);
    std::string_view const fragment_shader_source =
        SHADER_SOURCE(textured_brightness_fragment);

    constexpr Dimensions dimensions { .width = 1024, .height = 1024 };
    constexpr Dimensions input_image { .width = 640, .height = 640 };

    auto vao = ogl::create<ogl::VertexArray>();
    auto vertex_buffer = ogl::create<ogl::Buffer>();
    auto texture_coords_buffer = ogl::create<ogl::Buffer>();
    auto element_buffer = ogl::create<ogl::Buffer>();

    auto fbo = ogl::create<ogl::Framebuffer>();
    auto input_texture = ogl::create<ogl::Texture>();
    auto output_texture = ogl::create<ogl::Texture>();

    ogl::bind(ogl::vertex_array_target, vao, [&](auto vao_binding) {
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

        ogl::bind(ogl::array_buffer_target, vertex_buffer, [&](auto binding) {
            ogl::buffer_data(binding,
                             std::span { vertices.data(), vertices.size() },
                             GL_STATIC_DRAW);

            ogl::vertex_attrib_pointer(
                binding, 0, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);
            ogl::enable_vertex_array_attrib(vao_binding, 0);
        });

        ogl::bind(
            ogl::array_buffer_target, texture_coords_buffer, [&](auto binding) {
                ogl::buffer_data(
                    binding,
                    std::span { texture_coords.data(), texture_coords.size() },
                    GL_STATIC_DRAW);

                ogl::vertex_attrib_pointer(
                    binding, 1, 2, GL_FLOAT, false, 2 * sizeof(float), nullptr);

                ogl::enable_vertex_array_attrib(vao_binding, 1);
            });

        ogl::bind(ogl::element_array_buffer_target,
                  element_buffer,
                  [&](auto binding) {
                      ogl::buffer_data(
                          binding,
                          std::span { indices.data(), indices.size() },
                          GL_STATIC_DRAW);
                  });
    });

    ogl::bind(ogl::texture_2d_target, input_texture, [&](auto binding) {
        auto const pixel_data = read_pixels(pixel_file_path);

        EXPECT(pixel_data.size() == input_image.width * input_image.height *
                                        sizeof(unsigned char) * 3);

        ogl::texture_image_2d(binding,
                              0,
                              GL_RGB,
                              input_image.width,
                              input_image.height,
                              0,
                              GL_RGB,
                              GL_UNSIGNED_BYTE,
                              pixel_data.data());

        ogl::texture_parameter(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ogl::texture_parameter(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ogl::texture_parameter(binding, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ogl::texture_parameter(binding, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    });

    ogl::bind(ogl::texture_2d_target, output_texture, [&](auto binding) {
        ogl::texture_image_2d(binding,
                              0,
                              GL_RGBA,
                              dimensions.width,
                              dimensions.height,
                              0,
                              GL_BGRA,
                              GL_UNSIGNED_BYTE,
                              0);

        ogl::texture_parameter(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ogl::texture_parameter(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    });

    auto vertex_shader = ogl::create_shader(ogl::ShaderType::vertex);
    ogl::shader_source(vertex_shader, vertex_shader_source);
    ogl::compile_shader(vertex_shader);

    auto fragment_shader = ogl::create_shader(ogl::ShaderType::fragment);
    ogl::shader_source(fragment_shader, fragment_shader_source);
    ogl::compile_shader(fragment_shader);

    auto shader_program = ogl::create<ogl::Program>();
    ogl::attach_shader(shader_program, vertex_shader);
    ogl::attach_shader(shader_program, fragment_shader);

    ogl::link_program(shader_program);
    ogl::validate_program(shader_program);

    ogl::bind(ogl::draw_framebuffer_target, fbo, [&](auto binding) {
        ogl::viewport(0, 0, dimensions.width, dimensions.height);
        ogl::framebuffer_texture(
            binding, GL_COLOR_ATTACHMENT0, output_texture, 0);

        ogl::draw_buffers(binding, GL_COLOR_ATTACHMENT0);

        ogl::check_framebuffer_status(binding);
    });

    ogl::bind(ogl::vertex_array_target, vao, [&](auto binding) {
        /* TODO:
         *  It would be nice to be able to call `bind(...)` with
         *  multiple targets so we don't have to do this...
         */
        auto fbo_binding = ogl::bind(ogl::draw_framebuffer_target, fbo);
        auto texture_binding = ogl::bind(ogl::texture_2d_target, input_texture);
        auto element_buffer_binding =
            ogl::bind(ogl::element_array_buffer_target, element_buffer);
        auto program_in_use = ogl::bind(ogl::program_target, shader_program);

        ogl::uniform(program_in_use,
                     "dimensions",
                     static_cast<GLfloat>(dimensions.width * 1.f),
                     static_cast<GLfloat>(dimensions.height * 1.f));

        ogl::clear_color(0.f, 0.f, 0.f, 1.f);
        ogl::clear(GL_COLOR_BUFFER_BIT);
        ogl::draw_elements(binding,
                           element_buffer_binding,
                           program_in_use,
                           GL_TRIANGLES,
                           6,
                           GL_UNSIGNED_INT,
                           nullptr);
    });

    std::vector<unsigned char> pixels(dimensions.width * dimensions.height * 4,
                                      0);

    ogl::bind(ogl::texture_2d_target, output_texture, [&](auto binding) {
        ogl::get_texture_image(
            binding,
            0,
            GL_BGRA,
            std::span<unsigned char> { pixels.data(), pixels.size() });
    });

    write_to_file("/tmp/render-to-texture",
                  dimensions,
                  true,
                  pixels.begin(),
                  pixels.end());
}

auto main() -> int
{
    EXPECT(::getenv("SHADOW_CAST_TEST_PIXEL_DATA_FILE") != nullptr);
    sc::wayland::DisplayPtr wayland_display { wl_display_connect(nullptr) };
    EXPECT(wayland_display);
    auto wayland = sc::initialize_wayland(std::move(wayland_display));
    sc::initialize_wayland_egl(sc::egl(), *wayland);
    try {
        render_triange_to_texture();
        render_textured_quad_to_texture(
            ::getenv("SHADOW_CAST_TEST_PIXEL_DATA_FILE"));
        return 0;
    }
    catch (std::exception const& e) {
        std::cerr << "Failed: " << e.what() << '\n';
    }
    return 1;
}
