#ifndef SHADOW_CAST_SERVICES_COLOR_CONVERTER_HPP_INCLUDED
#define SHADOW_CAST_SERVICES_COLOR_CONVERTER_HPP_INCLUDED

#include "gl/buffer.hpp"
#include "gl/framebuffer.hpp"
#include "gl/program.hpp"
#include "gl/texture.hpp"
#include "gl/vertex_array_object.hpp"
#include <cstdint>

namespace sc
{

struct ColorConverter
{
    ColorConverter(std::uint32_t output_width,
                   std::uint32_t output_height) noexcept;

    auto initialize() -> void;
    [[nodiscard]] auto input_texture() noexcept -> opengl::Texture&;
    [[nodiscard]] auto output_texture() noexcept -> opengl::Texture&;
    auto convert() -> void;

private:
    opengl::Framebuffer fbo_;
    opengl::Texture input_texture_;
    opengl::Texture output_texture_;
    opengl::VertexArray vao_;
    opengl::Buffer vertex_buffer_;
    opengl::Buffer texture_coords_buffer_;
    opengl::Buffer index_buffer_;
    opengl::Program program_;
    std::uint32_t output_width_;
    std::uint32_t output_height_;
    bool initialized_ { false };
};

} // namespace sc

#endif // SHADOW_CAST_SERVICES_COLOR_CONVERTER_HPP_INCLUDED
