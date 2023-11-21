find_package(PkgConfig REQUIRED)
include(FindPackageHandleStandardArgs)

pkg_check_modules(PC_DRM QUIET "libdrm")

find_path(DRM_INCLUDE_DIR
    NAMES drm.h
    PATHS "${PC_DRM_INCLUDE_DIRS}"
    PATH_SUFFIXES "libdrm"
)

find_library(DRM_LIBRARY
    NAMES drm
    PATHS "${PC_DRM_LIBRARY_DIRS}"
)

find_package_handle_standard_args(DRM
    FOUND_VAR DRM_FOUND
    REQUIRED_VARS
        DRM_LIBRARY
        DRM_INCLUDE_DIR
)

if (DRM_FOUND)
    # Include the found directory. Needed for including
    # `xf86drm.h`...
    list(APPEND DRM_INCLUDE_DIRS "${DRM_INCLUDE_DIR}")

    # Set the include dir up one level so that dependent
    # targets have to use `#include <libdrm/...>`. This
    # prevents any clashing header names...
    if (DRM_INCLUDE_DIR MATCHES "\/libdrm$")
        get_filename_component(
            DRM_INCLUDE_DIR
            "${DRM_INCLUDE_DIR}"
            DIRECTORY
        )
        list(APPEND DRM_INCLUDE_DIRS "${DRM_INCLUDE_DIR}")
    endif ()
    set(DRM_LIBRARIES ${DRM_LIBRARY})
    set(DRM_DEFINITIONS ${PC_DRM_CFLAGS_OTHER})
endif ()

if (DRM_FOUND)
    message(STATUS "DRM_INCLUDE_DIRS: ${DRM_INCLUDE_DIRS}")
    message(STATUS "DRM_LIBRARY: ${DRM_INCLUDE_DIR}")
    add_library(DRM::drm UNKNOWN IMPORTED)
    set_target_properties(
        DRM::drm
        PROPERTIES
            IMPORTED_LOCATION "${DRM_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_DRM_CGLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${DRM_INCLUDE_DIRS}"
    )
endif ()
