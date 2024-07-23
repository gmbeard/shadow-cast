#version 330 core
out vec4 FragColor;

in vec2 tex_coord;

uniform vec2 dimensions;
uniform sampler2D texture_sampler;

const float brightness_scale = 1.5;
const float border_size = 5.0;
const float border_radius = 100.0;

float get_border_min(float pos)
{
    return 1.0 * smoothstep(border_size, border_size + border_radius, pos);
}

float get_border_max(float pos)
{
    return 1.0 * smoothstep(pos, pos + border_radius, dimensions.x - border_size);
}

void main()
{
    float border_left_color = get_border_min(gl_FragCoord.x);
    float border_right_color = get_border_max(gl_FragCoord.x);
    float border_top_color = get_border_min(gl_FragCoord.y);
    float border_bottom_color = get_border_max(gl_FragCoord.y);

    vec3 color =
        texture(texture_sampler, tex_coord).rgb
        * clamp(border_left_color
                * border_right_color
                * border_top_color
                * border_bottom_color, 0.0, 1.0);
    vec3 cell_color = step(0.5, texture(texture_sampler, tex_coord).rgb);
    FragColor = vec4(mix(color, cell_color, 0.25), 1.0) * brightness_scale;
}

// vim: ft=glsl
