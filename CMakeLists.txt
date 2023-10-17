cmake_minimum_required(VERSION 3.27)

file(READ ".versioning/current" current_version)
string(STRIP "${current_version}" current_version)

project(
    shadow-cast
    LANGUAGES CXX
    VERSION ${current_version}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(CPPCheck)

string(TOLOWER ${CMAKE_CXX_BYTE_ORDER} SHADOW_CAST_BYTE_ORDER)

add_compile_options(-Wall -Werror -Wextra -Wshadow)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0)
endif()

find_package(Pipewire REQUIRED)
find_package(
    FFMpeg REQUIRED
    COMPONENTS
        avcodec
        avdevice
        avfilter
        avformat
        avresample
        avutil
        swresample
        swscale
)

option(
    SHADOW_CAST_ENABLE_TESTS
    "Enable tests for ${PROJECT_NAME}"
    OFF
)

option(
	SHADOW_CAST_ENABLE_ASAN
    "Enable ASan for ${PROJECT_NAME}"
    OFF
)

if (SHADOW_CAST_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(src)

if(SHADOW_CAST_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
