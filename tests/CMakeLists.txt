include(MakeTest)
include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(testing OBJECT testing.cpp)

# NOTE:
#  Use -DSHADOW_CAST_ENABLE_TEST_CATEGORIES="val1;val2" to control the
#  ENABLE_IF flags

add_subdirectory(glsl)

make_test(NAME base64_tests SOURCES base64_tests.cpp)
make_test(NAME intrusive_list_tests SOURCES intrusive_list_tests.cpp)
make_test(NAME pool_tests SOURCES pool_tests.cpp)
make_test(NAME cmd_line_tests SOURCES cmd_line_tests.cpp)
make_test(
    NAME gl_shader_tests
    SOURCES gl_shader_tests.cpp
    ENABLE_IF wayland all
    LABELS wayland)
make_test(
    NAME gl_buffer_tests
    SOURCES gl_buffer_tests.cpp
    ENABLE_IF wayland all
    LABELS wayland)
make_test(NAME gl_error_tests SOURCES gl_error_tests.cpp)
make_test(
    NAME gl_texture_tests
    SOURCES gl_texture_tests.cpp
    ENABLE_IF wayland all
    LABELS wayland)
make_test(NAME histogram_tests SOURCES histogram_tests.cpp)

add_custom_target(
    pixel_data
    COMMAND cmake
        -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/data/cyberpunk-girl-rgb-640x640.rgb"
        "${CMAKE_CURRENT_BINARY_DIR}/data/cyberpunk-girl-rgb-640x640.rgb"
)

make_test(
    NAME gl_render_to_texture_test
    SOURCES gl_render_to_texture_test.cpp
    ENV_VARS
        "SHADOW_CAST_TEST_PIXEL_DATA_FILE=${CMAKE_CURRENT_BINARY_DIR}/data/cyberpunk-girl-rgb-640x640.rgb"
    ENABLE_IF wayland all
    LABELS wayland
    LINK_LIBRARIES glsl_sources)

add_dependencies(gl_render_to_texture_test pixel_data)

if(EXISTS ${PROJECT_SOURCE_DIR}/.private/nvfbc.key)
    file(STRINGS ${PROJECT_SOURCE_DIR}/.private/nvfbc.key SHADOW_CAST_NVFBC_KEY)
endif()

if(SHADOW_CAST_NVFBC_KEY)
    make_test(
        NAME nvidia_tests
        SOURCES nvidia_tests.cpp
        ENV_VARS
            "SHADOW_CAST_NVFBC_KEY=${SHADOW_CAST_NVFBC_KEY}"
            # ASAN needs the following runtime setting to prevent OOM when invoking `cuInit`
            # (ref: https://github.com/google/sanitizers/issues/629#issuecomment-161357276)
            "ASAN_OPTIONS=protect_shadow_gap=0"
        ENABLE_IF nvfbc all
        LABELS nvidia
    )
else()
    message(WARNING "Couldn't load nvfbc.key. Ignoring nvidia tests")
endif()

