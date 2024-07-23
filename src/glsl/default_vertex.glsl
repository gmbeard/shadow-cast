#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 tex_coord;

void main()
{
    gl_Position = vec4(aPos.xyz, 1.0);
    tex_coord = aTexCoord;
}

// vim: ft=glsl
