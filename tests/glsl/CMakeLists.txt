include(EmbeddedGLSLTarget)

add_embedded_glsl_target(
    NAME glsl_sources
    SOURCES
        flipped_y_vertex.glsl
        identity_fragment.glsl
        textured_vertex.glsl
        textured_brightness_fragment.glsl
)
