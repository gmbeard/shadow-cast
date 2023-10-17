cmake_minimum_required(VERSION 3.27)
project(
    shadow-capture
    LANGUAGES CXX
    VERSION 0.1.0
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(CPPCheck)

add_compile_options(-Wall -Werror -Wextra -Wshadow)

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
    SHADOW_CAPTURE_ENABLE_TESTS
    "Enable tests for ${PROJECT_NAME}"
    OFF
)

option(
    SHADOW_CAPTURE_USE_ASAN
    "Enable ASan for ${PROJECT_NAME}"
    OFF
)

if (SHADOW_CAPTURE_USE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(src)
