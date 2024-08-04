#version 330 core
#extension GL_OES_EGL_image_external : enable

out vec4 FragColor;
in vec2 tex_coord;
uniform samplerExternalOES texture_sampler;

void main()
{
    FragColor = texture2D(texture_sampler, tex_coord);
}

// vim: ft=glsl
