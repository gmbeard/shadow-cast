#include "gl/object.hpp"
#include "gl/program.hpp"
#include "gl/shader.hpp"
#include "platform/egl.hpp"
#include "platform/wayland.hpp"
#include "testing.hpp"
#include <string_view>

constexpr std::string_view vert_src = R"~(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec4 vertexColor;
uniform vec3 light_direction;
void main()
{
    gl_Position = vec4(aPos, 1.0);
    vertexColor = vec4(0.5, 0.0, 0.0, 1.0) *
        vec4(light_direction.xyz, 0.0);
})~";

constexpr std::string_view frag_src = R"~(#version 330 core
out vec4 FragColor;
in vec4 vertexColor;
void main()
{
    FragColor = vertexColor;
})~";

namespace ogl = sc::opengl;

auto should_create_shader() -> void
{
    ogl::Shader shader_a = ogl::create_shader(ogl::ShaderType::fragment);
    ogl::Shader shader_b = ogl::create_shader(ogl::ShaderType::fragment);
    EXPECT(shader_a.name());
    EXPECT(shader_b.name());
}

auto should_compile_simple_shader() -> void
{
    ogl::Shader shader = ogl::create_shader(ogl::ShaderType::fragment);
    shader_source(shader, frag_src);
    compile_shader(shader);
}

auto should_create_program() -> void
{
    auto prog_ = ogl::create<ogl::Program>();
    prog_.num_shaders_attached = 42;

    ogl::Program prog { std::move(prog_) };
    EXPECT(prog.name());
    EXPECT(prog.num_shaders_attached == 42);
}

auto should_link_program() -> void
{
    auto vert = ogl::create_shader(ogl::ShaderType::vertex);
    auto frag = ogl::create_shader(ogl::ShaderType::fragment);

    shader_source(vert, vert_src);
    shader_source(frag, frag_src);

    auto prog = ogl::create<ogl::Program>();
    EXPECT(prog.name());

    sc::opengl::attach_shader(prog, vert);
    sc::opengl::attach_shader(prog, frag);
    sc::opengl::link_program(prog);
    sc::opengl::validate_program(prog);
}

auto should_get_uniform_location() -> void
{
    auto vert = ogl::create_shader(ogl::ShaderType::vertex);
    auto frag = ogl::create_shader(ogl::ShaderType::fragment);

    shader_source(vert, vert_src);
    shader_source(frag, frag_src);

    auto prog = ogl::create<ogl::Program>();
    EXPECT(prog.name());

    sc::opengl::attach_shader(prog, vert);
    sc::opengl::attach_shader(prog, frag);
    sc::opengl::link_program(prog);
    sc::opengl::validate_program(prog);

    EXPECT(sc::opengl::get_uniform_location(prog, "light_direction") >= 0);
}

auto main() -> int
{
    sc::wayland::DisplayPtr wayland_display { wl_display_connect(nullptr) };
    EXPECT(wayland_display);
    auto wayland = sc::initialize_wayland(std::move(wayland_display));
    sc::initialize_wayland_egl(sc::egl(), *wayland);
    return testing::run({ TEST(should_create_shader),
                          TEST(should_compile_simple_shader),
                          TEST(should_link_program),
                          TEST(should_get_uniform_location),
                          TEST(should_create_program) });
}
