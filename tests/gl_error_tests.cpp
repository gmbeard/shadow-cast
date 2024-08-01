#include "gl/error.hpp"
#include "testing.hpp"
#include <GL/gl.h>
#include <string_view>

auto should_yield_correct_error_string() -> void
{
    EXPECT(std::string_view { sc::gl_error_to_string(GL_INVALID_ENUM) } ==
           "GL_INVALID_ENUM");
}

auto main() -> int
{
    return testing::run({ TEST(should_yield_correct_error_string) });
}
