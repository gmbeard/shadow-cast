# This will create an object library target called `${NAME}.o`
# which will embed and export the GLSL as a array of bytes.
# The symbol name given to each export GLSL file is derived
# from the entry in `SOURCES`...
#
# ```
# extern char const _private_<SOURCE_FILE_WITH_UNDERSCORES>_start[];
# extern char const _private_<SOURCE_FILE_WITH_UNDERSCORES>_end[];
# ```
# 
# The source file (combined with any path components) have all `.`
# `-`, `/`, etc replaced with underscores (`_`)
#
# To keep the exported symbol names short, you should restrict
# `SOURCES` to files in the current directory. Any relative
# directories will also form part of the symbol name. E.g...
#
# ```
# add_embedded_glsl_target(
#   SOURCES glsl/vertex-shaders/shader.glsl
#   ...
# )
# ```
#
# would yield the following symbol names...
#
# ```
# extern char const _private_glsl_vertex_shaders_shader_glsl_start[];
# extern char const _private_glsl_vertex_shaders_shader_glsl_end[];
# ```
function(add_embedded_glsl_target)
    set(single_value_args NAME)
    set(multi_value_args SOURCES)
    cmake_parse_arguments(
        ADD_EMBEDDED_GLSL_TARGET
        "${options}"
        "${single_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    # We may be able to use the `-L` argument when generating an
    # object file, which could remove the path element from the
    # symbol names (`-L` is a search path)...
    add_custom_target(
        ${ADD_EMBEDDED_GLSL_TARGET_NAME}_generator
        COMMAND ${CMAKE_LINKER} -b binary -r
            -o "${CMAKE_CURRENT_BINARY_DIR}/${ADD_EMBEDDED_GLSL_TARGET_NAME}.o"
            ${ADD_EMBEDDED_GLSL_TARGET_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        # BYPRODUCTS is needed by Ninja...
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${ADD_EMBEDDED_GLSL_TARGET_NAME}.o"
    )
    add_library(${ADD_EMBEDDED_GLSL_TARGET_NAME} OBJECT IMPORTED GLOBAL)
    set_target_properties(
        ${ADD_EMBEDDED_GLSL_TARGET_NAME}
        PROPERTIES
            IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/${ADD_EMBEDDED_GLSL_TARGET_NAME}.o"
    )
    add_dependencies(${ADD_EMBEDDED_GLSL_TARGET_NAME} ${ADD_EMBEDDED_GLSL_TARGET_NAME}_generator)
endfunction()
