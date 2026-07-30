include(FetchContent)
#set(SHADOW_CAST_EXIOS_VERSION "0.6.0")
set(SHADOW_CAST_EXIOS_VERSION "12cf57ab85dd0fe18ab37d301aed582baf8cb640")

if(SHADOW_CAST_USE_LOCAL_DEPENDENCIES)
    FetchContent_Declare(
        Exios
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/deps/exios"
        OVERRIDE_FIND_PACKAGE
        EXCLUDE_FROM_ALL
    )
else()
    FetchContent_Declare(
        Exios
        GIT_REPOSITORY https://github.com/gmbeard/exios.git
        GIT_TAG ${SHADOW_CAST_EXIOS_VERSION}
        OVERRIDE_FIND_PACKAGE
        EXCLUDE_FROM_ALL
    )
endif()
