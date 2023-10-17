include(MakeTest)
include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(testing OBJECT testing.cpp)

make_test(NAME base64_tests SOURCES base64_tests.cpp SIMPLE)

if(EXISTS ${PROJECT_SOURCE_DIR}/.private/nvfbc.key)
    file(STRINGS ${PROJECT_SOURCE_DIR}/.private/nvfbc.key SHADOW_CAST_NVFBC_KEY)
endif()

if(SHADOW_CAST_NVFBC_KEY)
    make_test(
        NAME nvidia_tests
        SOURCES nvidia_tests.cpp
        FULL
        ENV_VARS
            "SHADOW_CAST_NVFBC_KEY=${SHADOW_CAST_NVFBC_KEY}"
            # ASAN needs the following runtime setting to prevent OOM when invoking `cuInit`
            # (ref: https://github.com/google/sanitizers/issues/629#issuecomment-161357276)
            "ASAN_OPTIONS=protect_shadow_gap=0"
        ENABLE_MATCH SHADOW_CAST_TEST_TYPE
    )
else()
    message(WARNING "Couldn't load nvfbc.key. Ignoring nvidia tests")
endif()

