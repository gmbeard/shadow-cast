#include "gl/object.hpp"
#include "gl/texture.hpp"
#include "platform/egl.hpp"
#include "platform/wayland.hpp"
#include "testing.hpp"
#include <GL/gl.h>
#include <GL/glext.h>

namespace ogl = sc::opengl;

auto should_create_texture() -> void
{
    auto tex = sc::opengl::create<sc::opengl::Texture>();
    EXPECT(tex.name() != 0);
}

auto should_move_texture() -> void
{
    auto tex1 = ogl::create<ogl::Texture>();
    auto const id = tex1.name();

    ogl::Texture tex2 { std::move(tex1) };
    EXPECT(tex2.name() == id);
    EXPECT(tex1.name() == 0);
}

auto should_bind_texture() -> void
{
    auto tex = ogl::create<ogl::Texture>();
    auto binding = ogl::bind(ogl::texture_2d_target, tex);
    EXPECT(binding.is_bound());
    EXPECT(binding.name() == tex.name());
}

auto should_initialize_texture_data() -> void
{
    auto tex = ogl::create<ogl::Texture>();
    ogl::bind(ogl::texture_2d_target, tex, [](auto binding) {
        ogl::texture_image_2d(binding,
                              0,
                              GL_RGBA,
                              256,
                              256,
                              0,
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              nullptr);
        ogl::texture_parameter(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ogl::texture_parameter(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        std::array<GLuint, 4> swizzle { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA };
        ogl::texture_parameter(binding,
                               GL_TEXTURE_SWIZZLE_RGBA,
                               std::span { swizzle.data(), swizzle.size() });
    });
}

auto main() -> int
{
    sc::wayland::DisplayPtr wayland_display { wl_display_connect(nullptr) };
    EXPECT(wayland_display);
    auto wayland = sc::initialize_wayland(std::move(wayland_display));
    sc::initialize_wayland_egl(sc::egl(), *wayland);
    return testing::run({ TEST(should_create_texture),
                          TEST(should_initialize_texture_data),
                          TEST(should_bind_texture),
                          TEST(should_move_texture) });
}
