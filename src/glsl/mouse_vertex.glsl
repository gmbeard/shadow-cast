#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 screen_dimensions;
uniform vec2 mouse_dimensions;
uniform vec2 mouse_position;

out vec2 tex_coord;

void main()
{
    /* NOTE:
     *  We must scale all the mouse plane's dimensions and screen position
     * to -1.0,1.0 coords...
     * - First scale the mouse plane to the screen scale.
     * - Then offset the mouse position by _adding_ half its w/h,
     *   and _subtracting_ half the screen w/h.
     * - Finally translate its x/y position by dividing by half screen
     *   w/h.
     */

    /* NOTE:
     *  We don't have to worry about inverting the y coords because the
     * texture will render upside down (which is correct when we finally
     * copy it to the encoder stage).
     */

    vec2 scaled_dimensions = mouse_dimensions / screen_dimensions;
    mat4 scale;
    scale[0] = vec4(scaled_dimensions.x, 0.0, 0.0, 0.0);
    scale[1] = vec4(0.0, scaled_dimensions.y, 0.0, 0.0);
    scale[2] = vec4(0.0, 0.0, 1.0, 0.0);
    scale[3] = vec4(0.0, 0.0, 0.0, 1.0);

    vec2 half_screen = screen_dimensions / 2;

    vec2 mpos = (mouse_position + mouse_dimensions / 2) - half_screen;

    mat4 translate;
    translate[0] = vec4(1.0, 0.0, 0.0, 0.0);
    translate[1] = vec4(0.0, 1.0, 0.0, 0.0);
    translate[2] = vec4(0.0, 0.0, 1.0, 0.0);
    translate[3] = vec4(mpos / half_screen, 0.0, 1.0);

    mat4 mv = translate * scale;

    gl_Position = mv * vec4(aPos, 1.0);
    tex_coord = aTexCoord;
}

// vim: ft=glsl
