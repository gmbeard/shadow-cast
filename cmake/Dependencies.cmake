include(FetchContent)
set(SHADOW_CAST_EXIOS_VERSION "0.2.0")

if(SHADOW_CAST_USE_LOCAL_DEPENDENCIES)
    FetchContent_Declare(
        Exios
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/deps/exios-${SHADOW_CAST_EXIOS_VERSION}"
        OVERRIDE_FIND_PACKAGE
    )
else()
    FetchContent_Declare(
        Exios
        GIT_REPOSITORY https://github.com/gmbeard/exios.git
        GIT_TAG ${SHADOW_CAST_EXIOS_VERSION}
        OVERRIDE_FIND_PACKAGE
    )
endif()
