find_package(PkgConfig REQUIRED)
include(FindPackageHandleStandardArgs)

function(try_find_ffmpeg_targets)
    cmake_parse_arguments(
        EXPORT_FFMPEG
        ""
        ""
        "NAMES"
        ${ARGN}
    )

    foreach(FFMPEG_COMPONENT ${EXPORT_FFMPEG_NAMES})
        pkg_check_modules(PC_${FFMPEG_COMPONENT}_CODEC QUIET "lib${FFMPEG_COMPONENT}")

        find_path(FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR
            NAMES ${FFMPEG_COMPONENT}.h
            PATHS "${PC_${FFMPEG_COMPONENT}_INCLUDE_DIRS}"
            PATH_SUFFIXES "lib${FFMPEG_COMPONENT}"
        )

        # libavutil contains a `time.h` file which will undoubtedly
        # clash horribly with standard library `time.h`. So we have
        # to remove the last path component and include these files
        # will then need to be referenced using their path suffix,
        # E.g...
        #
        #   #include <libavutil/time.h>
        #
        if (FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR
                AND FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR MATCHES "\/lib${FFMPEG_COMPONENT}$")
            get_filename_component(
                FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR
                "${FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR}"
                DIRECTORY
            )
        endif()

        find_library(FFMpeg_${FFMPEG_COMPONENT}_LIBRARY
            NAMES ${FFMPEG_COMPONENT}
            PATHS "${PC_${FFMPEG_COMPONENT}_LIBRARY_DIRS}"
        )

        list(APPEND FFMPEG_COMPONENT_VARS "FFMpeg_${FFMPEG_COMPONENT}_INCLUDE_DIR")
        list(APPEND FFMPEG_COMPONENT_VARS "FFMpeg_${FFMPEG_COMPONENT}_LIBRARY")

    endforeach()

    message(STATUS "Components: ${FFMpeg_FIND_COMPONENTS}")

    foreach(COMP ${FFMpeg_FIND_COMPONENTS})
        if (FFMpeg_${COMP}_LIBRARY AND FFMpeg_${COMP}_INCLUDE_DIR)
            message(STATUS "FFMpeg_${COMP}: FOUND")

            add_library(FFMpeg::${COMP} UNKNOWN IMPORTED)
            set_target_properties(FFMpeg::${COMP}
                PROPERTIES
                    IMPORTED_LOCATION "${FFMpeg_${COMP}_LIBRARY}"
                    INTERFACE_COMPILE_OPTIONS "${PC_${COMP}_CFLAGS_OTHER}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FFMpeg_${COMP}_INCLUDE_DIR}"
            )
            set(FFMpeg_${COMP}_FOUND TRUE)
        else()
            set(FFMpeg_${COMP}_FOUND "NOTFOUND")
        endif()

        if(NOT FFMpeg_${COMP}_FOUND AND FFMpeg_FIND_REQUIRED_${COMP})
            set(FFMpeg_FOUND "NOTFOUND" PARENT_SCOPE)
            break()
        else()
            set(FFMpeg_FOUND TRUE PARENT_SCOPE)
        endif()

    endforeach()

endfunction(try_find_ffmpeg_targets)

if (NOT FFMpeg_FIND_COMPONENTS)
    message(FATAL_ERROR "FindFFMpeg: No components were specified")
endif()

try_find_ffmpeg_targets(
    NAMES
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
)

if (FFMpeg_FIND_REQUIRED AND NOT FFMpeg_FOUND)
    message(FATAL_ERROR "FFMpeg: Not all required components could be found")
endif()

set(FFMpeg_FOUND TRUE)
