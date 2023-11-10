cmake_minimum_required(VERSION 3.27)

file(READ ".versioning/current" current_version)
string(STRIP "${current_version}" current_version)

project(
    shadow-cast
    LANGUAGES CXX
    VERSION ${current_version}
)

if (CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "Defaulting CMAKE_BUILD_TYPE to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(CPPCheck)
include(GNUInstallDirs)

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
find_package(X11 REQUIRED)

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

option(
    SHADOW_CAST_ENABLE_TSAN
    "Enable TSan for ${PROJECT_NAME}"
    OFF
)

if (SHADOW_CAST_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

if (SHADOW_CAST_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

add_subdirectory(src)

if(SHADOW_CAST_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(
    TARGETS shadow-cast
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
