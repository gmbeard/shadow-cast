#version 330 core
#extension GL_OES_EGL_image_external : enable

out vec4 FragColor;
in vec2 tex_coord;
uniform vec2 dimensions;
uniform samplerExternalOES texture_sampler;

void main()
{
    FragColor = vec4(texture2D(texture_sampler, tex_coord).rgb, 1.0);
}

// vim: ft=glsl
