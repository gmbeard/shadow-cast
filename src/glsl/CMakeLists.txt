include(EmbeddedGLSLTarget)

add_embedded_glsl_target(
    NAME embedded_glsl
    SOURCES
        default_vertex.glsl
        default_fragment.glsl
        mouse_vertex.glsl
        mouse_fragment.glsl
)
