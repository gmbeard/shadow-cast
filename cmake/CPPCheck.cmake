find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if(CMAKE_CXX_CPPCHECK AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND CMAKE_CXX_CPPCHECK
        "--inline-suppr"
        "--enable=warning"
        "--error-exitcode=1"
        "--cppcheck-build-dir=${PROJECT_BINARY_DIR}"
    )
else()
    set(CMAKE_CXX_CPPCHECK "")
endif()
