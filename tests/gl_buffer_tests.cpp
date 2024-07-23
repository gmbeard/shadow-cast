#include "gl/buffer.hpp"
#include "gl/object.hpp"
#include "gl/vertex_array_object.hpp"
#include "platform/egl.hpp"
#include "platform/wayland.hpp"
#include "testing.hpp"
#include "utils/scope_guard.hpp"
#include <array>
#include <iostream>

namespace ogl = sc::opengl;

auto should_bind_vao() -> void
{
    auto vao = ogl::create<ogl::VertexArray>();

    ogl::bind(ogl::vertex_array_target, vao, [](auto binding) {
        ogl::enable_vertex_array_attrib(binding, 1);
    });
}

auto should_bind_and_unbind_buffer() -> void
{
    auto buffer = ogl::create<ogl::Buffer>();
    auto binding = ogl::bind(ogl::array_buffer_target, buffer);
}

auto should_buffer_data() -> void
{
    auto vao = ogl::create<ogl::VertexArray>();
    auto buffer = ogl::create<ogl::Buffer>();
    ogl::bind(ogl::vertex_array_target, vao, [&](auto vao_binding) {
        std::array<float, 4> const data = { 1.f, 2.f, 3.f, 4.f };

        auto binding = ogl::bind(ogl::array_buffer_target, buffer);
        ogl::buffer_data(
            binding, std::span { data.data(), data.size() }, GL_STATIC_DRAW);

        ogl::vertex_attrib_pointer(binding, 0, 1, GL_FLOAT, false, 0, 0);
        ogl::enable_vertex_array_attrib(vao_binding, 0);
    });
}

auto main() -> int
{
    sc::wayland::DisplayPtr wayland_display { wl_display_connect(nullptr) };
    EXPECT(wayland_display);
    auto wayland = sc::initialize_wayland(std::move(wayland_display));
    sc::initialize_wayland_egl(sc::egl(), *wayland);
    return testing::run({ TEST(should_bind_vao),
                          TEST(should_bind_and_unbind_buffer),
                          TEST(should_buffer_data) });
}
