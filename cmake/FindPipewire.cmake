find_package(PkgConfig REQUIRED)
pkg_check_modules(
    PC_Pipewire
    QUIET
    pipewire
)

find_path(
    Pipewire_INCLUDE_DIR
    NAMES pipewire.h
    PATHS ${PC_Pipewire_INCLUDE_DIRS}
    PATH_SUFFIXES pipewire-0.3/pipewire
)

find_path(
    SPA_INCLUDE_DIR
    NAME plugin.h
    PATHS ${PC_Pipewire_INCLUDE_DIRS}
    PATH_SUFFIXES spa-0.2/spa/support
)

find_library(
    Pipewire_LIBRARY
    NAMES pipewire-0.3
    PATHS ${PC_Pipewire_LIBRARY_DIRS}
)

if (Pipewire_INCLUDE_DIR AND Pipewire_INCLUDE_DIR MATCHES "pipewire$")
    get_filename_component(Pipewire_INCLUDE_DIR "${Pipewire_INCLUDE_DIR}" DIRECTORY)
endif()

if (SPA_INCLUDE_DIR)
    get_filename_component(SPA_INCLUDE_DIR "${SPA_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(SPA_INCLUDE_DIR "${SPA_INCLUDE_DIR}" DIRECTORY)
endif()

message(STATUS "Pipewire_INCLUDE_DIR: ${Pipewire_INCLUDE_DIR}")
message(STATUS "Pipewire_LIBRARY: ${Pipewire_LIBRARY}")
message(STATUS "SPA_INCLUDE_DIR: ${SPA_INCLUDE_DIR}")

if (Pipewire_INCLUDE_DIR AND SPA_INCLUDE_DIR AND Pipewire_LIBRARY)
    add_library(Pipewire::pipewire UNKNOWN IMPORTED)
    set_target_properties(
        Pipewire::pipewire
        PROPERTIES
            IMPORTED_LOCATION "${Pipewire_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_Pipewire_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Pipewire_INCLUDE_DIR};${SPA_INCLUDE_DIR}"
    )
else()
    if (Pipewire_FIND_REQUIRED)
        message(FATAL_ERROR "Couldn't find required package: Pipewire")
    endif()
endif()
